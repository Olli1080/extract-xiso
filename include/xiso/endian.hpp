// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <concepts>
#include <cstddef>
#include <span>

namespace xiso
{

/// Decodes a little endian integer from the front of \p bytes, independent of the host byte order.
template <std::unsigned_integral T> [[nodiscard]] constexpr T load_le(std::span<const std::byte> bytes) noexcept
{
	T value = 0;
	for (std::size_t i = sizeof(T); i-- > 0;) value = static_cast<T>((value << 8) | static_cast<T>(bytes[i]));
	return value;
}

/// Encodes \p value as little endian into the front of \p bytes.
template <std::unsigned_integral T> constexpr void store_le(std::span<std::byte> bytes, T value) noexcept
{
	for (std::size_t i = 0; i < sizeof(T); ++i)
	{
		bytes[i] = static_cast<std::byte>(value & 0xFF);
		value = static_cast<T>(value >> 8);
	}
}

/// Encodes \p value as big endian into the front of \p bytes.
template <std::unsigned_integral T> constexpr void store_be(std::span<std::byte> bytes, T value) noexcept
{
	for (std::size_t i = sizeof(T); i-- > 0;)
	{
		bytes[i] = static_cast<std::byte>(value & 0xFF);
		value = static_cast<T>(value >> 8);
	}
}

} // namespace xiso
