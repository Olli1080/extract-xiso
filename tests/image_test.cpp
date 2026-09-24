#include <algorithm>
#include <filesystem>
#include <format>
#include <string>

#include <gtest/gtest.h>

#include "test_support.hpp"
#include "xiso/endian.hpp"
#include "xiso/entry.hpp"
#include "xiso/error.hpp"
#include "xiso/format.hpp"
#include "xiso/image_reader.hpp"
#include "xiso/logger.hpp"
#include "xiso/media_patch.hpp"
#include "xiso/operations.hpp"
#include "xiso/paths.hpp"

namespace
{

namespace fs = std::filesystem;
using namespace test;

/// Creates a game-like directory tree and helpers to make images out of it.
class ImageTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		source = dir / "game";

		auto pattern = Bytes(xiso::format::media_check_pattern.begin(), xiso::format::media_check_pattern.end());
		auto small_xbe = random_bytes(3000, 1);
		small_xbe.insert(small_xbe.begin() + 100, pattern.begin(), pattern.end());
		write_file(source / "default.xbe", small_xbe);

		// A pattern that starts 3 bytes before the end of the first 2 MiB copy chunk.
		auto big_xbe = random_bytes(2 * 1024 * 1024 + 4321, 2);
		std::ranges::copy(pattern, big_xbe.begin() + (2 * 1024 * 1024 - 3));
		write_file(source / "Big.XBE", big_xbe);

		write_file(source / "readme.txt", bytes_of("hello xbox"));
		write_file(source / "zero.bin", {});
		write_file(source / "exact.bin", random_bytes(4096, 3));
		write_file(source / "media" / "Video.BIK", random_bytes(5000, 4));
		write_file(source / "media" / "deep" / "leaf.dat", random_bytes(1, 5));
		fs::create_directories(source / "empty");
		fs::create_directories(source / "nested" / "also-empty");
		for (int i = 0; i < 300; ++i) // enough entries for a directory table of several sectors
			write_file(source / "many" / std::format("file_{:04}.dat", (i * 7919) % 300),
					   random_bytes(10 + i, 100 + i));
	}

	/// What the source tree should look like after a trip through an image.
	[[nodiscard]] std::map<std::string, Bytes> expected(bool patch_media = true) const
	{
		auto files = snapshot(source);
		if (patch_media)
			for (auto& [name, content] : files)
				if (xiso::ends_with_ignore_case(name, ".xbe")) xiso::patch_media_check(content);
		return files;
	}

	[[nodiscard]] xiso::Statistics totals() const
	{
		xiso::Statistics result;
		for (const auto& [name, content] : snapshot(source))
			if (!name.ends_with('/'))
			{
				++result.files;
				result.bytes += content.size();
			}
		return result;
	}

	fs::path create(const std::string& name = "game.iso")
	{
		const fs::path image = dir / name;
		xiso::create_image(source, image.string(), options, logger);
		return image;
	}

	fs::path extract(const fs::path& image, const std::string& into = "extracted")
	{
		xiso::extract_image(image, dir / into, options, logger);
		return dir / into;
	}

	TempDir dir;
	fs::path source;
	xiso::Logger logger{xiso::Verbosity::silent};
	xiso::Options options;
};

/// Makes an image look like one written by another tool, i.e. not optimized.
void clear_optimized_tag(const fs::path& image)
{
	auto content = read_file(image);
	std::fill_n(content.begin() + xiso::format::optimized_tag_offset, xiso::format::optimized_tag_size, std::byte{0});
	write_file(image, content);
}

} // namespace

TEST_F(ImageTest, CreatedImageHasTheXisoStructure)
{
	const fs::path image = create();
	const Bytes content = read_file(image);

	ASSERT_GT(content.size(), 0x20000u);
	EXPECT_EQ(content.size() % xiso::format::file_modulus, 0u);

	auto text_at = [&](std::size_t offset, std::size_t length)
	{ return std::string(reinterpret_cast<const char*>(content.data()) + offset, length); };
	EXPECT_EQ(text_at(0x10000, 20), "MICROSOFT*XBOX*MEDIA");
	EXPECT_EQ(text_at(0x10000 + 20 + 8 + 8 + 0x7c8, 20), "MICROSOFT*XBOX*MEDIA");
	EXPECT_EQ(text_at(0x8001, 5), "CD001");
	EXPECT_EQ(text_at(0x8800 + 1, 5), "CD001");
	EXPECT_EQ(text_at(xiso::format::optimized_tag_offset, 8), "in!xiso!");

	const auto root_sector = xiso::load_le<std::uint32_t>(std::span(content).subspan(0x10000 + 20, 4));
	EXPECT_EQ(root_sector, xiso::format::root_directory_sector);

	EXPECT_TRUE(xiso::is_optimized(image));
}

TEST_F(ImageTest, ListCountsFilesAndBytes)
{
	const fs::path image = create();
	const auto stats = xiso::list_image(image, options, logger);

	EXPECT_EQ(stats.files, totals().files);
	EXPECT_EQ(stats.bytes, totals().bytes);
}

TEST_F(ImageTest, ExtractRestoresTheTreeIncludingEmptyDirectories)
{
	const fs::path image = create();
	const auto stats = xiso::extract_image(image, dir / "extracted", options, logger);

	EXPECT_EQ(stats.files, totals().files);
	EXPECT_EQ(snapshot(dir / "extracted"), expected());
}

TEST_F(ImageTest, ExtractWithoutDestinationGoesNextToTheImage)
{
	fs::create_directories(dir / "images");
	const fs::path image = create("images/game.iso");

	xiso::extract_image(image, std::nullopt, options, logger);

	EXPECT_EQ(snapshot(dir / "images" / "game"), expected());
}

TEST_F(ImageTest, ExtractsIntoPathsLongerThanMaxPath)
{
	const fs::path image = create();
	const fs::path deep = dir / std::string(90, 'd') / std::string(90, 'e') / std::string(90, 'f');

	xiso::extract_image(image, deep, options, logger);

	ASSERT_GT(deep.native().size(), 260u);
	EXPECT_EQ(snapshot(xiso::long_path(deep)), expected());
}

TEST_F(ImageTest, MediaPatchCanBeDisabled)
{
	options.media_enable = false;
	const fs::path image = create();
	extract(image);

	EXPECT_EQ(snapshot(dir / "extracted"), expected(false));
}

TEST_F(ImageTest, MediaPatchOnlyTouchesXbeFiles)
{
	auto pattern = Bytes(xiso::format::media_check_pattern.begin(), xiso::format::media_check_pattern.end());
	write_file(source / "data.bin", pattern);
	const fs::path image = create();
	extract(image);

	EXPECT_EQ(read_file(dir / "extracted" / "data.bin"), pattern);
}

TEST_F(ImageTest, SkipSystemUpdateFolder)
{
	write_file(source / "$SystemUpdate" / "update.xbe", random_bytes(100, 9));
	write_file(source / "$SystemUpdate" / "inner" / "file.bin", random_bytes(100, 10));
	const fs::path image = create();

	xiso::extract_image(image, dir / "with", options, logger);
	EXPECT_TRUE(fs::exists(dir / "with" / "$SystemUpdate" / "update.xbe"));

	options.skip_system_update = true;
	const auto stats = xiso::extract_image(image, dir / "without", options, logger);
	EXPECT_FALSE(fs::exists(dir / "without" / "$SystemUpdate"));
	EXPECT_EQ(stats.files, totals().files - 2);
}

TEST_F(ImageTest, RewriteProducesAnOptimizedImageWithTheSameContent)
{
	const fs::path image = create();
	clear_optimized_tag(image);
	ASSERT_FALSE(xiso::is_optimized(image));

	fs::create_directories(dir / "rewritten");
	const auto result = xiso::rewrite_image(image, dir / "rewritten", false, options, logger);

	ASSERT_FALSE(result.skipped);
	EXPECT_EQ(result.output, dir / "rewritten" / "game.iso");
	EXPECT_EQ(result.statistics.files, totals().files);
	EXPECT_TRUE(xiso::is_optimized(result.output));
	EXPECT_TRUE(fs::exists(dir / "game.iso.old"));
	EXPECT_FALSE(fs::exists(image));

	xiso::extract_image(result.output, dir / "extracted", options, logger);
	EXPECT_EQ(snapshot(dir / "extracted"), expected());
}

TEST_F(ImageTest, RewriteCanDeleteTheOriginal)
{
	const fs::path image = create();
	clear_optimized_tag(image);

	fs::create_directories(dir / "rewritten");
	xiso::rewrite_image(image, dir / "rewritten", true, options, logger);

	EXPECT_FALSE(fs::exists(dir / "game.iso.old"));
}

TEST_F(ImageTest, RewriteSkipsOptimizedImages)
{
	const fs::path image = create();
	const auto result = xiso::rewrite_image(image, dir.path(), false, options, logger);

	EXPECT_TRUE(result.skipped);
	EXPECT_TRUE(fs::exists(image));
	EXPECT_FALSE(fs::exists(dir / "game.iso.old"));
}

TEST_F(ImageTest, RewriteFailureRestoresTheOriginal)
{
	const fs::path image = create();
	clear_optimized_tag(image);
	const Bytes before = read_file(image);

	EXPECT_THROW(xiso::rewrite_image(image, dir / "no-such-directory", false, options, logger), xiso::Error);

	EXPECT_TRUE(fs::exists(image));
	EXPECT_FALSE(fs::exists(dir / "game.iso.old"));
	EXPECT_EQ(read_file(image), before);
}

TEST_F(ImageTest, RewriteRefusesToOverwriteAnOldFile)
{
	const fs::path image = create();
	clear_optimized_tag(image);
	write_file(dir / "game.iso.old", bytes_of("precious"));

	EXPECT_THROW(xiso::rewrite_image(image, dir.path(), false, options, logger), xiso::Error);
	EXPECT_EQ(read_file(dir / "game.iso.old"), bytes_of("precious"));
	EXPECT_TRUE(fs::exists(image));
}

TEST_F(ImageTest, RewriteLeavesFilesThatAreNoImagesAlone)
{
	write_file(dir / "junk.iso", bytes_of("this is not an xbox image"));

	EXPECT_THROW(xiso::rewrite_image(dir / "junk.iso", dir.path(), false, options, logger), xiso::Error);
	EXPECT_TRUE(fs::exists(dir / "junk.iso"));
	EXPECT_FALSE(fs::exists(dir / "junk.iso.old"));
}

TEST_F(ImageTest, ImagesFromOtherPartitionOffsetsAreRecognized)
{
	// Full-disc dumps carry the file system at a fixed offset (XGD3 here).
	constexpr std::uint64_t offset = 0x02080000;
	const fs::path image = create();
	const Bytes plain = read_file(image);

	const fs::path shifted = dir / "shifted.iso";
	{
		Bytes disc(offset + plain.size());
		std::ranges::copy(plain, disc.begin() + offset);
		write_file(shifted, disc);
	}

	const auto stats = xiso::extract_image(shifted, dir / "extracted", options, logger);
	EXPECT_EQ(stats.files, totals().files);
	EXPECT_EQ(snapshot(dir / "extracted"), expected());
}

TEST_F(ImageTest, InvalidImagesAreRejected)
{
	write_file(dir / "junk.iso", bytes_of("this is not an xbox image"));
	write_file(dir / "empty.iso", {});

	EXPECT_THROW(xiso::list_image(dir / "junk.iso", options, logger), xiso::Error);
	EXPECT_THROW(xiso::list_image(dir / "empty.iso", options, logger), xiso::Error);
	EXPECT_THROW(xiso::list_image(dir / "missing.iso", options, logger), xiso::Error);
}

TEST_F(ImageTest, CreateFromMissingDirectoryFails)
{
	EXPECT_THROW(xiso::create_image(dir / "nope", (dir / "x.iso").string(), options, logger), xiso::Error);
	EXPECT_FALSE(fs::exists(dir / "x.iso"));
}

TEST_F(ImageTest, FailedCreateLeavesNoPartialImage)
{
	EXPECT_THROW(xiso::create_image(source, (dir / "missing-dir" / "x.iso").string(), options, logger), xiso::Error);
	EXPECT_FALSE(fs::exists(dir / "missing-dir" / "x.iso"));
}

TEST_F(ImageTest, ImageIsNamedAfterTheDirectoryInTheGivenLocation)
{
	fs::create_directories(dir / "out");
	xiso::create_image(source, (dir / "out").generic_string() + "/", options, logger);
	EXPECT_TRUE(fs::exists(dir / "out" / "game.iso"));
}

TEST_F(ImageTest, CaseInsensitiveDuplicatesAreRejected)
{
	write_file(source / "Dup.txt", bytes_of("1"));
	write_file(source / "dup.txt", bytes_of("2"));

	std::size_t found = 0;
	for (const auto& item : fs::directory_iterator(source))
		found += xiso::compare_names(xiso::to_utf8(item.path().filename()), "dup.txt") == 0;
	if (found < 2) GTEST_SKIP() << "file system is case insensitive";

	EXPECT_THROW(create(), xiso::Error);
	EXPECT_FALSE(fs::exists(dir / "game.iso"));
}

TEST(ImageNames, StripTheIsoExtension)
{
	EXPECT_EQ(xiso::image_names("halo.iso").base_name, "halo");
	EXPECT_EQ(xiso::image_names("some/dir/HALO.ISO").base_name, "HALO");
	EXPECT_EQ(xiso::image_names("some/dir/HALO.ISO").file_name, "HALO.ISO");
	EXPECT_EQ(xiso::image_names("game.xiso").base_name, "game.xiso");
	EXPECT_EQ(xiso::image_names(".iso").base_name, ".iso");
}

// ---- damaged and hostile images -------------------------------------------------------------------------------

class HostileImageTest : public ::testing::Test
{
protected:
	/// An image with one file called \p name (which has to be 7 characters long), whose name is then overwritten.
	fs::path image_with_name(const std::string& replacement)
	{
		write_file(dir / "src" / "QQQQQQQ", bytes_of("hi"));
		const fs::path image = dir / "evil.iso";
		xiso::create_image(dir / "src", image.string(), options, logger);

		Bytes content = read_file(image);
		const std::size_t at = find_bytes(content, bytes_of("QQQQQQQ"));
		EXPECT_NE(at, npos);
		std::ranges::copy(bytes_of(replacement), content.begin() + static_cast<std::ptrdiff_t>(at));
		write_file(image, content);
		return image;
	}

	TempDir dir;
	xiso::Logger logger{xiso::Verbosity::silent};
	xiso::Options options;
};

TEST_F(HostileImageTest, PathTraversalIsRefused)
{
	const fs::path image = image_with_name("../evil");

	EXPECT_THROW(xiso::extract_image(image, dir / "out", options, logger), xiso::Error);
	EXPECT_FALSE(fs::exists(dir / "evil"));
	EXPECT_THROW(xiso::list_image(image, options, logger), xiso::Error);
}

TEST_F(HostileImageTest, SeparatorsInNamesAreRefused)
{
	EXPECT_THROW(xiso::list_image(image_with_name("a\\b.txt"), options, logger), xiso::Error);
}

TEST_F(HostileImageTest, DotAndDotDotAreRefused)
{
	// Names are C strings on disk, so trailing NULs just shorten them.
	EXPECT_THROW(xiso::list_image(image_with_name(std::string("..\0\0\0\0\0", 7)), options, logger), xiso::Error);
}

TEST_F(HostileImageTest, EmptyNamesAreRefused)
{
	EXPECT_THROW(xiso::list_image(image_with_name(std::string(7, '\0')), options, logger), xiso::Error);
}

TEST_F(HostileImageTest, NamesMadeOfDotsAreFine)
{
	EXPECT_NO_THROW(xiso::list_image(image_with_name("......."), options, logger));
}

TEST_F(HostileImageTest, DirectoryLoopsAreDetected)
{
	write_file(dir / "src" / "sub" / "f.txt", bytes_of("x"));
	const fs::path image = dir / "loop.iso";
	xiso::create_image(dir / "src", image.string(), options, logger);

	// The root table holds a single entry, "sub". Point its sector at the root table itself.
	Bytes content = read_file(image);
	const std::size_t entry = static_cast<std::size_t>(xiso::format::root_directory_sector) * xiso::format::sector_size;
	xiso::store_le<std::uint32_t>(std::span(content).subspan(entry + 4, 4), xiso::format::root_directory_sector);
	write_file(image, content);

	EXPECT_THROW(xiso::list_image(image, options, logger), xiso::Error);
}

TEST_F(HostileImageTest, TruncatedImagesExtractWhatIsThere)
{
	write_file(dir / "src" / "big.bin", random_bytes(100000, 7));
	const fs::path image = dir / "cut.iso";
	xiso::create_image(dir / "src", image.string(), options, logger);

	// Root table in sector 0x108, the file follows in the next sector. Keep 50000 of its bytes.
	const std::uint64_t data =
		static_cast<std::uint64_t>(xiso::format::root_directory_sector + 1) * xiso::format::sector_size;
	fs::resize_file(image, data + 50000);

	const auto stats = xiso::extract_image(image, dir / "out", options, logger);
	EXPECT_EQ(stats.bytes, 50000u);
	EXPECT_EQ(fs::file_size(dir / "out" / "big.bin"), 50000u);
}
