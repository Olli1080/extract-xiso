// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "xiso/error.hpp"
#include "xiso/logger.hpp"
#include "xiso/operations.hpp"
#include "xiso/options.hpp"
#include "xiso/paths.hpp"
#include "xiso/version.hpp"

namespace
{

using namespace xiso;

std::string banner()
{
	return std::format("extract-xiso v{} for {} - written by in <in@fishtank.com>\n", version, target);
}

constexpr std::string_view usage_text =
	"extract-xiso [options] [-[lrx]] <file1.xiso> [file2.xiso] ...\n"
	"  extract-xiso [options] -c <dir> [name] [-c <dir> [name]] ...";

struct Arguments
{
	std::vector<std::vector<std::string>> create; ///< each entry: directory and optionally the image name
	std::vector<std::string> images;
	std::optional<std::string> directory;
	bool list = false;
	bool rewrite = false;
	bool extract = false;
	bool delete_original = false;
	bool no_media_patch = false;
	bool quiet = false;
	bool silent = false;
	bool skip_system_update = false;
};

void configure(CLI::App& app, Arguments& args)
{
	app.usage(std::string(usage_text));
	app.set_help_flag("-h", "Print this help text and exit.")->group("Options");
	app.set_version_flag("-v", banner(), "Print version information and exit.")->group("Options");
	app.allow_windows_style_options(false); // "/path" is a path, not an option
	app.get_formatter()->column_width(24);

	app.add_option("xiso", args.images, "xiso image(s) to list, rewrite or extract")->option_text("<file.xiso> ...");

	app.add_option("-c",
				   args.create,
				   "Create xiso from file(s) starting in <dir>. If the [name] parameter is specified, the "
				   "xiso will be created with the (path and) name given, otherwise the xiso will be "
				   "created in the current directory with the name <dir>.iso. The -c option may be "
				   "specified multiple times to create multiple xiso images.")
		->type_name("<dir> [name]")
		->type_size(1, 2)
		->group("Mutually exclusive modes");
	app.add_flag("-l", args.list, "List files in xiso(s).")->group("Mutually exclusive modes");
	app.add_flag("-r", args.rewrite, "Rewrite xiso(s) as optimized xiso(s).")->group("Mutually exclusive modes");
	app.add_flag("-x",
				 args.extract,
				 "Extract xiso(s) (the default mode if none is given). If no directory is specified "
				 "with -d, a directory with the name of the xiso (minus the .iso portion) will be "
				 "created next to the xiso and the xiso will be expanded there.")
		->group("Mutually exclusive modes");

	app.add_option("-d",
				   args.directory,
				   "In extract mode, expand xiso in <directory>. In rewrite mode, rewrite xiso in <directory>.")
		->type_name("<directory>")
		->group("Options");
	app.add_flag("-D", args.delete_original, "In rewrite mode, delete old xiso after processing.")->group("Options");
	app.add_flag("-m",
				 args.no_media_patch,
				 "In create or rewrite mode, disable automatic .xbe media enable patching (not "
				 "recommended).")
		->group("Options");
	app.add_flag("-q", args.quiet, "Run quiet (suppress all non-error output).")->group("Options");
	app.add_flag("-Q", args.silent, "Run silent (suppress all output).")->group("Options");
	app.add_flag("-s", args.skip_system_update, "Skip $SystemUpdate folder.")->group("Options");
}

/// The parser cannot express these rules; returns what is wrong with the combination of options, if anything.
std::optional<std::string> check(const Arguments& args)
{
	const int modes = !args.create.empty() + args.list + args.rewrite + args.extract;
	if (modes > 1) return "-c, -l, -r and -x are mutually exclusive";
	if (args.no_media_patch && (args.list || args.extract)) return "-m only applies to create and rewrite mode";
	if (args.create.empty() && args.images.empty()) return "no xiso image given";
	if (!args.create.empty() && !args.images.empty())
		return "in create mode -c takes the directory (and the name of the xiso), no other files can be given";
	return std::nullopt;
}

int run(const Arguments& args)
{
	const Logger logger(args.silent ? Verbosity::silent : args.quiet ? Verbosity::quiet : Verbosity::normal);
	const Options options{.media_enable = !args.no_media_patch, .skip_system_update = args.skip_system_update};
	const std::optional<std::filesystem::path> destination =
		args.directory ? std::optional(from_utf8(*args.directory)) : std::nullopt;

	logger.info("{}\n", banner());

	bool failed = false;
	const auto report = [&](const Error& error)
	{
		logger.error("{}\n", error.what());
		failed = true;
	};

	if (!args.create.empty())
	{
		for (const auto& group : args.create)
		{
			try
			{
				create_image(
					from_utf8(group[0]), group.size() > 1 ? std::optional(group[1]) : std::nullopt, options, logger);
			}
			catch (const Error& error)
			{
				report(error);
			}
		}
		return failed ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	Statistics total;
	for (const std::string& name : args.images)
	{
		logger.info("\n");
		const std::filesystem::path image = from_utf8(name);
		try
		{
			Statistics statistics;
			std::string shown = name;
			if (args.rewrite)
			{
				const RewriteResult result = rewrite_image(image, destination, args.delete_original, options, logger);
				if (result.skipped) continue;
				statistics = result.statistics;
				shown = to_utf8(result.output);
			}
			else if (args.list)
				statistics = list_image(image, options, logger);
			else
				statistics = extract_image(image, destination, options, logger);

			logger.info("\n{} files in {} total {} bytes\n", statistics.files, shown, statistics.bytes);
			if (args.rewrite)
				logger.info("\n{} successfully rewritten{}\n", name, destination ? std::format(" as {}", shown) : ".");
			total += statistics;
		}
		catch (const Error& error)
		{
			report(error);
		}
	}

	if (!failed && args.images.size() > 1)
		logger.info("\n{} files in {} xiso's total {} bytes\n", total.files, args.images.size(), total.bytes);

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
	Arguments args;
	CLI::App app{banner(), "extract-xiso"};
	configure(app, args);
	argv = app.ensure_utf8(argv);

	try
	{
		app.parse(argc, argv);
	}
	catch (const CLI::ParseError& error)
	{
		const int code = app.exit(error);
		return code == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (const auto problem = check(args))
	{
		std::fprintf(stderr, "%s\n\n%s", problem->c_str(), app.help().c_str());
		return EXIT_FAILURE;
	}

	return run(args);
}
