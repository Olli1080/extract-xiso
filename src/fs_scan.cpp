// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/fs_scan.hpp"

#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <system_error>

#include "xiso/error.hpp"
#include "xiso/format.hpp"
#include "xiso/paths.hpp"

namespace xiso
{

namespace
{

namespace fs = std::filesystem;

/// Shows the name of the entry that is being looked at, overwriting the previous one.
class ScanProgress
{
public:
	explicit ScanProgress(const Logger& logger) : logger_(logger) {}

	void show(const std::string& name)
	{
		const std::size_t length = name.size();
		logger_.info("{}{}{}{}",
					 std::string(width_, '\b'),
					 name,
					 std::string(width_ > length ? width_ - length : 0, ' '),
					 std::string(width_ > length ? width_ - length : 0, '\b'));
		width_ = length;
		logger_.flush();
	}

	/// Erases whatever is still shown.
	void clear()
	{
		logger_.info("{}{}{}", std::string(width_, '\b'), std::string(width_, ' '), std::string(width_, '\b'));
		width_ = 0;
	}

private:
	const Logger& logger_;
	std::size_t width_ = 0;
};

void scan_into(const fs::path& directory, Entry& parent, ScanProgress& progress, const Logger& logger)
{
	std::error_code ec;
	fs::directory_iterator iterator(directory, ec);
	if (ec) throw Error(std::format("unable to read directory {}: {}", to_utf8(directory), ec.message()));

	for (; iterator != fs::directory_iterator{}; iterator.increment(ec))
	{
		if (ec) break;
		const fs::directory_entry& item = *iterator;

		Entry entry;
		entry.name = to_utf8(item.path().filename());
		progress.show(entry.name);

		const fs::file_status status = item.status(ec);
		if (ec) throw Error(std::format("read error: {}: {}", to_utf8(item.path()), ec.message()));

		if (status.type() == fs::file_type::directory)
		{
			entry.is_directory = true;
			scan_into(item.path(), entry, progress, logger);
		}
		else if (status.type() == fs::file_type::regular)
		{
			const std::uintmax_t size = item.file_size(ec);
			if (ec) throw Error(std::format("read error: {}: {}", to_utf8(item.path()), ec.message()));
			if (size > std::numeric_limits<std::uint32_t>::max())
			{
				logger.error("file {} is too large for xiso, skipping...\n", entry.name);
				continue;
			}
			entry.size = static_cast<std::uint32_t>(size);
		}
		else
			continue;

		if (entry.name.size() > format::max_filename_length)
			throw Error(std::format("file name {} is too long for an xiso", entry.name));

		const std::string name = entry.name;
		if (!parent.children.insert(std::move(entry)))
			throw Error(std::format("error inserting file {} into tree (duplicate filename?)", name));
	}
	if (ec) throw Error(std::format("unable to read directory {}: {}", to_utf8(directory), ec.message()));
}

} // namespace

Entry scan_directory(const fs::path& directory, const Logger& logger)
{
	Entry root;
	root.is_directory = true;

	ScanProgress progress(logger);
	scan_into(long_path(directory), root, progress, logger);
	progress.clear();
	return root;
}

} // namespace xiso
