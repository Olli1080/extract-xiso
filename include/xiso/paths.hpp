// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace xiso
{

/// Names inside an image are treated as UTF-8; these convert to and from native paths without going through the
/// (lossy, on Windows) ANSI code page.
[[nodiscard]] inline std::string to_utf8(const std::filesystem::path& path)
{
	const std::u8string text = path.u8string();
	return {reinterpret_cast<const char*>(text.data()), text.size()};
}

[[nodiscard]] inline std::filesystem::path from_utf8(std::string_view text)
{
	return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

} // namespace xiso
