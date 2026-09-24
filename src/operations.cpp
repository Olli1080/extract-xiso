// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/operations.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>
#include <system_error>
#include <vector>

#include "xiso/content_source.hpp"
#include "xiso/entry.hpp"
#include "xiso/error.hpp"
#include "xiso/format.hpp"
#include "xiso/fs_scan.hpp"
#include "xiso/image_reader.hpp"
#include "xiso/image_writer.hpp"
#include "xiso/paths.hpp"

namespace xiso
{

namespace
{

namespace fs = std::filesystem;

constexpr char separator = static_cast<char>(fs::path::preferred_separator);
constexpr std::string_view system_update_folder = "$SystemUpdate";
constexpr std::size_t extract_buffer_size = 0x00200000;
/// Deeper nesting than any real image has; stops directory loops in damaged images.
constexpr int max_directory_depth = 256;

/// Adds context to an error and rethrows it.
template <typename Action> decltype(auto) with_context(Action&& action, const std::string& context)
{
	try
	{
		return action();
	}
	catch (const Error& error)
	{
		throw Error(std::format("{}\n{}", error.what(), context));
	}
}

// ---- list and extract ---------------------------------------------------------------------------------------

class TreeWalker
{
public:
	enum class Mode
	{
		list,
		extract
	};

	TreeWalker(ImageReader& reader, Mode mode, const Options& options, const Logger& logger)
		: reader_(reader),
		  mode_(mode),
		  options_(options),
		  logger_(logger),
		  buffer_(mode == Mode::extract ? extract_buffer_size : 0)
	{
	}

	/// \param linked_list_compat  see ImageReader::walk_directory
	Statistics run(const fs::path& output_root, const std::string& display_root, bool linked_list_compat)
	{
		walk(reader_.sector_offset(reader_.volume().root_sector), output_root, display_root, linked_list_compat, 0);
		return statistics_;
	}

private:
	[[nodiscard]] bool extracting() const noexcept { return mode_ == Mode::extract; }

	void walk(std::uint64_t table_start,
			  const fs::path& output_dir,
			  const std::string& display,
			  bool linked_list_compat,
			  int depth)
	{
		if (depth > max_directory_depth)
			throw Error("directories are nested too deeply, the image appears to be corrupt");

		reader_.walk_directory<int>(
			table_start,
			linked_list_compat,
			[](const DirRecord&) { return 0; },
			[&](DirRecord& record, int&, bool compat)
			{
				if (record.is_directory())
					visit_directory(record, output_dir, display, compat, depth);
				else
					visit_file(record, output_dir, display);
			});
	}

	void visit_directory(const DirRecord& record,
						 const fs::path& output_dir,
						 const std::string& display,
						 bool linked_list_compat,
						 int depth)
	{
		if (options_.skip_system_update && record.name.contains(system_update_folder)) return;

		const fs::path directory = output_dir / from_utf8(record.name);
		if (extracting())
		{
			std::error_code ec;
			fs::create_directories(directory, ec);
			if (ec) throw Error(std::format("unable to create directory {}: {}", to_utf8(directory), ec.message()));
		}
		logger_.info("{}{}{}{} (0 bytes){}\n",
					 extracting() ? "creating " : "",
					 display,
					 record.name,
					 separator,
					 extracting() ? " [OK]" : "");
		logger_.flush();

		if (record.size > 0)
			walk(reader_.sector_offset(record.start_sector),
				 directory,
				 display + record.name + separator,
				 linked_list_compat,
				 depth + 1);
	}

	void visit_file(DirRecord& record, const fs::path& output_dir, const std::string& display)
	{
		if (extracting())
			extract_file(record, output_dir / from_utf8(record.name), display);
		else
			logger_.info("{}{} ({} bytes)\n", display, record.name, record.size);

		++statistics_.files;
		statistics_.bytes += record.size;
	}

	/// Copies one file out of the image. Shrinks record.size if the image turns out to be truncated.
	void extract_file(DirRecord& record, const fs::path& target, const std::string& display)
	{
		OutputFile out(target);
		reader_.seek_to_sector(record.start_sector);

		if (record.size == 0)
			logger_.info("extracting {}{} (0 bytes) [100%]\r", display, record.name);
		else
		{
			std::uint32_t copied = 0;
			while (copied < record.size)
			{
				const std::size_t wanted = std::min<std::size_t>(record.size - copied, buffer_.size());
				const std::size_t count = reader_.read_some(std::span(buffer_).first(wanted));
				if (count == 0) break;
				out.write(std::span<const std::byte>(buffer_).first(count));
				copied += static_cast<std::uint32_t>(count);

				logger_.info("extracting {}{} ({} bytes) [{}%]\r",
							 display,
							 record.name,
							 record.size,
							 static_cast<std::uint64_t>(copied) * 100 / record.size);
				logger_.flush();
			}
			if (copied < record.size)
			{
				logger_.info("\nWARNING: File {} is truncated. Reported size: {} bytes, read size: {} bytes!",
							 record.name,
							 record.size,
							 copied);
				record.size = copied;
			}
		}
		out.close();
		logger_.info("\n");
	}

	ImageReader& reader_;
	Mode mode_;
	const Options& options_;
	const Logger& logger_;
	std::vector<std::byte> buffer_;
	Statistics statistics_;
};

/// Reads the whole directory tree of an image into memory in the order that reproduces its layout when written.
void read_tree(ImageReader& reader, std::uint64_t table_start, EntryTree& children, bool linked_list_compat, int depth)
{
	if (depth > max_directory_depth) throw Error("directories are nested too deeply, the image appears to be corrupt");

	// Entries have to go into the tree in the order they are read from disk, not in name order: the tree shape,
	// and with it the layout of the rewritten image, depends on the insertion order.
	reader.walk_directory<Entry*>(
		table_start,
		linked_list_compat,
		[&](const DirRecord& record)
		{
			Entry entry;
			entry.name = record.name;
			entry.size = record.size;
			entry.source_sector = record.start_sector;
			entry.is_directory = record.is_directory();

			Entry* stored = children.insert(std::move(entry));
			if (!stored) throw Error("this iso appears to be corrupt");
			return stored;
		},
		[&](const DirRecord& record, Entry*& entry, bool compat)
		{
			if (entry->is_directory && record.size > 0)
				read_tree(reader, reader.sector_offset(record.start_sector), entry->children, compat, depth + 1);
		});
}

[[nodiscard]] std::string with_directory_separator(const std::string& path) { return path + separator; }

} // namespace

ImageNames image_names(const fs::path& image)
{
	ImageNames names;
	names.file_name = to_utf8(image.filename());
	names.base_name = names.file_name;
	if (names.file_name.size() > 4 && ends_with_ignore_case(names.file_name, ".iso"))
		names.base_name.resize(names.file_name.size() - 4);
	return names;
}

Statistics list_image(const fs::path& image, const Options& options, const Logger& logger)
{
	const ImageNames names = image_names(image);
	if (names.file_name.empty()) throw Error(std::format("invalid xiso image name: {}", to_utf8(image)));

	return with_context(
		[&]
		{
			ImageReader reader(image);
			if (reader.volume().empty())
			{
				logger.info("xbox image {} contains no files.\n", names.file_name);
				return Statistics{};
			}
			logger.info("listing {}:\n\n", names.file_name);
			return TreeWalker(reader, TreeWalker::Mode::list, options, logger)
				.run({}, std::string(1, separator), !is_optimized(image));
		},
		std::format("failed to list xbox iso image {}", names.file_name));
}

Statistics extract_image(const fs::path& image,
						 const std::optional<fs::path>& destination,
						 const Options& options,
						 const Logger& logger)
{
	const ImageNames names = image_names(image);
	if (names.file_name.empty()) throw Error(std::format("invalid xiso image name: {}", to_utf8(image)));

	return with_context(
		[&]
		{
			ImageReader reader(image);
			if (reader.volume().empty())
			{
				logger.info("xbox image {} contains no files.\n", names.file_name);
				return Statistics{};
			}
			logger.info("extracting {}:\n\n", names.file_name);

			const fs::path root =
				long_path(destination ? *destination : image.parent_path() / from_utf8(names.base_name));
			std::error_code ec;
			fs::create_directories(root, ec);
			if (ec) throw Error(std::format("unable to create directory {}: {}", to_utf8(root), ec.message()));

			const std::string display = with_directory_separator(destination ? to_utf8(*destination) : names.base_name);
			return TreeWalker(reader, TreeWalker::Mode::extract, options, logger)
				.run(root, display, !is_optimized(image));
		},
		std::format("failed to extract xbox iso image {}", names.file_name));
}

RewriteResult rewrite_image(const fs::path& image,
							const std::optional<fs::path>& destination,
							bool delete_original,
							const Options& options,
							const Logger& logger)
{
	const ImageNames names = image_names(image);
	if (names.file_name.empty()) throw Error(std::format("invalid xiso image name: {}", to_utf8(image)));

	RewriteResult result;
	const std::string context = std::format("failed to rewrite xbox iso image {}", names.file_name);

	// Look at the image before touching it, so a file that is not an image is not renamed.
	const bool skip = with_context(
		[&]
		{
			if (is_optimized(image))
			{
				logger.info("{} is already optimized, skipping...\n", to_utf8(image));
				return true;
			}
			if (ImageReader(image).volume().empty())
			{
				logger.info("xbox image {} contains no files.\n", names.file_name);
				return true;
			}
			return false;
		},
		context);
	if (skip)
	{
		result.skipped = true;
		return result;
	}

	fs::path old_image = image;
	old_image += ".old";
	std::error_code ec;
	if (fs::exists(long_path(old_image), ec))
		throw Error(std::format("{} already exists, cannot rewrite {}", to_utf8(old_image), to_utf8(image)));
	fs::rename(long_path(image), long_path(old_image), ec);
	if (ec) throw Error(std::format("cannot rename {} to {}", to_utf8(image), to_utf8(old_image)));

	try
	{
		with_context(
			[&]
			{
				ImageReader reader(old_image);

				Entry root;
				root.is_directory = true;
				read_tree(reader, reader.sector_offset(reader.volume().root_sector), root.children, true, 0);

				const fs::path directory = destination ? *destination : fs::current_path();
				result.output = directory / from_utf8(names.base_name + ".iso");
				logger.info("rewriting {}.iso:\n\n", names.base_name);

				ImageSource source(reader);
				result.statistics =
					write_image(result.output, root, source, {}, reader.read_timestamp(), options, logger);
			},
			context);
	}
	catch (...)
	{
		// Put the original back rather than leaving it stranded under its ".old" name.
		fs::rename(long_path(old_image), long_path(image), ec);
		throw;
	}

	if (delete_original)
	{
		fs::remove(long_path(old_image), ec);
		if (ec) logger.error("unable to delete {}\n", to_utf8(old_image));
	}
	return result;
}

Statistics create_image(const fs::path& directory,
						const std::optional<std::string>& name,
						const Options& options,
						const Logger& logger)
{
	std::error_code ec;
	if (!fs::is_directory(long_path(directory), ec))
		throw Error(std::format(
			"unable to change to directory {}: {}", to_utf8(directory), ec ? ec.message() : "Not a directory"));

	// The image is named after the directory unless a name was given.
	fs::path root = fs::absolute(directory).lexically_normal();
	if (!root.has_filename()) root = root.parent_path();
	std::string directory_name = to_utf8(root.filename());
	if (directory_name.empty()) directory_name = "root";

	fs::path output_directory;
	std::string image_file;
	if (name && from_utf8(*name).has_filename())
	{
		const fs::path given = from_utf8(*name);
		output_directory = given.parent_path();
		image_file = to_utf8(given.filename());
	}
	else
	{
		if (name) output_directory = from_utf8(*name);
		image_file = directory_name + ".iso";
	}
	const fs::path output = output_directory / from_utf8(image_file);

	return with_context(
		[&]
		{
			logger.info("\ncreating {}:\n\n", image_file);

			logger.info("generating avl tree from filesystem: ");
			logger.flush();
			Entry tree;
			try
			{
				tree = scan_directory(root, logger);
			}
			catch (const Error&)
			{
				logger.info("failed!\n\n");
				throw;
			}
			logger.info("[OK]\n\n");

			FilesystemSource source;
			const Statistics statistics = write_image(output, tree, source, root, std::nullopt, options, logger);
			logger.info("\nsuccessfully created {} ({} files totalling {} bytes added)\n",
						image_file,
						statistics.files,
						statistics.bytes);
			return statistics;
		},
		std::format("could not create {}", image_file));
}

} // namespace xiso
