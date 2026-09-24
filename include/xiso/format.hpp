// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

/// Constants describing the on-disk XISO (xdvdfs) layout.
namespace xiso::format
{

inline constexpr std::uint32_t sector_size = 2048;

/// The volume descriptor lives at this offset (relative to the start of the game partition).
inline constexpr std::uint64_t header_offset = 0x10000;
inline constexpr std::string_view header_magic = "MICROSOFT*XBOX*MEDIA";
inline constexpr std::size_t header_magic_size = 20;
inline constexpr std::size_t filetime_size = 8;
inline constexpr std::size_t header_unused_size = 0x7c8;

/// Offsets of the game partition inside a full-disc dump: plain XISO, global (XGD2), XGD3 and XGD1.
inline constexpr std::array<std::uint64_t, 4> partition_offsets{0, 0x0FD90000, 0x02080000, 0x18300000};

inline constexpr std::uint32_t file_modulus = 0x10000;
inline constexpr std::uint32_t root_directory_sector = 0x108;

/// Files written by extract-xiso carry this tag so a rewrite knows the image is already optimized.
inline constexpr std::uint64_t optimized_tag_offset = 31337;
inline constexpr std::string_view optimized_tag_prefix = "in!xiso!";
inline constexpr std::size_t optimized_tag_size = 8 + 16;
/// Only the first characters are compared when checking for the tag, so images written by other versions match.
inline constexpr std::size_t optimized_tag_compare_size = 7;

/// Fixed part of a directory table entry: left(2) right(2) sector(4) size(4) attributes(1) name length(1).
inline constexpr std::uint32_t entry_header_size = 14;
inline constexpr std::size_t max_filename_length = 255;
inline constexpr std::uint32_t dword_size = 4;
/// Left/right links are 16 bit dword offsets, so a directory table cannot exceed this many bytes.
inline constexpr std::uint32_t max_table_size = 0xFFFF * dword_size;

inline constexpr std::uint8_t attribute_directory = 0x10;
inline constexpr std::uint8_t attribute_archive = 0x20;

inline constexpr std::uint16_t table_padding_marker = 0xFFFF;
inline constexpr std::byte padding_byte{0xFF};

/// Byte sequence in .xbe files whose trailing conditional jump is turned into an unconditional one ("media enable").
inline constexpr std::array<std::byte, 8> media_check_pattern{std::byte{0xe8},
															  std::byte{0xca},
															  std::byte{0xfd},
															  std::byte{0xff},
															  std::byte{0xff},
															  std::byte{0x85},
															  std::byte{0xc0},
															  std::byte{0x7d}};
inline constexpr std::byte media_check_patch{0xeb};

/// Number of sectors needed to hold \p bytes.
[[nodiscard]] constexpr std::uint64_t sectors_for(std::uint64_t bytes) noexcept
{
	return (bytes + sector_size - 1) / sector_size;
}

/// Padding needed to bring \p value up to the next multiple of \p alignment.
[[nodiscard]] constexpr std::uint64_t padding_for(std::uint64_t value, std::uint64_t alignment) noexcept
{
	return (alignment - value % alignment) % alignment;
}

} // namespace xiso::format
