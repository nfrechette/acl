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

#include <acl/core/bitset.h>

#include <cstring>

using namespace acl;

TEST_CASE("bitset", "[core][utils]")
{
	REQUIRE(BitSetDescription::make_from_num_bits(0).get_size() == 0);
	REQUIRE(BitSetDescription::make_from_num_bits(1).get_size() == 1);
	REQUIRE(BitSetDescription::make_from_num_bits(31).get_size() == 1);
	REQUIRE(BitSetDescription::make_from_num_bits(32).get_size() == 1);
	REQUIRE(BitSetDescription::make_from_num_bits(33).get_size() == 2);
	REQUIRE(BitSetDescription::make_from_num_bits(64).get_size() == 2);
	REQUIRE(BitSetDescription::make_from_num_bits(65).get_size() == 3);

	REQUIRE(BitSetDescription::make_from_num_bits(0).get_num_bits() == 0);
	REQUIRE(BitSetDescription::make_from_num_bits(1).get_num_bits() == 32);
	REQUIRE(BitSetDescription::make_from_num_bits(31).get_num_bits() == 32);
	REQUIRE(BitSetDescription::make_from_num_bits(32).get_num_bits() == 32);
	REQUIRE(BitSetDescription::make_from_num_bits(33).get_num_bits() == 64);
	REQUIRE(BitSetDescription::make_from_num_bits(64).get_num_bits() == 64);
	REQUIRE(BitSetDescription::make_from_num_bits(65).get_num_bits() == 96);

	constexpr BitSetDescription desc = BitSetDescription::make_from_num_bits<64>();
	REQUIRE(desc.get_size() == 2);
	REQUIRE(desc.get_size() == BitSetDescription::make_from_num_bits(64).get_size());

	uint32_t bitset_data[desc.get_size() + 1];	// Add padding
	std::memset(&bitset_data[0], 0, sizeof(bitset_data));

	bitset_reset(&bitset_data[0], desc, true);
	REQUIRE(bitset_data[0] == 0xFFFFFFFF);
	REQUIRE(bitset_data[1] == 0xFFFFFFFF);
	REQUIRE(bitset_data[2] == 0);

	bitset_data[2] = 0xFFFFFFFF;
	bitset_reset(&bitset_data[0], desc, false);
	REQUIRE(bitset_data[0] == 0);
	REQUIRE(bitset_data[1] == 0);
	REQUIRE(bitset_data[2] == 0xFFFFFFFF);

	bitset_data[2] = 0;
	bitset_set(&bitset_data[0], desc, 0, false);
	REQUIRE(bitset_data[0] == 0);
	REQUIRE(bitset_data[1] == 0);
	REQUIRE(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 0, true);
	REQUIRE(bitset_data[0] == 0x80000000);
	REQUIRE(bitset_data[1] == 0);
	REQUIRE(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 31, true);
	REQUIRE(bitset_data[0] == 0x80000001);
	REQUIRE(bitset_data[1] == 0);
	REQUIRE(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 31, false);
	REQUIRE(bitset_data[0] == 0x80000000);
	REQUIRE(bitset_data[1] == 0);
	REQUIRE(bitset_data[2] == 0);

	bitset_set(&bitset_data[0], desc, 32, true);
	REQUIRE(bitset_data[0] == 0x80000000);
	REQUIRE(bitset_data[1] == 0x80000000);
	REQUIRE(bitset_data[2] == 0);

	bitset_set_range(&bitset_data[0], desc, 8, 4, true);
	REQUIRE(bitset_data[0] == 0x80F00000);
	REQUIRE(bitset_data[1] == 0x80000000);
	REQUIRE(bitset_data[2] == 0);

	bitset_set_range(&bitset_data[0], desc, 10, 2, false);
	REQUIRE(bitset_data[0] == 0x80C00000);
	REQUIRE(bitset_data[1] == 0x80000000);
	REQUIRE(bitset_data[2] == 0);

	REQUIRE(bitset_test(&bitset_data[0], desc, 0) == true);
	REQUIRE(bitset_test(&bitset_data[0], desc, 1) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 2) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 3) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 4) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 5) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 6) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 7) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 8) == true);
	REQUIRE(bitset_test(&bitset_data[0], desc, 9) == true);
	REQUIRE(bitset_test(&bitset_data[0], desc, 10) == false);
	REQUIRE(bitset_test(&bitset_data[0], desc, 11) == false);

	bitset_data[2] = 0xFFFFFFFF;
	REQUIRE(bitset_count_set_bits(&bitset_data[0], desc) == 4);
}
