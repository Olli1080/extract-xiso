// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/binary_file.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <ios>

#include "xiso/error.hpp"
#include "xiso/paths.hpp"

namespace xiso
{

namespace
{

std::string display(const std::filesystem::path& path) { return to_utf8(path); }

} // namespace

InputFile::InputFile(const std::filesystem::path& path) : path_(path), stream_(path, std::ios::binary)
{
	if (!stream_) throw Error(std::format("open error: {} {}", display(path_), last_system_error()));
}

void InputFile::seek(std::uint64_t offset)
{
	stream_.clear();
	stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
	if (stream_.fail()) throw Error(std::format("seek error: {}", last_system_error()));
}

std::uint64_t InputFile::tell()
{
	const auto position = stream_.tellg();
	if (position < 0) throw Error(std::format("seek error: {}", last_system_error()));
	return static_cast<std::uint64_t>(position);
}

std::size_t InputFile::read_some(std::span<std::byte> buffer)
{
	stream_.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	const auto count = static_cast<std::size_t>(stream_.gcount());
	if (stream_.bad()) throw Error(std::format("read error: {}", last_system_error()));
	stream_.clear(); // a short read only means end of file
	return count;
}

bool InputFile::try_read_exact(std::span<std::byte> buffer) { return read_some(buffer) == buffer.size(); }

void InputFile::read_exact(std::span<std::byte> buffer)
{
	if (!try_read_exact(buffer)) throw Error(std::format("read error: unexpected end of file in {}", display(path_)));
}

OutputFile::OutputFile(const std::filesystem::path& path)
	: path_(path), stream_(path, std::ios::binary | std::ios::trunc)
{
	if (!stream_) throw Error(std::format("open error: {} {}", display(path_), last_system_error()));
}

void OutputFile::seek(std::uint64_t offset)
{
	stream_.clear();
	stream_.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
	if (stream_.fail()) throw Error(std::format("seek error: {}", last_system_error()));
}

std::uint64_t OutputFile::seek_end()
{
	stream_.clear();
	stream_.seekp(0, std::ios::end);
	return tell();
}

std::uint64_t OutputFile::tell()
{
	const auto position = stream_.tellp();
	if (position < 0) throw Error(std::format("seek error: {}", last_system_error()));
	return static_cast<std::uint64_t>(position);
}

void OutputFile::write(std::span<const std::byte> data)
{
	stream_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
	if (!stream_) throw Error(std::format("write error: {}", last_system_error()));
}

void OutputFile::fill(std::byte value, std::uint64_t count)
{
	std::array<std::byte, 4096> block;
	block.fill(value);
	while (count > 0)
	{
		const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(count, block.size()));
		write(std::span<const std::byte>(block).first(chunk));
		count -= chunk;
	}
}

void OutputFile::close()
{
	stream_.close();
	if (stream_.fail()) throw Error(std::format("write error: {}", last_system_error()));
}

} // namespace xiso
