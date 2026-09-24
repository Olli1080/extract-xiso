#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "xiso/format.hpp"
#include "xiso/media_patch.hpp"

namespace
{

using xiso::format::media_check_pattern;

std::vector<std::byte> filler(std::size_t size) { return std::vector<std::byte>(size, std::byte{0x11}); }

void put_pattern(std::vector<std::byte>& data, std::size_t offset)
{
	std::ranges::copy(media_check_pattern, data.begin() + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

TEST(MediaPatch, PatchesTheLastByteOfEachMatch)
{
	auto data = filler(100);
	put_pattern(data, 10);
	put_pattern(data, 60);

	EXPECT_EQ(xiso::patch_media_check(data), 2u);
	EXPECT_EQ(data[17], std::byte{0xEB});
	EXPECT_EQ(data[67], std::byte{0xEB});
	EXPECT_EQ(data[16], media_check_pattern[6]); // everything else untouched
}

TEST(MediaPatch, NoMatchLeavesDataAlone)
{
	auto data = filler(64);
	const auto copy = data;
	EXPECT_EQ(xiso::patch_media_check(data), 0u);
	EXPECT_EQ(data, copy);
}

TEST(MediaPatch, MatchAtTheVeryStartAndEnd)
{
	auto data = filler(8 + 20 + 8);
	put_pattern(data, 0);
	put_pattern(data, 28);
	EXPECT_EQ(xiso::patch_media_check(data), 2u);
}

TEST(MediaPatch, BackToBackMatches)
{
	auto data = filler(16);
	put_pattern(data, 0);
	put_pattern(data, 8);
	EXPECT_EQ(xiso::patch_media_check(data), 2u);
}

TEST(MediaPatch, BufferShorterThanPattern)
{
	std::array<std::byte, 5> data{};
	EXPECT_EQ(xiso::patch_media_check(data), 0u);
	EXPECT_EQ(xiso::patch_media_check({}), 0u);
}
