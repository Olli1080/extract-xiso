// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "xiso/logger.hpp"
#include "xiso/options.hpp"

/// The four things the tool does with an image. All of them report problems by throwing xiso::Error; the message
/// is the full text to show the user.
namespace xiso
{

/// File name of an image and the name extraction/rewriting uses: the file name without its ".iso" extension.
struct ImageNames
{
	std::string file_name; ///< e.g. "halo-ce.iso"
	std::string base_name; ///< e.g. "halo-ce"
};

[[nodiscard]] ImageNames image_names(const std::filesystem::path& image);

/// Prints the files in \p image.
Statistics list_image(const std::filesystem::path& image, const Options& options, const Logger& logger);

/// Extracts \p image into \p destination, or into a directory named after the image (without ".iso") next to it
/// if none is given.
Statistics extract_image(const std::filesystem::path& image,
						 const std::optional<std::filesystem::path>& destination,
						 const Options& options,
						 const Logger& logger);

struct RewriteResult
{
	bool skipped = false;		  ///< the image was already optimized (or empty), nothing was done
	std::filesystem::path output; ///< the new image
	Statistics statistics;
};

/// Rewrites \p image as an optimized image. The original is renamed to "<image>.old" first, and removed afterwards
/// if \p delete_original is set. The new image goes to \p destination, or the working directory if none is given.
RewriteResult rewrite_image(const std::filesystem::path& image,
							const std::optional<std::filesystem::path>& destination,
							bool delete_original,
							const Options& options,
							const Logger& logger);

/// Creates an image from the files in \p directory. Without \p name the image is called "<directory name>.iso" and
/// created in the working directory; \p name may be a file name or a path (a path ending in a separator is taken as
/// the directory to create the default-named image in).
Statistics create_image(const std::filesystem::path& directory,
						const std::optional<std::string>& name,
						const Options& options,
						const Logger& logger);

} // namespace xiso
