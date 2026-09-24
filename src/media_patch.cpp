// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/media_patch.hpp"

#include <algorithm>

#include "xiso/format.hpp"

namespace xiso
{

std::size_t patch_media_check(std::span<std::byte> data)
{
	const auto& pattern = format::media_check_pattern;

	std::size_t patched = 0;
	auto position = data.begin();
	while (position != data.end())
	{
		const auto match = std::search(position, data.end(), pattern.begin(), pattern.end());
		if (match == data.end()) break;
		*(match + (pattern.size() - 1)) = format::media_check_patch;
		position = match + pattern.size();
		++patched;
	}
	return patched;
}

} // namespace xiso
