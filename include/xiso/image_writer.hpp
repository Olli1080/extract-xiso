// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <filesystem>
#include <optional>

#include "xiso/content_source.hpp"
#include "xiso/entry.hpp"
#include "xiso/image_reader.hpp"
#include "xiso/logger.hpp"
#include "xiso/options.hpp"

namespace xiso
{

/// Writes \p root (a directory entry that stands for the root of the volume) as an XISO image to \p output.
///
/// Lays out the directory tables and file data, copies the file contents from \p source and writes the volume
/// descriptors. \p source_directory is handed to the source as the location of the root directory's files.
/// \p volume_time is stored in the descriptor; the current time is used if none is given.
///
/// Fills in the sector numbers of \p root. Removes the partial output file and rethrows if anything fails.
Statistics write_image(const std::filesystem::path& output,
					   Entry& root,
					   ContentSource& source,
					   const std::filesystem::path& source_directory,
					   const std::optional<FileTime>& volume_time,
					   const Options& options,
					   const Logger& logger);

} // namespace xiso
