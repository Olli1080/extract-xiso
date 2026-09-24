// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/error.hpp"

#include <cerrno>
#include <system_error>

namespace xiso
{

std::string last_system_error()
{
	const int code = errno;
	if (code == 0) return "unexpected end of file";
	return std::generic_category().message(code);
}

} // namespace xiso
