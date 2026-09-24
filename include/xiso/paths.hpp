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

/// Path to hand to the file system. On Windows this is the absolute path with the extended-length prefix, which
/// lifts the 260 character MAX_PATH limit (a game folder extracted into a folder of the same long name easily
/// exceeds it); everywhere else the path is returned as it is.
[[nodiscard]] inline std::filesystem::path long_path(const std::filesystem::path& path)
{
#ifdef _WIN32
	constexpr std::wstring_view prefix = LR"(\\?\)";
	constexpr std::wstring_view unc_prefix = LR"(\\?\UNC\)";
	if (path.empty() || path.native().starts_with(prefix)) return path;

	const std::filesystem::path absolute = std::filesystem::absolute(path).lexically_normal();
	const std::wstring& native = absolute.native();
	if (native.starts_with(LR"(\\)")) // \\server\share becomes \\?\UNC\server\share
		return std::filesystem::path(std::wstring(unc_prefix) + native.substr(2));
	return std::filesystem::path(std::wstring(prefix) + native);
#else
	return path;
#endif
}

} // namespace xiso
