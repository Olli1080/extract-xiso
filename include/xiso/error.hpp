// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <stdexcept>
#include <string>

namespace xiso
{

/// Any failure while reading, writing or validating an image.
class Error : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

/// Text for the most recent failed system call ("unexpected end of file" if errno was not set).
[[nodiscard]] std::string last_system_error();

} // namespace xiso
