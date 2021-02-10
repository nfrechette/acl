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

#include <acl/core/bitset.h>

#include <cstring>

using namespace acl;

TEST_CASE("bitset", "[core][utils]")
{
	CHECK(bitset_description::make_from_num_bits(0).get_size() == 0);
	CHECK(bitset_description::make_from_num_bits(1).get_size() == 1);
	CHECK(bitset_description::make_from_num_bits(31).get_size() == 1);
	CHECK(bitset_description::make_from_num_bits(32).get_size() == 1);
	CHECK(bitset_description::make_from_num_bits(33).get_size() == 2);
	CHECK(bitset_description::make_from_num_bits(64).get_size() == 2);
	CHECK(bitset_description::make_from_num_bits(65).get_size() == 3);

	CHECK(bitset_description::make_from_num_bits(0).get_num_bits() == 0);
	CHECK(bitset_description::make_from_num_bits(1).get_num_bits() == 32);
	CHECK(bitset_description::make_from_num_bits(31).get_num_bits() == 32);
	CHECK(bitset_description::make_from_num_bits(32).get_num_bits() == 32);
	CHECK(bitset_description::make_from_num_bits(33).get_num_bits() == 64);
	CHECK(bitset_description::make_from_num_bits(64).get_num_bits() == 64);
	CHECK(bitset_description::make_from_num_bits(65).get_num_bits() == 96);

	constexpr bitset_description desc = bitset_description::make_from_num_bits<64>();
	CHECK(desc.get_size() == 2);
	CHECK(desc.get_size() == bitset_description::make_from_num_bits(64).get_size());

	uint32_t bitset_data[desc.get_size() + 1];	// Add padding
	std::memset(&bitset_data[0], 0, sizeof(bitset_data));

	bitset_reset(&bitset_data[0], desc, true);
	CHECK(bitset_data[0] == 0xFFFFFFFF);
	CHECK(bitset_data[1] == 0xFFFFFFFF);
	CHECK(bitset_data[2] == 0);

	bitset_data[2] = 0xFFFFFFFF;
	bitset_reset(&bitset_data[0], desc, false);
	CHECK(bitset_data[0] == 0);
	CHECK(bitset_data[1] == 0);
	CHECK(bitset_data[2] == 0xFFFFFFFF);

	bitset_data[2] = 0;
	bitset_set(&bitset_data[0], desc, 0, false);
	CHECK(bitset_data[0] == 0);
	CHECK(bitset_data[1] == 0);
	CHECK(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 0, true);
	CHECK(bitset_data[0] == 0x80000000);
	CHECK(bitset_data[1] == 0);
	CHECK(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 31, true);
	CHECK(bitset_data[0] == 0x80000001);
	CHECK(bitset_data[1] == 0);
	CHECK(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 31, false);
	CHECK(bitset_data[0] == 0x80000000);
	CHECK(bitset_data[1] == 0);
	CHECK(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 32, true);
	CHECK(bitset_data[0] == 0x80000000);
	CHECK(bitset_data[1] == 0x80000000);
	CHECK(bitset_data[2] == 0);

	bitset_set_range(&bitset_data[0], desc, 8, 4, true);
	CHECK(bitset_data[0] == 0x80F00000);
	CHECK(bitset_data[1] == 0x80000000);
	CHECK(bitset_data[2] == 0);

	bitset_set_range(&bitset_data[0], desc, 10, 2, false);
	CHECK(bitset_data[0] == 0x80C00000);
	CHECK(bitset_data[1] == 0x80000000);
	CHECK(bitset_data[2] == 0);

	CHECK(bitset_test(&bitset_data[0], desc, 0) == true);
	CHECK(bitset_test(&bitset_data[0], desc, 1) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 2) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 3) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 4) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 5) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 6) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 7) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 8) == true);
	CHECK(bitset_test(&bitset_data[0], desc, 9) == true);
	CHECK(bitset_test(&bitset_data[0], desc, 10) == false);
	CHECK(bitset_test(&bitset_data[0], desc, 11) == false);

	bitset_data[2] = 0xFFFFFFFF;
	CHECK(bitset_count_set_bits(&bitset_data[0], desc) == 4);

	uint32_t bitset_data1[desc.get_size() + 1];	// Add padding
	bitset_data[0] = 0x00000010;
	bitset_data[1] = 0x00100000;

	bitset_data1[0] = 0x10101011;
	bitset_data1[1] = 0x10101011;
	bitset_and_not(bitset_data, bitset_data, bitset_data1, desc);
	CHECK(bitset_data[0] == 0x10101001);
	CHECK(bitset_data[1] == 0x10001011);
}
