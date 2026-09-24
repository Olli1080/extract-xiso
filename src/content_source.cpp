// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/content_source.hpp"

#include <algorithm>

#include "xiso/image_reader.hpp"
#include "xiso/paths.hpp"

namespace xiso
{

namespace
{

class FileStream final : public ContentStream
{
public:
	explicit FileStream(const std::filesystem::path& path) : file_(path) {}

	std::size_t read(std::span<std::byte> buffer) override { return file_.read_some(buffer); }

private:
	InputFile file_;
};

class ImageStream final : public ContentStream
{
public:
	ImageStream(ImageReader& reader, std::uint32_t sector, std::uint32_t size) : reader_(reader), remaining_(size)
	{
		reader_.seek_to_sector(sector);
	}

	std::size_t read(std::span<std::byte> buffer) override
	{
		const std::size_t wanted = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), remaining_));
		const std::size_t count = reader_.read_some(buffer.first(wanted));
		remaining_ -= count;
		return count;
	}

private:
	ImageReader& reader_;
	std::uint64_t remaining_;
};

} // namespace

std::unique_ptr<ContentStream> FilesystemSource::open(const std::filesystem::path& directory, const Entry& entry)
{
	return std::make_unique<FileStream>(directory / from_utf8(entry.name));
}

std::unique_ptr<ContentStream> ImageSource::open(const std::filesystem::path&, const Entry& entry)
{
	return std::make_unique<ImageStream>(reader_, entry.source_sector, entry.size);
}

} // namespace xiso
