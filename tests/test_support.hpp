#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace test
{

/// A fresh temporary directory that is removed again when the object goes away.
class TempDir
{
public:
	TempDir();
	~TempDir();
	TempDir(const TempDir&) = delete;
	TempDir& operator=(const TempDir&) = delete;

	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
	[[nodiscard]] std::filesystem::path operator/(const std::string& name) const { return path_ / name; }

private:
	std::filesystem::path path_;
};

using Bytes = std::vector<std::byte>;

[[nodiscard]] Bytes random_bytes(std::size_t size, unsigned seed);
[[nodiscard]] Bytes bytes_of(const std::string& text);

void write_file(const std::filesystem::path& path, const Bytes& content);
[[nodiscard]] Bytes read_file(const std::filesystem::path& path);

/// Every file below \p root (relative path with '/' separators) with its content; directories map to nothing but
/// are listed too (as "path/") so empty directories are compared as well.
[[nodiscard]] std::map<std::string, Bytes> snapshot(const std::filesystem::path& root);

/// Offset of the first occurrence of \p needle in \p haystack, or npos.
[[nodiscard]] std::size_t find_bytes(const Bytes& haystack, const Bytes& needle);
inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

} // namespace test
