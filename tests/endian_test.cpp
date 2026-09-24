#include <array>
#include <cstddef>

#include <gtest/gtest.h>

#include "xiso/endian.hpp"

TEST(Endian, LoadLittleEndian)
{
	const std::array<std::byte, 4> bytes{std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12}};
	EXPECT_EQ(xiso::load_le<std::uint32_t>(bytes), 0x12345678u);
	EXPECT_EQ(xiso::load_le<std::uint16_t>(bytes), 0x5678u);
}

TEST(Endian, StoreLittleAndBigEndian)
{
	std::array<std::byte, 4> bytes{};
	xiso::store_le<std::uint32_t>(bytes, 0x12345678u);
	EXPECT_EQ(bytes[0], std::byte{0x78});
	EXPECT_EQ(bytes[3], std::byte{0x12});

	xiso::store_be<std::uint32_t>(bytes, 0x12345678u);
	EXPECT_EQ(bytes[0], std::byte{0x12});
	EXPECT_EQ(bytes[3], std::byte{0x78});
}

TEST(Endian, RoundTrip)
{
	std::array<std::byte, 8> bytes{};
	xiso::store_le<std::uint64_t>(bytes, 0x0123456789ABCDEFull);
	EXPECT_EQ(xiso::load_le<std::uint64_t>(bytes), 0x0123456789ABCDEFull);
}
