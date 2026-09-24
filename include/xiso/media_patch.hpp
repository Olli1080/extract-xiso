// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <cstddef>
#include <span>

namespace xiso
{

/// Number of trailing bytes of a buffer that may be the start of a pattern continuing in the next chunk.
inline constexpr std::size_t media_patch_carry = 7;

/// Patches every media check in \p data (the "media enable" patch that lets an .xbe run from burned media).
/// Returns the number of patches applied.
std::size_t patch_media_check(std::span<std::byte> data);

} // namespace xiso
