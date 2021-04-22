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

#ifdef ACL_UNIT_TEST

#include <acl/math/vector4_32.h>

#endif

using namespace acl;

TEST_CASE("scalar packing math", "[math][scalar][packing]")
{

#ifdef ACL_UNIT_TEST

	Vector4_32 vec0;
	const uint8_t max_num_bits = 24;
	for (uint8_t num_bits = 1; num_bits <= max_num_bits; ++num_bits)
	{
		INFO("num_bits: " << int(num_bits));

#else

	const float threshold = 1.0E-6F;

	const uint8_t max_num_bits = 23;
	for (uint8_t num_bits = 1; num_bits < max_num_bits; ++num_bits)
	{

#endif

		const uint32_t max_value = (1 << num_bits) - 1;

		CHECK(pack_scalar_unsigned(0.0F, num_bits) == 0);
		CHECK(pack_scalar_unsigned(1.0F, num_bits) == max_value);
		CHECK(unpack_scalar_unsigned(0, num_bits) == 0.0F);
		CHECK(unpack_scalar_unsigned(max_value, num_bits) == 1.0F);

		CHECK(pack_scalar_signed(-1.0F, num_bits) == 0);
		CHECK(pack_scalar_signed(1.0F, num_bits) == max_value);
		CHECK(unpack_scalar_signed(0, num_bits) == -1.0F);

#ifdef ACL_UNIT_TEST

		CHECK(unpack_scalar_signed(max_value, num_bits) == 1.0F);

		// Inlined float functions can take advantage of additional precision,
		// so run the tests again after storing results as float.
		vec0 = vector_set(
			unpack_scalar_unsigned(0, num_bits),
			unpack_scalar_unsigned(max_value, num_bits),
			unpack_scalar_signed(0, num_bits),
			unpack_scalar_signed(max_value, num_bits));
		CHECK(vector_get_x(vec0) == 0.0F);
		CHECK(vector_get_y(vec0) == 1.0F);
		CHECK(vector_get_z(vec0) == -1.0F);
		CHECK(vector_get_w(vec0) == 1.0F);

#else

		CHECK(scalar_near_equal(unpack_scalar_signed(max_value, num_bits), 1.0F, threshold));

#endif

		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST

		const float max_error = 1.0F - std::nextafter((safe_to_float(max_value-1) + 0.5F) / safe_to_float(max_value), 1.0F);
		INFO("maxError: " << max_error);
		float prev0 = -2.0F;
		float prev1 = -2.0F;
		const float error = std::min(1.0E-6F, 1.0F / (1 << (num_bits + 1)));
		for (uint32_t value = 0; value <= max_value; ++value)

#else

		for (uint32_t value = 0; value < max_value; ++value)

#endif

		{
			const float unpacked0 = unpack_scalar_unsigned(value, num_bits);
			const uint32_t packed0 = pack_scalar_unsigned(unpacked0, num_bits);
			if (packed0 != value || unpacked0 < 0.0F || unpacked0 > 1.0F)
				num_errors++;

#ifdef ACL_UNIT_TEST

			const float true_unsigned = float(value) / max_value;
			const float true_signed = 2.0F * true_unsigned - 1.0F;
			if (std::fabs(true_unsigned - unpacked0) > error)
			{
				++num_errors;
			}
			CHECK(prev0 < unpacked0);
			prev0 = unpacked0;

#endif

			const float unpacked1 = unpack_scalar_signed(value, num_bits);
			const uint32_t packed1 = pack_scalar_signed(unpacked1, num_bits);
			if (packed1 != value || unpacked1 < -1.0F || unpacked1 > 1.0F)
				num_errors++;

#ifdef ACL_UNIT_TEST

			CHECK(prev1 < unpacked1);
			prev1 = unpacked1;

			if (value < max_value)
			{
				if (pack_scalar_unsigned(true_unsigned + max_error, num_bits) != value)
				{
					++num_errors;
					//INFO("trueUnsigned: " << trueUnsigned);
					//INFO("trueSigned: " << trueSigned);
					//INFO("value: " << int(value));
					//REQUIRE(false);
				}
				if (pack_scalar_signed(true_signed + 2.0F*max_error, num_bits) != value)
				{
					++num_errors;
					//INFO("trueUnsigned: " << trueUnsigned);
					//INFO("trueSigned: " << trueSigned);
					//INFO("value: " << int(value));
					//REQUIRE(false);
				}
			}
			if (value > 0)
			{
				if (pack_scalar_unsigned(true_unsigned - max_error, num_bits) != value)
				{
					++num_errors;
					//INFO("trueUnsigned: " << trueUnsigned);
					//INFO("trueSigned: " << trueSigned);
					//INFO("value: " << int(value));
					//REQUIRE(false);
				}
				if (pack_scalar_signed(true_signed - 2.0F*max_error, num_bits) != value)
				{
					++num_errors;
					//INFO("trueUnsigned: " << trueUnsigned);
					//INFO("trueSigned: " << trueSigned);
					//INFO("value: " << int(value));
					//REQUIRE(false);
				}
			}

#endif

		}
		CHECK(num_errors == 0);
	}
}
