// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <cstdio>
#include <format>
#include <string>
#include <utility>

namespace xiso
{

enum class Verbosity
{
	normal, ///< progress on stdout, errors on stderr
	quiet,	///< errors only
	silent	///< nothing at all
};

/// Console output with the three verbosity levels of the command line tool.
class Logger
{
public:
	explicit Logger(Verbosity verbosity = Verbosity::normal) noexcept : verbosity_(verbosity) {}

	template <typename... Args> void info(std::format_string<Args...> format, Args&&... args) const
	{
		if (verbosity_ == Verbosity::normal) write(stdout, std::format(format, std::forward<Args>(args)...));
	}

	template <typename... Args> void error(std::format_string<Args...> format, Args&&... args) const
	{
		if (verbosity_ != Verbosity::silent) write(stderr, std::format(format, std::forward<Args>(args)...));
	}

	/// Flushes stdout so that partial lines (progress output) show up immediately.
	void flush() const
	{
		if (verbosity_ == Verbosity::normal) std::fflush(stdout);
	}

	[[nodiscard]] Verbosity verbosity() const noexcept { return verbosity_; }

private:
	static void write(std::FILE* stream, const std::string& text) { std::fwrite(text.data(), 1, text.size(), stream); }

	Verbosity verbosity_;
};

} // namespace xiso
