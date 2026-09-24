// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <cstdint>

namespace xiso
{

struct Options
{
	/// Apply the "media enable" patch to .xbe files when creating or rewriting an image.
	bool media_enable = true;
	/// Leave the $SystemUpdate folder out when listing or extracting.
	bool skip_system_update = false;
};

/// Number of files and bytes an operation processed.
struct Statistics
{
	std::uint64_t files = 0;
	std::uint64_t bytes = 0;

	Statistics& operator+=(const Statistics& other) noexcept
	{
		files += other.files;
		bytes += other.bytes;
		return *this;
	}
};

} // namespace xiso
