////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
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

#include <acl/core/bit_manip_utils.h>

//#include <cstring>

using namespace acl;

TEST_CASE("bit_manip_utils", "[core][utils]")
{
	REQUIRE(count_set_bits(uint8_t(0x00)) == 0);
	REQUIRE(count_set_bits(uint8_t(0x01)) == 1);
	REQUIRE(count_set_bits(uint8_t(0x10)) == 1);
	REQUIRE(count_set_bits(uint8_t(0xFF)) == 8);

	REQUIRE(count_set_bits(uint16_t(0x0000)) == 0);
	REQUIRE(count_set_bits(uint16_t(0x0001)) == 1);
	REQUIRE(count_set_bits(uint16_t(0x1000)) == 1);
	REQUIRE(count_set_bits(uint16_t(0xFFFF)) == 16);

	REQUIRE(count_set_bits(uint32_t(0x00000000)) == 0);
	REQUIRE(count_set_bits(uint32_t(0x00000001)) == 1);
	REQUIRE(count_set_bits(uint32_t(0x10000000)) == 1);
	REQUIRE(count_set_bits(uint32_t(0xFFFFFFFF)) == 32);

	REQUIRE(count_set_bits(uint64_t(0x0000000000000000ull)) == 0);
	REQUIRE(count_set_bits(uint64_t(0x0000000000000001ull)) == 1);
	REQUIRE(count_set_bits(uint64_t(0x1000000000000000ull)) == 1);
	REQUIRE(count_set_bits(uint64_t(0xFFFFFFFFFFFFFFFFull)) == 64);

	REQUIRE(rotate_bits_left(0x00000010, 0) == 0x00000010);
	REQUIRE(rotate_bits_left(0x10000010, 1) == 0x20000020);
	REQUIRE(rotate_bits_left(0x10000010, 2) == 0x40000040);
	REQUIRE(rotate_bits_left(0x10000010, 3) == 0x80000080);
	REQUIRE(rotate_bits_left(0x10000010, 4) == 0x00000101);

	REQUIRE(and_not(0x00000010, 0x10101011) == 0x10101001);
}
