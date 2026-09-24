// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/image_writer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <string>
#include <system_error>
#include <vector>

#include "xiso/endian.hpp"
#include "xiso/format.hpp"
#include "xiso/media_patch.hpp"
#include "xiso/paths.hpp"
#include "xiso/version.hpp"

namespace xiso
{

namespace
{

using namespace format;

constexpr char separator = static_cast<char>(std::filesystem::path::preferred_separator);

constexpr std::size_t copy_buffer_size = 0x00200000;

// The CD-ROM layout from ECMA-119. With it burning software detects the image as a data disc and no sector size has
// to be selected by hand.
constexpr std::uint64_t ecma_data_area_start = 0x8000;
constexpr std::uint64_t ecma_volume_space_size = ecma_data_area_start + 80;
constexpr std::uint64_t ecma_volume_set_size = ecma_data_area_start + 120;
constexpr std::uint64_t ecma_volume_set_identifier = ecma_data_area_start + 190;
constexpr std::uint64_t ecma_volume_creation_date = ecma_data_area_start + 813;
constexpr std::size_t ecma_date_size = 17; // 16 digits and a GMT offset byte

FileTime current_filetime()
{
	// FILETIME counts 100ns ticks since 1601-01-01.
	constexpr std::uint64_t seconds_1601_to_1970 = 11644473600ULL;
	constexpr std::uint64_t ticks_per_second = 10'000'000ULL;

	const auto seconds =
		std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	FileTime time{};
	store_le<std::uint64_t>(time, (static_cast<std::uint64_t>(seconds) + seconds_1601_to_1970) * ticks_per_second);
	return time;
}

std::span<const std::byte> as_span(std::string_view text) { return std::as_bytes(std::span(text)); }

class ImageWriter
{
public:
	ImageWriter(const std::filesystem::path& output,
				ContentSource& source,
				const Options& options,
				const Logger& logger)
		: out_(output), source_(source), options_(options), logger_(logger), buffer_(copy_buffer_size)
	{
	}

	Statistics write(Entry& root,
					 const std::filesystem::path& source_directory,
					 const std::optional<FileTime>& volume_time)
	{
		layout_directory(root);
		std::uint64_t next_sector = root_directory_sector;
		assign_sectors(root, next_sector);

		write_descriptor(root, volume_time.value_or(current_filetime()));

		write_tree(root, source_directory, std::string(1, separator));

		const std::uint64_t end = out_.seek_end();
		const std::uint64_t padding = padding_for(end, file_modulus);
		out_.fill(std::byte{0}, padding);
		write_volume_descriptors((end + padding) / sector_size);
		write_optimized_tag();

		out_.close();
		return statistics_;
	}

private:
	// ---- layout -----------------------------------------------------------------------------------------------

	/// Computes the size of the directory table of \p directory and the position of every entry inside of it, then
	/// does the same for all subdirectories.
	static void layout_directory(Entry& directory)
	{
		if (directory.children.empty())
		{
			directory.size = sector_size;
			return;
		}

		std::uint64_t size = 0;
		directory.children.for_each_preorder(
			[&](EntryTree::Node& node)
			{
				Entry& entry = node.value;
				if (entry.name.size() > max_filename_length)
					throw Error(std::format("file name {} is too long for an xiso", entry.name));

				std::uint64_t length = entry_header_size + entry.name.size();
				length += padding_for(length, dword_size);

				// Entries never straddle a sector boundary.
				if (sectors_for(size + length) > sectors_for(size)) size += padding_for(size, sector_size);

				if (size > max_table_size)
					throw Error(std::format("directory contains too many entries (at {})", entry.name));
				entry.table_offset = static_cast<std::uint32_t>(size);
				size += length;
			});
		directory.size = static_cast<std::uint32_t>(size);

		directory.children.for_each_preorder(
			[](EntryTree::Node& node)
			{
				if (node.value.is_directory) layout_directory(node.value);
			});
	}

	/// Hands out sectors: the directory table first, then its files, then the subdirectories one after the other.
	static void assign_sectors(Entry& directory, std::uint64_t& next_sector)
	{
		directory.start_sector = static_cast<std::uint32_t>(next_sector);
		if (directory.children.empty())
		{
			next_sector += 1;
			return;
		}
		next_sector += sectors_for(directory.size);

		directory.children.for_each_preorder(
			[&](EntryTree::Node& node)
			{
				if (!node.value.is_directory)
				{
					node.value.start_sector = static_cast<std::uint32_t>(next_sector);
					next_sector += sectors_for(node.value.size);
				}
			});
		directory.children.for_each_preorder(
			[&](EntryTree::Node& node)
			{
				if (node.value.is_directory) assign_sectors(node.value, next_sector);
			});

		if (next_sector > 0xFFFFFFFFULL) throw Error("the image would be too large for an xiso");
	}

	// ---- volume descriptor ------------------------------------------------------------------------------------

	void write_descriptor(const Entry& root, const FileTime& time)
	{
		std::array<std::byte, 8> root_info{};
		store_le<std::uint32_t>(std::span(root_info).first<4>(), root.start_sector);
		// Note: the size of the root table is stored as is, not rounded up to a sector like it is for subdirectories.
		store_le<std::uint32_t>(std::span(root_info).last<4>(), root.size);

		out_.fill(std::byte{0}, header_offset);
		out_.write(as_span(header_magic));
		out_.write(root_info);
		out_.write(time);
		out_.fill(std::byte{0}, header_unused_size);
		out_.write(as_span(header_magic));

		// Everything between the descriptor and the root directory is zero.
		const std::uint64_t root_offset = static_cast<std::uint64_t>(root.start_sector) * sector_size;
		out_.fill(std::byte{0}, root_offset - out_.tell());
	}

	/// Expects the file to be complete already; \p total_sectors is the image size.
	void write_volume_descriptors(std::uint64_t total_sectors)
	{
		const auto sectors = static_cast<std::uint32_t>(total_sectors);

		std::array<std::byte, 8> space_size{};
		store_le<std::uint32_t>(std::span(space_size).first<4>(), sectors);
		store_be<std::uint32_t>(std::span(space_size).last<4>(), sectors);

		std::array<std::byte, ecma_date_size> date{};
		std::fill(date.begin(), date.end() - 1, std::byte{'0'});

		std::array<std::byte, ecma_volume_creation_date - ecma_volume_set_identifier> spaces{};
		spaces.fill(std::byte{0x20});

		out_.seek(ecma_data_area_start);
		out_.write(
			as_span(std::string_view("\x01"
									 "CD001\x01",
									 7)));
		out_.seek(ecma_volume_space_size);
		out_.write(space_size);
		out_.seek(ecma_volume_set_size);
		out_.write(as_span(std::string_view("\x01\x00\x00\x01\x01\x00\x00\x01\x00\x08\x08\x00", 12)));
		out_.seek(ecma_volume_set_identifier);
		out_.write(spaces);
		for (int i = 0; i < 4; ++i) // creation, modification, expiration and effective date
			out_.write(date);
		out_.write(as_span(std::string_view("\x01", 1)));
		out_.seek(ecma_data_area_start + sector_size);
		out_.write(
			as_span(std::string_view("\xff"
									 "CD001\x01",
									 7)));
	}

	void write_optimized_tag()
	{
		std::array<std::byte, optimized_tag_size> tag{};
		const std::string text = std::string(optimized_tag_prefix) + std::string(version);
		std::memcpy(tag.data(), text.data(), std::min(text.size(), tag.size()));

		out_.seek(optimized_tag_offset);
		out_.write(tag);
	}

	// ---- tree -------------------------------------------------------------------------------------------------

	/// \param display  where this directory is in the image, for progress output
	void write_tree(Entry& directory, const std::filesystem::path& source_directory, const std::string& display)
	{
		logger_.info("adding {} (0 bytes) [OK]\n", display);

		if (directory.children.empty())
		{
			out_.seek(static_cast<std::uint64_t>(directory.start_sector) * sector_size);
			out_.fill(padding_byte, sector_size);
			return;
		}

		// The files of a directory sit right behind its table, subdirectories follow.
		out_.seek(static_cast<std::uint64_t>(directory.start_sector) * sector_size);
		directory.children.for_each_preorder(
			[&](EntryTree::Node& node)
			{
				if (!node.value.is_directory) write_file(node.value, source_directory, display);
			});
		directory.children.for_each_preorder(
			[&](EntryTree::Node& node)
			{
				Entry& entry = node.value;
				if (entry.is_directory)
					write_tree(entry, source_directory / from_utf8(entry.name), display + entry.name + separator);
			});

		// The table goes last because file sizes are only final once the data has been copied.
		const std::uint64_t table_start = static_cast<std::uint64_t>(directory.start_sector) * sector_size;
		out_.seek(table_start);
		directory.children.for_each_preorder([&](EntryTree::Node& node) { write_entry(node, table_start); });
		out_.fill(padding_byte, padding_for(out_.tell(), sector_size));
	}

	void write_entry(const EntryTree::Node& node, std::uint64_t table_start)
	{
		const Entry& entry = node.value;

		const std::uint64_t entry_start = table_start + entry.table_offset;
		const std::uint64_t position = out_.tell();
		if (entry_start < position) throw Error("internal error: directory entries out of order");
		out_.fill(padding_byte, entry_start - position);

		// Directories are stored with their size rounded up to whole sectors.
		std::uint32_t size = entry.size;
		if (entry.is_directory) size += static_cast<std::uint32_t>(padding_for(size, sector_size));

		std::array<std::byte, entry_header_size + max_filename_length> record{};
		std::span<std::byte> bytes(record);
		store_le<std::uint16_t>(bytes.subspan<0, 2>(), link_offset(node.left.get()));
		store_le<std::uint16_t>(bytes.subspan<2, 2>(), link_offset(node.right.get()));
		store_le<std::uint32_t>(bytes.subspan<4, 4>(), entry.start_sector);
		store_le<std::uint32_t>(bytes.subspan<8, 4>(), size);
		record[12] = static_cast<std::byte>(entry.is_directory ? attribute_directory : attribute_archive);
		record[13] = static_cast<std::byte>(entry.name.size());
		std::memcpy(record.data() + entry_header_size, entry.name.data(), entry.name.size());

		out_.write(bytes.first(entry_header_size + entry.name.size()));
	}

	static std::uint16_t link_offset(const EntryTree::Node* node) noexcept
	{
		return node ? static_cast<std::uint16_t>(node->value.table_offset / dword_size) : 0;
	}

	void write_file(Entry& entry, const std::filesystem::path& source_directory, const std::string& display)
	{
		out_.seek(static_cast<std::uint64_t>(entry.start_sector) * sector_size);
		auto content = source_.open(source_directory, entry);

		logger_.info("adding {}{} ({} bytes) ", display, entry.name, entry.size);
		logger_.flush();

		const bool patch_media = options_.media_enable && ends_with_ignore_case(entry.name, ".xbe");
		const std::uint32_t reported_size = entry.size;
		std::uint32_t remaining = reported_size;

		try
		{
			// With the media patch the last bytes of each chunk are held back and prepended to the next one so a
			// pattern that spans two chunks is still found.
			std::size_t carried = 0;
			while (remaining > 0)
			{
				const std::size_t wanted = std::min<std::size_t>(remaining, copy_buffer_size - carried);
				const std::size_t count = content->read(std::span(buffer_).subspan(carried, wanted));
				if (count == 0)
				{
					out_.write(std::span<const std::byte>(buffer_).first(carried));
					break;
				}
				remaining -= static_cast<std::uint32_t>(count);

				if (!patch_media)
				{
					out_.write(std::span<const std::byte>(buffer_).first(count));
					continue;
				}

				const std::size_t total = carried + count;
				patch_media_check(std::span(buffer_).first(total));
				carried = remaining > 0 ? std::min(media_patch_carry, total) : 0;
				out_.write(std::span<const std::byte>(buffer_).first(total - carried));
				if (carried > 0) std::memmove(buffer_.data(), buffer_.data() + (total - carried), carried);
			}

			entry.size = reported_size - remaining;
			out_.fill(padding_byte, padding_for(entry.size, sector_size));
		}
		catch (...)
		{
			logger_.info("failed\n");
			throw;
		}
		logger_.info("[OK]\n");

		if (entry.size != reported_size)
			logger_.info("WARNING: File {} is truncated. Reported size: {} bytes, wrote size: {} bytes!\n",
						 entry.name,
						 reported_size,
						 entry.size);

		++statistics_.files;
		statistics_.bytes += entry.size;
	}

	OutputFile out_;
	ContentSource& source_;
	const Options& options_;
	const Logger& logger_;
	std::vector<std::byte> buffer_;
	Statistics statistics_;
};

} // namespace

Statistics write_image(const std::filesystem::path& output,
					   Entry& root,
					   ContentSource& source,
					   const std::filesystem::path& source_directory,
					   const std::optional<FileTime>& volume_time,
					   const Options& options,
					   const Logger& logger)
{
	try
	{
		ImageWriter writer(output, source, options, logger);
		return writer.write(root, source_directory, volume_time);
	}
	catch (...)
	{
		std::error_code ignored;
		std::filesystem::remove(output, ignored);
		throw;
	}
}

} // namespace xiso
