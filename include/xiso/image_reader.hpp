// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "xiso/binary_file.hpp"
#include "xiso/endian.hpp"
#include "xiso/error.hpp"
#include "xiso/format.hpp"

namespace xiso
{

/// One directory table entry as stored in the image.
struct DirRecord
{
	std::uint16_t left = 0;	 ///< dword offset of the left child within the table, 0 if none
	std::uint16_t right = 0; ///< dword offset of the right child within the table, 0 if none
	std::uint32_t start_sector = 0;
	std::uint32_t size = 0;
	std::uint8_t attributes = 0;
	std::string name;

	[[nodiscard]] bool is_directory() const noexcept { return (attributes & format::attribute_directory) != 0; }
};

/// Location of the root directory found in the volume descriptor.
struct VolumeInfo
{
	std::uint32_t root_sector = 0;
	std::uint32_t root_size = 0;

	[[nodiscard]] bool empty() const noexcept { return root_sector == 0 && root_size == 0; }
};

using FileTime = std::array<std::byte, format::filetime_size>;

/// Read access to an existing XISO image.
class ImageReader
{
public:
	/// Opens \p path and validates the volume descriptor. Throws xiso::Error if this is not an Xbox image.
	explicit ImageReader(const std::filesystem::path& path);

	[[nodiscard]] const VolumeInfo& volume() const noexcept { return volume_; }
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

	/// Byte offset of a directory table or file that starts at \p sector.
	[[nodiscard]] std::uint64_t sector_offset(std::uint32_t sector) const noexcept
	{
		return static_cast<std::uint64_t>(sector) * format::sector_size + partition_offset_;
	}

	/// The volume creation time stored in the descriptor.
	[[nodiscard]] FileTime read_timestamp();

	/// Positions the reader at the data of a file.
	void seek_to_sector(std::uint32_t sector) { file_.seek(sector_offset(sector)); }
	[[nodiscard]] std::size_t read_some(std::span<std::byte> buffer) { return file_.read_some(buffer); }

	/// Walks one directory table, following the binary tree of entries.
	///
	/// \p on_read is called for every entry in the order the entries are read from disk (parent before children) and
	/// returns a cookie that is handed back to \p on_visit. \p on_visit is called in name order (in-order traversal);
	/// this is where subdirectories should be descended into by calling walk_directory again. It receives the
	/// current "linked list compatibility" flag, which has to be passed on to those nested calls.
	///
	/// The walk is iterative along the tree, so images written as degenerate linked lists cannot exhaust the stack.
	///
	/// \param linked_list_compat  handle images whose entries were laid out as a linked list (right links skipping
	///                            ahead over sector boundaries)
	template <typename Cookie, typename OnRead, typename OnVisit>
	void walk_directory(std::uint64_t table_start, bool linked_list_compat, OnRead&& on_read, OnVisit&& on_visit);

private:
	[[nodiscard]] DirRecord read_record(std::uint16_t left);

	std::filesystem::path path_;
	InputFile file_;
	std::uint64_t partition_offset_ = 0;
	VolumeInfo volume_;
};

/// Reads the "optimized" tag of an image written by extract-xiso. Images too short to carry it are not optimized.
[[nodiscard]] bool is_optimized(const std::filesystem::path& path);

/// Rejects names that would escape the extraction directory or that cannot be a single path component.
void validate_filename(const std::string& name);

namespace detail
{

inline constexpr std::size_t max_records_per_table = 0x10000;
inline constexpr std::uint64_t max_table_span = format::max_table_size + format::sector_size;

} // namespace detail

template <typename Cookie, typename OnRead, typename OnVisit>
void ImageReader::walk_directory(std::uint64_t table_start,
								 bool linked_list_compat,
								 OnRead&& on_read,
								 OnVisit&& on_visit)
{
	struct Pending
	{
		DirRecord record;
		Cookie cookie;
	};

	std::vector<Pending> stack;
	std::size_t records = 0;
	std::uint64_t link_bytes = 0; // table offset of the entry that is read next (as far as it is known)

	file_.seek(table_start);

	for (;;)
	{
		std::array<std::byte, 2> word{};
		file_.read_exact(word);
		const auto left = load_le<std::uint16_t>(word);

		if (left == format::table_padding_marker)
		{
			if (link_bytes == 0 && stack.empty()) return; // empty directory
			// The rest of this sector is padding, the next entry starts on the following sector boundary.
			link_bytes += format::sector_size - link_bytes % format::sector_size;
			if (link_bytes > detail::max_table_span) throw Error("directory table appears to be corrupt");
			file_.seek(table_start + link_bytes);
			continue;
		}

		if (++records > detail::max_records_per_table) throw Error("directory table appears to be corrupt");

		DirRecord record = read_record(left);
		Cookie cookie = on_read(record);

		if (record.left != 0)
		{
			linked_list_compat = false;
			link_bytes = static_cast<std::uint64_t>(record.left) * format::dword_size;
			file_.seek(table_start + link_bytes);
			stack.push_back({std::move(record), std::move(cookie)});
			continue;
		}

		const std::uint64_t position_after_entry = linked_list_compat ? file_.tell() : 0;
		Pending current{std::move(record), std::move(cookie)};

		for (;;)
		{
			on_visit(current.record, current.cookie, linked_list_compat);

			if (current.record.right != 0)
			{
				std::uint32_t right = current.record.right;
				if (linked_list_compat)
				{
					const std::uint64_t sector = (position_after_entry - table_start) / format::sector_size;
					if (static_cast<std::uint64_t>(right) * format::dword_size / format::sector_size > sector)
						right = static_cast<std::uint32_t>(sector * (format::sector_size / format::dword_size) +
														   format::sector_size / format::dword_size);
				}
				link_bytes = static_cast<std::uint64_t>(right) * format::dword_size;
				file_.seek(table_start + link_bytes);
				break; // read the right child
			}

			if (stack.empty()) return;
			current = std::move(stack.back());
			stack.pop_back();
		}
	}
}

} // namespace xiso
