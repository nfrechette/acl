#include <catch.hpp>

#include <acl/core/memory.h>

using namespace acl;

TEST_CASE("memcpy_bits", "[core][memory]")
{
	uint64_t dest = byte_swap(~0ull);
	uint64_t src = byte_swap(0x5555555555555555ull);
	memcpy_bits(&dest, 1, &src, 0, 64 - 3);
	REQUIRE(dest == byte_swap(0xAAAAAAAAAAAAAAABull));

	dest = byte_swap(0x0F00FF0000000000ull);
	src = byte_swap(0x3800000000000000ull);
	memcpy_bits(&dest, 0, &src, 2, 5);
	REQUIRE(dest == byte_swap(0xE700FF0000000000ull));

	dest = byte_swap(0x0F00FF0000000000ull);
	src = byte_swap(0x3800000000000000ull);
	memcpy_bits(&dest, 1, &src, 2, 5);
	REQUIRE(dest == byte_swap(0x7300FF0000000000ull));
}
