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

#include <catch2/catch.hpp>

#include <acl/core/bit_manip_utils.h>

using namespace acl;

TEST_CASE("bit_manip_utils", "[core][utils]")
{
	CHECK(count_set_bits(uint8_t(0x00)) == 0);
	CHECK(count_set_bits(uint8_t(0x01)) == 1);
	CHECK(count_set_bits(uint8_t(0x10)) == 1);
	CHECK(count_set_bits(uint8_t(0xFF)) == 8);

	CHECK(count_set_bits(uint16_t(0x0000)) == 0);
	CHECK(count_set_bits(uint16_t(0x0001)) == 1);
	CHECK(count_set_bits(uint16_t(0x1000)) == 1);
	CHECK(count_set_bits(uint16_t(0x1001)) == 2);
	CHECK(count_set_bits(uint16_t(0xFFFF)) == 16);

	CHECK(count_set_bits(uint32_t(0x00000000)) == 0);
	CHECK(count_set_bits(uint32_t(0x00000001)) == 1);
	CHECK(count_set_bits(uint32_t(0x10000000)) == 1);
	CHECK(count_set_bits(uint32_t(0x10101001)) == 4);
	CHECK(count_set_bits(uint32_t(0xFFFFFFFF)) == 32);

	CHECK(count_set_bits(uint64_t(0x0000000000000000ULL)) == 0);
	CHECK(count_set_bits(uint64_t(0x0000000000000001ULL)) == 1);
	CHECK(count_set_bits(uint64_t(0x1000000000000000ULL)) == 1);
	CHECK(count_set_bits(uint64_t(0x1000100001010101ULL)) == 6);
	CHECK(count_set_bits(uint64_t(0xFFFFFFFFFFFFFFFFULL)) == 64);

	CHECK(count_leading_zeros(uint32_t(0x00000000)) == 32);
	CHECK(count_leading_zeros(uint32_t(0x00000001)) == 31);
	CHECK(count_leading_zeros(uint32_t(0x00000002)) == 30);
	CHECK(count_leading_zeros(uint32_t(0x80000000)) == 0);
	CHECK(count_leading_zeros(uint32_t(0x40000000)) == 1);

	CHECK(count_trailing_zeros(uint32_t(0x00000000)) == 32);
	CHECK(count_trailing_zeros(uint32_t(0x00000001)) == 0);
	CHECK(count_trailing_zeros(uint32_t(0x00000002)) == 1);
	CHECK(count_trailing_zeros(uint32_t(0x80000000)) == 31);
	CHECK(count_trailing_zeros(uint32_t(0x40000000)) == 30);

	CHECK(rotate_bits_left(0x00000010, 0) == 0x00000010);
	CHECK(rotate_bits_left(0x10000010, 1) == 0x20000020);
	CHECK(rotate_bits_left(0x10000010, 2) == 0x40000040);
	CHECK(rotate_bits_left(0x10000010, 3) == 0x80000080);
	CHECK(rotate_bits_left(0x10000010, 4) == 0x00000101);

	CHECK(and_not(0x00000010, 0x10101011) == 0x10101001);
}
