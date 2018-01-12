#include <catch.hpp>

#include <acl/core/memory_utils.h>

using namespace acl;

TEST_CASE("memcpy_bits", "[core][memory]")
{
	uint64_t dest = ~0ull;
	uint64_t src = byte_swap(uint64_t(0x5555555555555555ull));
	memcpy_bits(&dest, 1, &src, 0, 64 - 3);
	REQUIRE(dest == byte_swap(uint64_t(0xAAAAAAAAAAAAAAABull)));

	dest = byte_swap(uint64_t(0x0F00FF0000000000ull));
	src = byte_swap(uint64_t(0x3800000000000000ull));
	memcpy_bits(&dest, 0, &src, 2, 5);
	REQUIRE(dest == byte_swap(uint64_t(0xE700FF0000000000ull)));

	dest = byte_swap(uint64_t(0x0F00FF0000000000ull));
	src = byte_swap(uint64_t(0x3800000000000000ull));
	memcpy_bits(&dest, 1, &src, 2, 5);
	REQUIRE(dest == byte_swap(uint64_t(0x7300FF0000000000ull)));

	dest = 0;
	src = ~0ull;
	memcpy_bits(&dest, 1, &src, 0, 7);
	REQUIRE(dest == byte_swap(uint64_t(0x7F00000000000000ull)));

	memcpy_bits(&dest, 8, &src, 0, 8);
	REQUIRE(dest == byte_swap(uint64_t(0x7FFF000000000000ull)));

	memcpy_bits(&dest, 0, &src, 0, 64);
	REQUIRE(dest == ~0ull);
}
