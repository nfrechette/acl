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

#include <acl/math/scalar_packing.h>

using namespace acl;

TEST_CASE("scalar packing math", "[math][scalar][packing]")
{
	const float threshold = 1.0e-6f;

	const uint8_t max_num_bits = 23;
	for (uint8_t num_bits = 1; num_bits < max_num_bits; ++num_bits)
	{
		const uint32_t max_value = (1 << num_bits) - 1;

		CHECK(pack_scalar_unsigned(0.0f, num_bits) == 0);
		CHECK(pack_scalar_unsigned(1.0f, num_bits) == max_value);
		CHECK(unpack_scalar_unsigned(0, num_bits) == 0.0f);
		CHECK(unpack_scalar_unsigned(max_value, num_bits) == 1.0f);

		CHECK(pack_scalar_signed(-1.0f, num_bits) == 0);
		CHECK(pack_scalar_signed(1.0f, num_bits) == max_value);
		CHECK(unpack_scalar_signed(0, num_bits) == -1.0f);
		CHECK(scalar_near_equal(unpack_scalar_signed(max_value, num_bits), 1.0f, threshold));

		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < max_value; ++value)
		{
			const float unpacked0 = unpack_scalar_unsigned(value, num_bits);
			const uint32_t packed0 = pack_scalar_unsigned(unpacked0, num_bits);
			if (packed0 != value || unpacked0 < 0.0f || unpacked0 > 1.0f)
				num_errors++;

			const float unpacked1 = unpack_scalar_signed(value, num_bits);
			const uint32_t packed1 = pack_scalar_signed(unpacked1, num_bits);
			if (packed1 != value || unpacked1 < -1.0f || unpacked1 > 1.0f)
				num_errors++;
		}
		CHECK(num_errors == 0);
	}
}
