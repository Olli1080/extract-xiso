// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/image_reader.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include "xiso/paths.hpp"

namespace xiso
{

namespace
{

bool matches_magic(std::span<const std::byte> bytes)
{
	return std::ranges::equal(
		bytes, std::as_bytes(std::span(format::header_magic)), [](std::byte a, std::byte b) { return a == b; });
}

} // namespace

ImageReader::ImageReader(const std::filesystem::path& path) : path_(path), file_(path)
{
	std::array<std::byte, format::header_magic_size> magic{};

	// The volume descriptor sits at 0x10000 in a plain image, or further in for full-disc dumps.
	bool found = false;
	for (const std::uint64_t offset : format::partition_offsets)
	{
		file_.seek(format::header_offset + offset);
		if (file_.try_read_exact(magic) && matches_magic(magic))
		{
			partition_offset_ = offset;
			found = true;
			break;
		}
	}
	if (!found) throw Error(std::format("{} does not appear to be a valid xbox iso image", to_utf8(path_.filename())));

	std::array<std::byte, 8> root{};
	file_.read_exact(root);
	volume_.root_sector = load_le<std::uint32_t>(std::span<const std::byte>(root).first<4>());
	volume_.root_size = load_le<std::uint32_t>(std::span<const std::byte>(root).last<4>());

	// The descriptor ends with the magic once more.
	file_.seek(format::header_offset + partition_offset_ + format::header_magic_size + root.size() +
			   format::filetime_size + format::header_unused_size);
	if (!file_.try_read_exact(magic) || !matches_magic(magic))
		throw Error(std::format("{} appears to be corrupt", to_utf8(path_.filename())));
}

FileTime ImageReader::read_timestamp()
{
	FileTime time{};
	file_.seek(format::header_offset + partition_offset_ + format::header_magic_size + 8);
	file_.read_exact(time);
	return time;
}

DirRecord ImageReader::read_record(std::uint16_t left)
{
	std::array<std::byte, format::entry_header_size - 2> fixed{};
	file_.read_exact(fixed);

	DirRecord record;
	record.left = left;
	record.right = load_le<std::uint16_t>(std::span<const std::byte>(fixed).subspan<0, 2>());
	record.start_sector = load_le<std::uint32_t>(std::span<const std::byte>(fixed).subspan<2, 4>());
	record.size = load_le<std::uint32_t>(std::span<const std::byte>(fixed).subspan<6, 4>());
	record.attributes = static_cast<std::uint8_t>(fixed[10]);
	const auto name_length = static_cast<std::size_t>(fixed[11]);

	std::array<std::byte, format::max_filename_length> name{};
	file_.read_exact(std::span(name).first(name_length));
	record.name.assign(reinterpret_cast<const char*>(name.data()), name_length);
	// Names are C strings on disk: whatever follows a NUL is not part of the name.
	record.name.resize(std::char_traits<char>::length(record.name.c_str()));

	validate_filename(record.name);
	return record;
}

bool is_optimized(const std::filesystem::path& path)
{
	InputFile file(path);
	file.seek(format::optimized_tag_offset);

	std::array<std::byte, format::optimized_tag_size> tag{};
	if (file.read_some(tag) < format::optimized_tag_compare_size) return false;

	return std::ranges::equal(
		std::span(tag).first(format::optimized_tag_compare_size),
		std::as_bytes(std::span(format::optimized_tag_prefix)).first(format::optimized_tag_compare_size));
}

void validate_filename(const std::string& name)
{
	// Names come straight from the (untrusted) image and become file names on extraction. Beyond "." and ".."
	// the original check ("...", etc. are fine) also has to reject anything that could leave the target directory.
	bool suspicious = name.empty() || name == "." || name == ".." || name.find_first_of("/\\") != std::string::npos;
	if (!suspicious)
	{
		const std::filesystem::path as_path = from_utf8(name);
		suspicious = as_path.has_root_path() || as_path.has_parent_path() || as_path.filename() != as_path;
	}
	if (suspicious) throw Error(std::format("filename '{}' contains invalid character(s), aborting.", name));
}

} // namespace xiso
