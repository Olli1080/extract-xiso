// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace xiso
{

/// Random access binary reader. All failures are reported as xiso::Error.
class InputFile
{
public:
	explicit InputFile(const std::filesystem::path& path);

	void seek(std::uint64_t offset);
	[[nodiscard]] std::uint64_t tell();

	/// Reads up to buffer.size() bytes and returns how many were read (fewer only at end of file).
	[[nodiscard]] std::size_t read_some(std::span<std::byte> buffer);
	/// Reads exactly buffer.size() bytes or throws.
	void read_exact(std::span<std::byte> buffer);
	/// Reads exactly buffer.size() bytes, returns false at a premature end of file.
	[[nodiscard]] bool try_read_exact(std::span<std::byte> buffer);

private:
	std::filesystem::path path_;
	std::ifstream stream_;
};

/// Random access binary writer. All failures are reported as xiso::Error.
class OutputFile
{
public:
	/// Creates (or truncates) the file at \p path.
	explicit OutputFile(const std::filesystem::path& path);

	void seek(std::uint64_t offset);
	/// Moves to the end of the file and returns its size.
	std::uint64_t seek_end();
	[[nodiscard]] std::uint64_t tell();

	void write(std::span<const std::byte> data);
	/// Writes \p count copies of \p value.
	void fill(std::byte value, std::uint64_t count);
	void close();

private:
	std::filesystem::path path_;
	std::ofstream stream_;
};

} // namespace xiso
