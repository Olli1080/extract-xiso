// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

#include "xiso/binary_file.hpp"
#include "xiso/entry.hpp"

namespace xiso
{

class ImageReader;

/// Sequential reader for the data of one file that is being added to an image.
class ContentStream
{
public:
	virtual ~ContentStream() = default;
	/// Reads up to buffer.size() bytes, returns 0 at the end of the data.
	virtual std::size_t read(std::span<std::byte> buffer) = 0;
};

/// Where the contents of the files that go into a new image come from.
class ContentSource
{
public:
	virtual ~ContentSource() = default;
	/// Opens the data of \p entry, which lives in \p directory of the source tree.
	virtual std::unique_ptr<ContentStream> open(const std::filesystem::path& directory, const Entry& entry) = 0;
};

/// Files from a directory on disk.
class FilesystemSource final : public ContentSource
{
public:
	std::unique_ptr<ContentStream> open(const std::filesystem::path& directory, const Entry& entry) override;
};

/// Files from an existing image (used to rewrite it).
class ImageSource final : public ContentSource
{
public:
	explicit ImageSource(ImageReader& reader) noexcept : reader_(reader) {}

	std::unique_ptr<ContentStream> open(const std::filesystem::path& directory, const Entry& entry) override;

private:
	ImageReader& reader_;
};

} // namespace xiso
