#include <gtest/gtest.h>

#include "xiso/entry.hpp"

using xiso::compare_names;

TEST(CompareNames, IgnoresAsciiCase)
{
	EXPECT_EQ(compare_names("Default.XBE", "default.xbe"), std::strong_ordering::equal);
	EXPECT_EQ(compare_names("apple", "BANANA"), std::strong_ordering::less);
	EXPECT_EQ(compare_names("Banana", "apple"), std::strong_ordering::greater);
}

TEST(CompareNames, ShorterPrefixSortsFirst)
{
	EXPECT_EQ(compare_names("file", "file2"), std::strong_ordering::less);
	EXPECT_EQ(compare_names("file2", "file"), std::strong_ordering::greater);
	EXPECT_EQ(compare_names("", "a"), std::strong_ordering::less);
}

TEST(CompareNames, HighBytesSortAfterAscii)
{
	// Compared as unsigned bytes, the same on every platform.
	EXPECT_EQ(compare_names("\xE4", "z"), std::strong_ordering::greater);
}

TEST(CompareNames, OnlyAsciiLettersAreFolded)
{
	EXPECT_NE(compare_names("\xC4", "\xE4"), std::strong_ordering::equal);
	EXPECT_EQ(compare_names("_", "a"),
			  std::strong_ordering::greater); // '_' (0x5F) sorts after 'A' (0x41), not before 'a' (0x61)
}

TEST(EndsWithIgnoreCase, Basic)
{
	EXPECT_TRUE(xiso::ends_with_ignore_case("default.XBE", ".xbe"));
	EXPECT_FALSE(xiso::ends_with_ignore_case("xbe", ".xbe"));
	EXPECT_FALSE(xiso::ends_with_ignore_case("default.xbex", ".xbe"));
}

TEST(EntryTree, TreatsNamesDifferingInCaseAsDuplicates)
{
	xiso::EntryTree tree;
	xiso::Entry first;
	first.name = "Default.xbe";
	xiso::Entry second;
	second.name = "DEFAULT.XBE";
	xiso::Entry third;
	third.name = "other.bin";

	EXPECT_NE(tree.insert(std::move(first)), nullptr);
	EXPECT_EQ(tree.insert(std::move(second)), nullptr);
	EXPECT_NE(tree.insert(std::move(third)), nullptr);
}
