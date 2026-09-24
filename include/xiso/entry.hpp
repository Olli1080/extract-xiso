// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

#include "xiso/avl_tree.hpp"

namespace xiso
{

/// Orders names the way the Xbox does: byte wise, ASCII case-insensitive.
[[nodiscard]] std::strong_ordering compare_names(std::string_view lhs, std::string_view rhs) noexcept;

/// True if \p name ends with \p suffix, ignoring ASCII case.
[[nodiscard]] bool ends_with_ignore_case(std::string_view name, std::string_view suffix) noexcept;

struct Entry;

struct EntryNameOrder
{
	std::strong_ordering operator()(const Entry& lhs, const Entry& rhs) const noexcept;
};

using EntryTree = AvlTree<Entry, EntryNameOrder>;

/// A file or directory in the tree that is about to be written to an image.
struct Entry
{
	std::string name;
	/// File size in bytes; for directories the size of the directory table (filled in by the layout pass).
	std::uint32_t size = 0;
	std::uint32_t start_sector = 0;
	/// Where the data lives in the image this entry was read from (rewrite only).
	std::uint32_t source_sector = 0;
	/// Offset of this entry within its parent's directory table.
	std::uint32_t table_offset = 0;
	bool is_directory = false;
	/// Directory contents; an empty directory has no children.
	EntryTree children;
};

inline std::strong_ordering EntryNameOrder::operator()(const Entry& lhs, const Entry& rhs) const noexcept
{
	return compare_names(lhs.name, rhs.name);
}

} // namespace xiso
