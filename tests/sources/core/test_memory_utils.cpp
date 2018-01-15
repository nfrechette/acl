////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include <catch.hpp>

#include <acl/core/memory_utils.h>

#include <cstring>

using namespace acl;

TEST_CASE("misc tests", "[core][memory]")
{
	size_t num_powers_of_two = 0;
	for (size_t i = 0; i <= 65536; ++i)
	{
		if (is_power_of_two(i))
			num_powers_of_two++;
	}

	REQUIRE(num_powers_of_two == 17);
	REQUIRE(is_power_of_two(1) == true);
	REQUIRE(is_power_of_two(2) == true);
	REQUIRE(is_power_of_two(4) == true);
	REQUIRE(is_power_of_two(8) == true);
	REQUIRE(is_power_of_two(16) == true);
	REQUIRE(is_power_of_two(32) == true);
	REQUIRE(is_power_of_two(64) == true);
	REQUIRE(is_power_of_two(128) == true);
	REQUIRE(is_power_of_two(256) == true);
	REQUIRE(is_power_of_two(512) == true);
	REQUIRE(is_power_of_two(1024) == true);
	REQUIRE(is_power_of_two(2048) == true);
	REQUIRE(is_power_of_two(4096) == true);
	REQUIRE(is_power_of_two(8192) == true);
	REQUIRE(is_power_of_two(16384) == true);
	REQUIRE(is_power_of_two(32768) == true);
	REQUIRE(is_power_of_two(65536) == true);

	REQUIRE(is_alignment_valid<int32_t>(0) == false);
	REQUIRE(is_alignment_valid<int32_t>(4) == true);
	REQUIRE(is_alignment_valid<int32_t>(8) == true);
	REQUIRE(is_alignment_valid<int32_t>(2) == false);
	REQUIRE(is_alignment_valid<int32_t>(5) == false);
	REQUIRE(is_alignment_valid<int64_t>(8) == true);
	REQUIRE(is_alignment_valid<int64_t>(16) == true);

	struct alignas(8) Tmp
	{
		int32_t padding;	// Aligned to 8 bytes
		int32_t value;		// Aligned to 4 bytes
	};
	Tmp tmp;
	REQUIRE(is_aligned_to(&tmp.padding, 8) == true);
	REQUIRE(is_aligned_to(&tmp.value, 4) == true);
	REQUIRE(is_aligned_to(&tmp.value, 2) == true);
	REQUIRE(is_aligned_to(&tmp.value, 1) == true);
	REQUIRE(is_aligned_to(&tmp.value, 8) == false);

	REQUIRE(is_aligned_to(4, 4) == true);
	REQUIRE(is_aligned_to(4, 2) == true);
	REQUIRE(is_aligned_to(4, 1) == true);
	REQUIRE(is_aligned_to(4, 8) == false);
	REQUIRE(is_aligned_to(6, 4) == false);
	REQUIRE(is_aligned_to(6, 2) == true);
	REQUIRE(is_aligned_to(6, 1) == true);

	REQUIRE(is_aligned_to(align_to(5, 4), 4) == true);
	REQUIRE(align_to(5, 4) == 8);
	REQUIRE(is_aligned_to(align_to(8, 4), 4) == true);
	REQUIRE(align_to(8, 4) == 8);

	int32_t array[8];
	REQUIRE(get_array_size(array) == (sizeof(array) / sizeof(array[0])));
}

TEST_CASE("raw memory support", "[core][memory]")
{
	uint8_t buffer[1024];
	uint8_t* ptr = &buffer[32];
	REQUIRE(add_offset_to_ptr<uint8_t>(ptr, 23) == ptr + 23);
	REQUIRE(add_offset_to_ptr<uint8_t>(ptr, 64) == ptr + 64);

	uint16_t value16 = 0x04FE;
	REQUIRE(byte_swap(value16) == 0xFE04);

	uint32_t value32 = 0x04FE78AB;
	REQUIRE(byte_swap(value32) == 0xAB78FE04);

	uint64_t value64 = uint64_t(0x04FE78AB0098DC56ull);
	REQUIRE(byte_swap(value64) == uint64_t(0x56DC9800AB78FE04ull));

	uint8_t unaligned_value_buffer[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	std::memcpy(&unaligned_value_buffer[1], &value32, sizeof(uint32_t));
	REQUIRE(unaligned_load<uint32_t>(&unaligned_value_buffer[1]) == value32);
}

TEST_CASE("memcpy_bits", "[core][memory]")
{
	uint64_t dest = uint64_t(~0ull);
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
	src = uint64_t(~0ull);
	memcpy_bits(&dest, 1, &src, 0, 7);
	REQUIRE(dest == byte_swap(uint64_t(0x7F00000000000000ull)));

	memcpy_bits(&dest, 8, &src, 0, 8);
	REQUIRE(dest == byte_swap(uint64_t(0x7FFF000000000000ull)));

	memcpy_bits(&dest, 0, &src, 0, 64);
	REQUIRE(dest == uint64_t(~0ull));
}
