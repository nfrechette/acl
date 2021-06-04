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

#include <acl/core/variable_bit_rates.h>
#include <acl/math/scalar_packing.h>
#include <acl/math/vector4_packing.h>

using namespace acl;
using namespace rtm;

struct UnalignedBuffer
{
	uint32_t padding0;
	uint16_t padding1;
	uint8_t buffer[250];
};
static_assert((offsetof(UnalignedBuffer, buffer) % 2) == 0, "Minimum packing alignment is 2");

TEST_CASE("scalar packing math", "[math][scalar][packing]")
{

#ifdef ACL_UNIT_TEST_STRICT

	for (uint8_t num_bits = 1; num_bits <= 23; ++num_bits)
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

#ifdef ACL_UNIT_TEST_STRICT

		CHECK(unpack_scalar_unsigned(max_value, num_bits) == 1.0F);

#else

		CHECK(rtm::scalar_near_equal(unpack_scalar_unsigned(max_value, num_bits), 1.0F, threshold));

#endif

#if !defined(ACL_PACKING_PRECISION_BOOST) && !defined(ACL_UNIT_TEST_STRICT)

		CHECK(pack_scalar_signed(-1.0F, num_bits) == 0);
		CHECK(pack_scalar_signed(1.0F, num_bits) == max_value);
		CHECK(unpack_scalar_signed(0, num_bits) == -1.0F);
		CHECK(rtm::scalar_near_equal(unpack_scalar_signed(max_value, num_bits), 1.0F, threshold));

#endif

		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST_STRICT

		const float max_error = 1.0F - std::nextafter((rtm::scalar_safe_to_float(max_value - 1) + 0.5F) / rtm::scalar_safe_to_float(max_value), 1.0F);
		INFO("maxError: " << max_error);
		float prev = -2.0F;
		const float threshold = std::min(1.0E-6F, 1.0F / (1 << (num_bits + 1)));
		for (uint32_t value = 0; value <= max_value; ++value)
		{
			INFO("value: " << int(value));

#else

		for (uint32_t value = 0; value < max_value; ++value)
		{

#endif

			const float unpacked0 = unpack_scalar_unsigned(value, num_bits);
			const uint32_t packed0 = pack_scalar_unsigned(unpacked0, num_bits);
			if (packed0 != value || unpacked0 < 0.0F || unpacked0 > 1.0F)
				num_errors++;

#ifdef ACL_UNIT_TEST_STRICT

			const float true_unsigned = float(value) / max_value;
			if (std::fabs(true_unsigned - unpacked0) > threshold)
			{
				++num_errors;
			}
			CHECK(prev < unpacked0);
			prev = unpacked0;

#endif

#ifndef ACL_PACKING_PRECISION_BOOST

			const float unpacked1 = unpack_scalar_signed(value, num_bits);
			const uint32_t packed1 = pack_scalar_signed(unpacked1, num_bits);
			if (packed1 != value || unpacked1 < -1.0F || unpacked1 > 1.0F)
				num_errors++;

#endif

#ifdef ACL_UNIT_TEST_STRICT

			if (value < max_value)
			{
				if (pack_scalar_unsigned(true_unsigned + max_error, num_bits) != value)
				{
					++num_errors;
				}
			}
			if (value > 0)
			{
				if (pack_scalar_unsigned(true_unsigned - max_error, num_bits) != value)
				{
					++num_errors;
				}
			}

#endif

		}
		CHECK(num_errors == 0);
	}
}

#ifdef ACL_UNIT_TEST_STRICT

TEST_CASE("unpack_scalarf_32_unsafe", "[math][scalar][packing]")

#else

TEST_CASE("unpack_scalarf_96_unsafe", "[math][scalar][packing]")

#endif

{
	{
		UnalignedBuffer tmp0;
		UnalignedBuffer tmp1;
		vector4f vec0 = vector_set(6123.123812F, 19237.01293127F, 0.913912387F, 0.1816253F);
		pack_vector4_128(vec0, &tmp0.buffer[0]);

		uint32_t x = unaligned_load<uint32_t>(&tmp0.buffer[0]);
		x = byte_swap(x);
		unaligned_write(x, &tmp0.buffer[0]);

		uint32_t y = unaligned_load<uint32_t>(&tmp0.buffer[4]);
		y = byte_swap(y);
		unaligned_write(y, &tmp0.buffer[4]);

		const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
		uint32_t num_errors = 0;
		for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
		{
			const uint8_t offset = offsets[offset_idx];

			memcpy_bits(&tmp1.buffer[0], offset, &tmp0.buffer[0], 0, 32);
			scalarf scalar1 = unpack_scalarf_32_unsafe(&tmp1.buffer[0], offset);

#ifdef ACL_UNIT_TEST_STRICT

			if (!scalar_equal(vector_get_x(vec0), scalar_cast(scalar1)))

#else

			if (!scalar_near_equal(vector_get_x(vec0), scalar_cast(scalar1), 1.0E-6F))

#endif

				num_errors++;

			memcpy_bits(&tmp1.buffer[0], offset, &tmp0.buffer[4], 0, 32);
			scalar1 = unpack_scalarf_32_unsafe(&tmp1.buffer[0], offset);

#ifdef ACL_UNIT_TEST_STRICT

			if (!scalar_equal(vector_get_y(vec0), scalar_cast(scalar1)))

#else

			if (!scalar_near_equal(vector_get_y(vec0), scalar_cast(scalar1), 1.0E-6F))

#endif

				num_errors++;
		}
		CHECK(num_errors == 0);
	}
}

TEST_CASE("unpack_scalarf_uXX_unsafe", "[math][scalar][packing]")
{
	{
		UnalignedBuffer tmp0;
		alignas(16) uint8_t buffer[64];

		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST_STRICT

		vector4f vec0;
		scalarf scalar1;
		scalarf scalar2;

#else

		vector4f vec0 = vector_set(unpack_scalar_unsigned(0, 16), unpack_scalar_unsigned(12355, 16), unpack_scalar_unsigned(43222, 16), unpack_scalar_unsigned(54432, 16));
		pack_vector2_uXX_unsafe(vec0, 16, &buffer[0]);
		scalarf scalar1 = unpack_scalarf_uXX_unsafe(16, &buffer[0], 0);
		if (!scalar_near_equal(vector_get_x(vec0), scalar_cast(scalar1), 1.0E-6F))
			num_errors++;

#endif

		for (uint8_t bit_rate = 1; bit_rate < k_highest_bit_rate; ++bit_rate)
		{
			uint32_t num_bits = get_num_bits_at_bit_rate(bit_rate);
			uint32_t max_value = (1 << num_bits) - 1;

#ifdef ACL_UNIT_TEST_STRICT

			INFO("num_bits: " << int(num_bits));
			const float max_error = 1.0F - std::nextafter((rtm::scalar_safe_to_float(max_value - 1) + 0.5F) / rtm::scalar_safe_to_float(max_value), 1.0F);
			INFO("maxError: " << max_error);
			const float threshold = std::min(1.0E-6F, 1.0F / (1 << (num_bits + 1)));

			vec0 = vector_set(0.0F, 1.0F, 0.0F, 0.0F);
			pack_vector4_uXX_unsafe(vec0, num_bits, &buffer[0]);
			scalar1 = unpack_scalarf_uXX_unsafe(num_bits, &buffer[0], 0);
			if (!scalar_equal(vector_get_x(vec0), scalar1))
			{
				++num_errors;
			}

			scalar2 = unpack_scalarf_uXX_unsafe(num_bits, &buffer[0], num_bits);
			if (!scalar_equal(vector_get_y(vec0), scalar2))
			{
				++num_errors;
			}

#endif

			for (uint32_t value = 0; value <= max_value; ++value)
			{

#ifdef ACL_UNIT_TEST_STRICT

				vec0 = vector_set(float(value) / max_value, 0.0F, 0.0F, 0.0F);

				pack_vector4_uXX_unsafe(vec0, num_bits, &buffer[0]);
				scalar1 = unpack_scalarf_uXX_unsafe(num_bits, &buffer[0], 0);
				
				pack_vector4_uXX_unsafe(vector_set(std::min(vector_get_x(vec0) + max_error, 1.0F)), num_bits, &buffer[0]);
				scalar2 = unpack_scalarf_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!scalar_equal(scalar1, scalar2))
				{
					++num_errors;
				}

				pack_vector4_uXX_unsafe(vector_set(std::max(vector_get_x(vec0) - max_error, 0.0F)), num_bits, &buffer[0]);
				scalar2 = unpack_scalarf_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!scalar_equal(scalar1, scalar2))
				{
					++num_errors;
				}

				if (!scalar_near_equal(vector_get_x(vec0), scalar_cast(scalar1), threshold))

#else

				const float value_unsigned = scalar_clamp(unpack_scalar_unsigned(value, num_bits), 0.0F, 1.0F);

				vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
				pack_vector2_uXX_unsafe(vec0, num_bits, &buffer[0]);
				scalar1 = unpack_scalarf_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!scalar_near_equal(vector_get_x(vec0), scalar_cast(scalar1), 1.0E-6F))

#endif		
				
					num_errors++;

				{
					const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
					for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
					{
						const uint8_t offset = offsets[offset_idx];

						memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, size_t(num_bits) * 4);
						scalar1 = unpack_scalarf_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						
#ifdef ACL_UNIT_TEST_STRICT

						if (!scalar_equal(scalar1, scalar2))

#else						

						if (!scalar_near_equal(vector_get_x(vec0), scalar_cast(scalar1), 1.0E-6F))

#endif

							num_errors++;
					}
				}
			}
		}
		CHECK(num_errors == 0);
	}
}
