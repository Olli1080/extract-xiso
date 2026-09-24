// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#include "xiso/entry.hpp"

#include <algorithm>
#include <cstddef>

namespace xiso
{

namespace
{

constexpr unsigned char to_upper(char c) noexcept
{
	const auto byte = static_cast<unsigned char>(c);
	return byte >= 'a' && byte <= 'z' ? static_cast<unsigned char>(byte - 'a' + 'A') : byte;
}

} // namespace

std::strong_ordering compare_names(std::string_view lhs, std::string_view rhs) noexcept
{
	const std::size_t common = std::min(lhs.size(), rhs.size());
	for (std::size_t i = 0; i < common; ++i)
	{
		const unsigned char a = to_upper(lhs[i]);
		const unsigned char b = to_upper(rhs[i]);
		if (a != b) return a <=> b;
	}
	return lhs.size() <=> rhs.size();
}

bool ends_with_ignore_case(std::string_view name, std::string_view suffix) noexcept
{
	return name.size() >= suffix.size() && compare_names(name.substr(name.size() - suffix.size()), suffix) == 0;
}

} // namespace xiso
