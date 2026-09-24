#include "test_support.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>

namespace test
{

namespace fs = std::filesystem;

TempDir::TempDir()
{
	static std::atomic<unsigned> counter{0};
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	path_ = fs::temp_directory_path() / ("xiso-test-" + std::to_string(stamp) + "-" + std::to_string(counter++));
	fs::create_directories(path_);
}

TempDir::~TempDir()
{
	std::error_code ignored;
	fs::remove_all(path_, ignored);
}

Bytes random_bytes(std::size_t size, unsigned seed)
{
	// xorshift64*: plenty random for test data and fast even in debug builds
	std::uint64_t state = 0x9E3779B97F4A7C15ull ^ (static_cast<std::uint64_t>(seed) * 0xD1B54A32D192ED03ull);
	Bytes bytes(size);
	for (std::size_t i = 0; i < size; i += sizeof(std::uint64_t))
	{
		state ^= state >> 12;
		state ^= state << 25;
		state ^= state >> 27;
		const std::uint64_t value = state * 0x2545F4914F6CDD1Dull;
		for (std::size_t j = 0; j < sizeof(value) && i + j < size; ++j)
			bytes[i + j] = static_cast<std::byte>(value >> (8 * j));
	}
	return bytes;
}

Bytes bytes_of(const std::string& text)
{
	const auto* data = reinterpret_cast<const std::byte*>(text.data());
	return Bytes(data, data + text.size());
}

void write_file(const fs::path& path, const Bytes& content)
{
	fs::create_directories(path.parent_path());
	std::ofstream out(path, std::ios::binary);
	out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
}

Bytes read_file(const fs::path& path)
{
	std::ifstream in(path, std::ios::binary);
	Bytes content(static_cast<std::size_t>(fs::file_size(path)));
	in.read(reinterpret_cast<char*>(content.data()), static_cast<std::streamsize>(content.size()));
	return content;
}

std::map<std::string, Bytes> snapshot(const fs::path& root)
{
	std::map<std::string, Bytes> result;
	for (const auto& item : fs::recursive_directory_iterator(root))
	{
		std::string relative = item.path().lexically_relative(root).generic_string();
		if (item.is_directory())
			result[relative + "/"] = {};
		else
			result[relative] = read_file(item.path());
	}
	return result;
}

std::size_t find_bytes(const Bytes& haystack, const Bytes& needle)
{
	const auto found = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end());
	return found == haystack.end() ? npos : static_cast<std::size_t>(found - haystack.begin());
}

} // namespace test
