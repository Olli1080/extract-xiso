// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <filesystem>

#include "xiso/entry.hpp"
#include "xiso/logger.hpp"

namespace xiso
{

/// Builds the entry tree for the contents of \p directory. The result is a directory entry standing for the root.
///
/// Symbolic links are followed; entries that are neither regular files nor directories are ignored and so are files
/// that are too large for the format (4 GiB - 1).
[[nodiscard]] Entry scan_directory(const std::filesystem::path& directory, const Logger& logger);

} // namespace xiso
