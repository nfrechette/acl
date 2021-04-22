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

#include <acl/math/scalar_32.h>
#include <acl/math/vector4_packing.h>

#include <cstring>

using namespace acl;

struct UnalignedBuffer
{
	uint32_t padding0;
	uint16_t padding1;
	uint8_t buffer[250];
};
static_assert((offsetof(UnalignedBuffer, buffer) % 2) == 0, "Minimum packing alignment is 2");

TEST_CASE("pack_vector4_128", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp;
		Vector4_32 vec0 = vector_set(6123.123812F, 19237.01293127F, 1891.019231829F, 0.913912387F);
		pack_vector4_128(vec0, &tmp.buffer[0]);
		Vector4_32 vec1 = unpack_vector4_128(&tmp.buffer[0]);
		REQUIRE(std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) == 0);
	}

	{
		UnalignedBuffer tmp0;
		UnalignedBuffer tmp1;
		Vector4_32 vec0 = vector_set(6123.123812F, 19237.01293127F, 1891.019231829F, 0.913912387F);
		pack_vector4_128(vec0, &tmp0.buffer[0]);

		uint32_t x = unaligned_load<uint32_t>(&tmp0.buffer[0]);
		x = byte_swap(x);
		unaligned_write(x, &tmp0.buffer[0]);

		uint32_t y = unaligned_load<uint32_t>(&tmp0.buffer[4]);
		y = byte_swap(y);
		unaligned_write(y, &tmp0.buffer[4]);

		uint32_t z = unaligned_load<uint32_t>(&tmp0.buffer[8]);
		z = byte_swap(z);
		unaligned_write(z, &tmp0.buffer[8]);

		uint32_t w = unaligned_load<uint32_t>(&tmp0.buffer[12]);
		w = byte_swap(w);
		unaligned_write(w, &tmp0.buffer[12]);

		const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
		uint32_t num_errors = 0;
		for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
		{
			const uint8_t offset = offsets[offset_idx];

			memcpy_bits(&tmp1.buffer[0], offset, &tmp0.buffer[0], 0, 128);
			Vector4_32 vec1 = unpack_vector4_128_unsafe(&tmp1.buffer[0], offset);
			if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector4_64", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 65536; ++value)
		{
			const float value_signed = unpack_scalar_signed(value, 16);
			const float value_unsigned = unpack_scalar_unsigned(value, 16);

			Vector4_32 vec0 = vector_set(value_signed);
			pack_vector4_64(vec0, false, &tmp.buffer[0]);
			Vector4_32 vec1 = unpack_vector4_64(&tmp.buffer[0], false);
			if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))
				num_errors++;

			vec0 = vector_set(value_unsigned);
			pack_vector4_64(vec0, true, &tmp.buffer[0]);
			vec1 = unpack_vector4_64(&tmp.buffer[0], true);
			if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector4_32", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 256; ++value)
		{
			const float value_signed = min(unpack_scalar_signed(value, 8), 1.0F);
			const float value_unsigned = min(unpack_scalar_unsigned(value, 8), 1.0F);

			Vector4_32 vec0 = vector_set(value_signed);
			pack_vector4_32(vec0, false, &tmp.buffer[0]);
			Vector4_32 vec1 = unpack_vector4_32(&tmp.buffer[0], false);
			if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))
				num_errors++;

			vec0 = vector_set(value_unsigned);
			pack_vector4_32(vec0, true, &tmp.buffer[0]);
			vec1 = unpack_vector4_32(&tmp.buffer[0], true);
			if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector4_XX", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp0;
		alignas(16) uint8_t buffer[64];

		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST

		Vector4_32 vec0;
		Vector4_32 vec1;
		Vector4_32 vec2;
		Vector4_32 vec_max_error;

#else

		Vector4_32 vec0 = vector_set(unpack_scalar_unsigned(0, 16), unpack_scalar_unsigned(12355, 16), unpack_scalar_unsigned(43222, 16), unpack_scalar_unsigned(54432, 16));
		pack_vector4_uXX_unsafe(vec0, 16, &buffer[0]);
		Vector4_32 vec1 = unpack_vector4_uXX_unsafe(16, &buffer[0], 0);
		if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))
			num_errors++;

#endif

		for (uint8_t bit_rate = 1; bit_rate < k_highest_bit_rate; ++bit_rate)
		{
			uint8_t num_bits = get_num_bits_at_bit_rate(bit_rate);
			uint32_t max_value = (1 << num_bits) - 1;

#ifdef ACL_UNIT_TEST

			INFO("num_bits: " << int(num_bits));
			const float error = std::min(1.0E-6F, 1.0F / (1 << (num_bits + 1)));

			const float max_error = 1.0F - std::nextafter((safe_to_float(max_value - 1) + 0.5F) / safe_to_float(max_value), 1.0F);
			INFO("max_error: " << max_error);
			vec_max_error = vector_set(max_error);

			vec0 = vector_set(0.0F, 1.0F, 0.0F, 1.0F);
			pack_vector4_uXX_unsafe(vec0, num_bits, &buffer[0]);
			vec1 = unpack_vector4_uXX_unsafe(num_bits, &buffer[0], 0);
			if (!vector_all_near_equal(vec0, vec1, 0.0F))
			{
				INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1) << vector_get_w(vec1));
				REQUIRE(false);
			}

			for (uint32_t value = 0; value <= max_value; value += 4)
			{
				vec0 = vector_min(vector_set(
					float(value) / max_value,
					float(value + 1) / max_value,
					float(value + 2) / max_value,
					float(value + 3) / max_value), vector_set(1.0F));

#else

			for (uint32_t value = 0; value <= max_value; ++value)
			{
				const float value_unsigned = clamp(unpack_scalar_unsigned(value, num_bits), 0.0F, 1.0F);

				vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);

#endif

				pack_vector4_uXX_unsafe(vec0, num_bits, &buffer[0]);
				vec1 = unpack_vector4_uXX_unsafe(num_bits, &buffer[0], 0);

#ifdef ACL_UNIT_TEST

				pack_vector4_uXX_unsafe(vector_max(vector_sub(vec0, vec_max_error), vector_set(0.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector4_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}
			
				pack_vector4_uXX_unsafe(vector_min(vector_add(vec0, vec_max_error), vector_set(1.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector4_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}

				if (!(vector_all_near_equal(vec0, vec1, error) && vector_all_greater_equal(vec1, vector_set(0.0F)) && vector_all_less_equal(vec1, vector_set(1.0F))))

#else

				if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))

#endif
					num_errors++;

				{
					const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
					for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
					{
						const uint8_t offset = offsets[offset_idx];

						memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, num_bits * 4);

#ifdef ACL_UNIT_TEST

						vec2 = unpack_vector4_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!(vector_all_near_equal(vec1, vec2, 0.0F)))

#else

						vec1 = unpack_vector4_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!vector_all_near_equal(vec0, vec1, 1.0E-6F))

#endif

							num_errors++;
					}
				}
			}

#ifdef ACL_UNIT_TEST

			REQUIRE(num_errors == 0);

#endif

		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector3_96", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp0;
		UnalignedBuffer tmp1;
		Vector4_32 vec0 = vector_set(6123.123812F, 19237.01293127F, 0.913912387F);
		pack_vector3_96(vec0, &tmp0.buffer[0]);
		Vector4_32 vec1 = unpack_vector3_96_unsafe(&tmp0.buffer[0]);
		REQUIRE(vector_all_near_equal3(vec0, vec1, 1.0E-6F));

		uint32_t x = unaligned_load<uint32_t>(&tmp0.buffer[0]);
		x = byte_swap(x);
		unaligned_write(x, &tmp0.buffer[0]);

		uint32_t y = unaligned_load<uint32_t>(&tmp0.buffer[4]);
		y = byte_swap(y);
		unaligned_write(y, &tmp0.buffer[4]);

		uint32_t z = unaligned_load<uint32_t>(&tmp0.buffer[8]);
		z = byte_swap(z);
		unaligned_write(z, &tmp0.buffer[8]);

		const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
		uint32_t num_errors = 0;
		for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
		{
			const uint8_t offset = offsets[offset_idx];

			memcpy_bits(&tmp1.buffer[0], offset, &tmp0.buffer[0], 0, 96);
			vec1 = unpack_vector3_96_unsafe(&tmp1.buffer[0], offset);
			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector3_48", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp0;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 65536; ++value)
		{
			const float value_signed = unpack_scalar_signed(value, 16);
			const float value_unsigned = unpack_scalar_unsigned(value, 16);

			Vector4_32 vec0 = vector_set(value_signed, value_signed, value_signed);
			pack_vector3_s48_unsafe(vec0, &tmp0.buffer[0]);
			Vector4_32 vec1 = unpack_vector3_s48_unsafe(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;

			vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
			pack_vector3_u48_unsafe(vec0, &tmp0.buffer[0]);
			vec1 = unpack_vector3_u48_unsafe(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("decay_vector3_48", "[math][vector4][decay]")
{
	{
		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST

		Vector4_32 vec0;
		Vector4_32 vec_unsigned;
		Vector4_32 vec2;
		const Vector4_32 vec_max_error = vector_set(1.0F - std::nextafter((safe_to_float(65535 - 1) + 0.5F) / safe_to_float(65535), 1.0F));
		const Vector4_32 vec_signed_max_error = vector_mul(vec_max_error, vector_set(2.0F));
		const float error = std::min(1.0E-6F, 1.0F / (1 << (16 + 1)));

		for (uint32_t value = 0; value < 65536; value += 3)
		{
			vec_unsigned = vector_min(vector_set(
				float(value) / 65535,
				float(value + 1) / 65535,
				float(value + 2) / 65535), vector_set(1.0F));
			vec0 = vector_mul_add(vec_unsigned, vector_set(2.0F), vector_set(-1.0F));

#else

		for (uint32_t value = 0; value < 65536; ++value)
		{
			const float value_signed = unpack_scalar_signed(value, 16);
			const float value_unsigned = unpack_scalar_unsigned(value, 16);

			Vector4_32 vec0 = vector_set(value_signed, value_signed, value_signed);

#endif

			Vector4_32 vec1 = decay_vector3_s48(vec0);

#ifdef ACL_UNIT_TEST

			vec2 = decay_vector3_s48(vector_max(vector_sub(vec0, vec_signed_max_error), vector_set(-1.0F)));
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			vec2 = decay_vector3_s48(vector_min(vector_add(vec0, vec_signed_max_error), vector_set(1.0F)));
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			if (!(vector_all_near_equal3(vec0, vec1, error) && vector_all_greater_equal3(vec1, vector_set(-1.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec0: " << vector_get_x(vec0) << " " << vector_get_y(vec0) << " " << vector_get_z(vec0));
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//CHECK(false);
			}

#else

			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;

#endif

#ifdef ACL_UNIT_TEST

			vec0 = vec_unsigned;

#else

			vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);

#endif

			vec1 = decay_vector3_u48(vec0);

#ifdef ACL_UNIT_TEST

			vec2 = decay_vector3_u48(vector_max(vector_sub(vec0, vec_max_error), vector_set(0.0F)));
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			vec2 = decay_vector3_u48(vector_min(vector_add(vec0, vec_max_error), vector_set(1.0F)));
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			if (!(vector_all_near_equal3(vec0, vec1, error) && vector_all_greater_equal3(vec1, vector_set(0.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec0: " << vector_get_x(vec0) << " " << vector_get_y(vec0) << " " << vector_get_z(vec0));
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//CHECK(false);
			}

#else

			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;

#endif

		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector3_32", "[math][vector4][packing]")
{
	{
		const uint8_t num_bits_xy = 11;
		const uint8_t num_bits_z = 10;
		const uint32_t max_value_xy = (1 << num_bits_xy) - 1;

		UnalignedBuffer tmp0;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < max_value_xy; ++value)
		{
			const uint32_t value_xy = value;
			const uint32_t value_z = value % (1 << num_bits_z);
			const float value_signed_xy = unpack_scalar_signed(value_xy, num_bits_xy);
			const float value_signed_z = unpack_scalar_signed(value_z, num_bits_z);
			const float value_unsigned_xy = unpack_scalar_unsigned(value_xy, num_bits_xy);
			const float value_unsigned_z = unpack_scalar_unsigned(value_z, num_bits_z);

			Vector4_32 vec0 = vector_set(value_signed_xy, value_signed_xy, value_signed_z);
			pack_vector3_32(vec0, 11, 11, 10, false, &tmp0.buffer[0]);
			Vector4_32 vec1 = unpack_vector3_32(11, 11, 10, false, &tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;

			vec0 = vector_set(value_unsigned_xy, value_unsigned_xy, value_unsigned_z);
			pack_vector3_32(vec0, 11, 11, 10, true, &tmp0.buffer[0]);
			vec1 = unpack_vector3_32(11, 11, 10, true, &tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("decay_vector3_32", "[math][vector4][decay]")
{
	{
		const uint8_t num_bits_xy = 11;
		const uint8_t num_bits_z = 10;
		const uint32_t max_value_xy = (1 << num_bits_xy) - 1;

		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST

		Vector4_32 vec0;
		Vector4_32 vec_unsigned;
		Vector4_32 vec2;

		const float error_xy = std::min(1.0E-6F, 1.0F / (1 << (num_bits_xy + 1)));
		const Vector4_32 vec_error = vector_set(error_xy, error_xy, std::min(1.0E-6F, 1.0F / (1 << (num_bits_z + 1))), 0.0F);

		const uint32_t max_value_z = (1 << num_bits_z) - 1;
		const float max_error_xy = 1.0F - std::nextafter((safe_to_float(max_value_xy - 1) + 0.5F) / safe_to_float(max_value_xy), 1.0F);
		const Vector4_32 vec_max_error = vector_set(max_error_xy, max_error_xy, 1.0F - std::nextafter((safe_to_float(max_value_z - 1) + 0.5F) / safe_to_float(max_value_z), 1.0F), 0.0F);
		const Vector4_32 vec_signed_max_error = vector_mul(vec_max_error, vector_set(2.0F));

		for (uint32_t value = 0; value < max_value_xy; value += 2)
		{
			vec_unsigned = vector_min(vector_set(
				float(value) / max_value_xy,
				float(value + 1) / max_value_xy,
				float(value/2) / max_value_z), vector_set(1.0F));
			vec0 = vector_mul_add(vec_unsigned, vector_set(2.0F), vector_set(-1.0F));

#else

		for (uint32_t value = 0; value < max_value_xy; ++value)
		{
			const uint32_t value_xy = value;
			const uint32_t value_z = value % (1 << num_bits_z);
			const float value_signed_xy = unpack_scalar_signed(value_xy, num_bits_xy);
			const float value_signed_z = unpack_scalar_signed(value_z, num_bits_z);
			const float value_unsigned_xy = unpack_scalar_unsigned(value_xy, num_bits_xy);
			const float value_unsigned_z = unpack_scalar_unsigned(value_z, num_bits_z);

			Vector4_32 vec0 = vector_set(value_signed_xy, value_signed_xy, value_signed_z);

#endif

			Vector4_32 vec1 = decay_vector3_s32(vec0, num_bits_xy, num_bits_xy, num_bits_z);

#ifdef ACL_UNIT_TEST

			vec2 = decay_vector3_s32(vector_max(vector_sub(vec0, vec_signed_max_error), vector_set(-1.0F)), num_bits_xy, num_bits_xy, num_bits_z);
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			vec2 = decay_vector3_s32(vector_min(vector_add(vec0, vec_signed_max_error), vector_set(1.0F)), num_bits_xy, num_bits_xy, num_bits_z);
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			if (!(vector_all_less_equal3(vector_abs(vector_sub(vec0, vec1)), vec_error) && vector_all_greater_equal3(vec1, vector_set(-1.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec0: " << vector_get_x(vec0) << " " << vector_get_y(vec0) << " " << vector_get_z(vec0));
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//CHECK(false);
			}

#else

			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;

#endif

#ifdef ACL_UNIT_TEST

			vec0 = vec_unsigned;

#else

			vec0 = vector_set(value_unsigned_xy, value_unsigned_xy, value_unsigned_z);

#endif
			
			vec1 = decay_vector3_u32(vec0, num_bits_xy, num_bits_xy, num_bits_z);

#ifdef ACL_UNIT_TEST

			vec2 = decay_vector3_u32(vector_max(vector_sub(vec0, vec_max_error), vector_set(0.0F)), num_bits_xy, num_bits_xy, num_bits_z);
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			vec2 = decay_vector3_u32(vector_min(vector_add(vec0, vec_max_error), vector_set(1.0F)), num_bits_xy, num_bits_xy, num_bits_z);
			if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
				//CHECK(false);
			}

			if (!(vector_all_less_equal3(vector_abs(vector_sub(vec0, vec1)), vec_error) && vector_all_greater_equal3(vec1, vector_set(-1.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))
			{
				++num_errors;
				//INFO("value: " << (int)value);
				//INFO("vec0: " << vector_get_x(vec0) << " " << vector_get_y(vec0) << " " << vector_get_z(vec0));
				//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				//CHECK(false);
			}

#else

			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;

#endif

		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector3_24", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp0;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 256; ++value)
		{
			const float value_signed = min(unpack_scalar_signed(value, 8), 1.0F);
			const float value_unsigned = min(unpack_scalar_unsigned(value, 8), 1.0F);

			Vector4_32 vec0 = vector_set(value_signed, value_signed, value_signed);
			pack_vector3_s24_unsafe(vec0, &tmp0.buffer[0]);
			Vector4_32 vec1 = unpack_vector3_s24_unsafe(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;

			vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
			pack_vector3_u24_unsafe(vec0, &tmp0.buffer[0]);
			vec1 = unpack_vector3_u24_unsafe(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector3_XX", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp0;
		alignas(16) uint8_t buffer[64];

		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST

		Vector4_32 vec0;
		Vector4_32 vec1;
		Vector4_32 vec2;
		Vector4_32 vec_max_error;
		Vector4_32 vec_signed_max_error;

#else

		Vector4_32 vec0 = vector_set(unpack_scalar_signed(0, 16), unpack_scalar_signed(12355, 16), unpack_scalar_signed(43222, 16));
		pack_vector3_sXX_unsafe(vec0, 16, &buffer[0]);
		Vector4_32 vec1 = unpack_vector3_sXX_unsafe(16, &buffer[0], 0);
		if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
			num_errors++;

		vec0 = vector_set(unpack_scalar_unsigned(0, 16), unpack_scalar_unsigned(12355, 16), unpack_scalar_unsigned(43222, 16));
		pack_vector3_uXX_unsafe(vec0, 16, &buffer[0]);
		vec1 = unpack_vector3_uXX_unsafe(16, &buffer[0], 0);
		if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
			num_errors++;

#endif

		for (uint8_t bit_rate = 1; bit_rate < k_highest_bit_rate; ++bit_rate)
		{
			uint8_t num_bits = get_num_bits_at_bit_rate(bit_rate);
			uint32_t max_value = (1 << num_bits) - 1;

#ifdef ACL_UNIT_TEST

			INFO("num_bits: " << int(num_bits));
			const float error = std::min(1.0E-6F, 1.0F / (1 << (num_bits + 1)));

			const float max_error = 1.0F - std::nextafter((safe_to_float(max_value - 1) + 0.5F) / safe_to_float(max_value), 1.0F);
			INFO("max_error: " << max_error);
			vec_max_error = vector_set(max_error);
			vec_signed_max_error = vector_mul(vec_max_error, vector_set(2.0F));

			vec0 = vector_set(0.0F, 1.0F, 0.0F);
			pack_vector3_uXX_unsafe(vec0, num_bits, &buffer[0]);
			vec1 = unpack_vector3_uXX_unsafe(num_bits, &buffer[0], 0);
			if (!vector_all_near_equal3(vec0, vec1, 0.0F))
			{
				INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				REQUIRE(false);
			}

			vec0 = vector_set(-1.0F, 1.0F, -1.0F);
			pack_vector3_sXX_unsafe(vec0, num_bits, &buffer[0]);
			vec1 = unpack_vector3_sXX_unsafe(num_bits, &buffer[0], 0);
			if (!vector_all_near_equal3(vec0, vec1, 0.0F))
			{
				INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
				REQUIRE(false);
			}

			for (uint32_t value = 0; value <= max_value; value += 3)
			{
				vec0 = vector_min(vector_set(
					float(value) / max_value,
					float(value + 1) / max_value,
					float(value + 2) / max_value), vector_set(1.0F));

#else

			for (uint32_t value = 0; value <= max_value; ++value)
			{
				const float value_signed = clamp(unpack_scalar_signed(value, num_bits), -1.0F, 1.0F);
				const float value_unsigned = clamp(unpack_scalar_unsigned(value, num_bits), 0.0F, 1.0F);

				vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);

#endif

				pack_vector3_uXX_unsafe(vec0, num_bits, &buffer[0]);
				vec1 = unpack_vector3_uXX_unsafe(num_bits, &buffer[0], 0);

#ifdef ACL_UNIT_TEST

				pack_vector3_uXX_unsafe(vector_max(vector_sub(vec0, vec_max_error), vector_set(0.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector3_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}

				pack_vector3_uXX_unsafe(vector_min(vector_add(vec0, vec_max_error), vector_set(1.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector3_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}

				if (!(vector_all_near_equal3(vec0, vec1, error) && vector_all_greater_equal3(vec1, vector_set(0.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))

#else

				if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))

#endif

					num_errors++;

				{
					const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
					for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
					{
						const uint8_t offset = offsets[offset_idx];

						memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, num_bits * 3);

#ifdef ACL_UNIT_TEST

						vec2 = unpack_vector3_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))

#else

						vec1 = unpack_vector3_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))

#endif

							num_errors++;
					}
				}

#ifdef ACL_UNIT_TEST

				vec0 = vector_mul_add(vec0, vector_set(2.0F), vector_set(-1.0F));

#else

				vec0 = vector_set(value_signed, value_signed, value_signed);

#endif

				pack_vector3_sXX_unsafe(vec0, num_bits, &buffer[0]);
				vec1 = unpack_vector3_sXX_unsafe(num_bits, &buffer[0], 0);

#ifdef ACL_UNIT_TEST

				pack_vector3_sXX_unsafe(vector_max(vector_sub(vec0, vec_signed_max_error), vector_set(-1.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector3_sXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}

				pack_vector3_sXX_unsafe(vector_min(vector_add(vec0, vec_signed_max_error), vector_set(1.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector3_sXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}

				if (!(vector_all_near_equal3(vec0, vec1, 2.0F*error) && vector_all_greater_equal3(vec1, vector_set(-1.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))

#else

				if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))

#endif

					num_errors++;

				{
					const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
					for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
					{
						const uint8_t offset = offsets[offset_idx];

						memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, num_bits * 3);

#ifdef ACL_UNIT_TEST

						vec2 = unpack_vector3_sXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))

#else

						vec1 = unpack_vector3_sXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))

#endif

						
							num_errors++;
					}
				}
			}

#ifdef ACL_UNIT_TEST

			CHECK(num_errors == 0);
			num_errors = 0;

#endif

		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("decay_vector3_XX", "[math][vector4][decay]")
{
	{
		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST

		Vector4_32 vec0;
		Vector4_32 vec_unsigned;
		Vector4_32 vec1;
		Vector4_32 vec2;
		Vector4_32 vec_max_error;
		Vector4_32 vec_signed_max_error;

#else

		Vector4_32 vec0 = vector_set(unpack_scalar_signed(0, 16), unpack_scalar_signed(12355, 16), unpack_scalar_signed(43222, 16));
		Vector4_32 vec1 = decay_vector3_sXX(vec0, 16);
		if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
			num_errors++;

		vec0 = vector_set(unpack_scalar_unsigned(0, 16), unpack_scalar_unsigned(12355, 16), unpack_scalar_unsigned(43222, 16));
		vec1 = decay_vector3_uXX(vec0, 16);
		if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
			num_errors++;

#endif

		for (uint8_t bit_rate = 1; bit_rate < k_highest_bit_rate; ++bit_rate)
		{
			uint8_t num_bits = get_num_bits_at_bit_rate(bit_rate);
			uint32_t max_value = (1 << num_bits) - 1;

#ifdef ACL_UNIT_TEST

			INFO("num_bits: " << int(num_bits));
			const float error = std::min(1.0E-6F, 1.0F / (1 << (num_bits + 1)));

			const float max_error = 1.0F - std::nextafter((safe_to_float(max_value - 1) + 0.5F) / safe_to_float(max_value), 1.0F);
			INFO("max_error: " << max_error);
			vec_max_error = vector_set(max_error);
			vec_signed_max_error = vector_mul(vec_max_error, vector_set(2.0F));

			for (uint32_t value = 0; value <= max_value; value += 3)
			{
				vec_unsigned = vector_min(vector_set(
					float(value) / max_value,
					float(value + 1) / max_value,
					float(value + 2) / max_value), vector_set(1.0F));
				vec0 = vector_mul_add(vec_unsigned, vector_set(2.0F), vector_set(-1.0F));


#else

			for (uint32_t value = 0; value <= max_value; ++value)
			{
				const float value_signed = clamp(unpack_scalar_signed(value, num_bits), -1.0F, 1.0F);
				const float value_unsigned = clamp(unpack_scalar_unsigned(value, num_bits), 0.0F, 1.0F);

				vec0 = vector_set(value_signed, value_signed, value_signed);

#endif


				vec1 = decay_vector3_sXX(vec0, num_bits);

#ifdef ACL_UNIT_TEST

				vec2 = decay_vector3_sXX(vector_max(vector_sub(vec0, vec_signed_max_error), vector_set(-1.0F)), num_bits);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
					//INFO("value: " << (int)value);
					//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
					//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
					//CHECK(false);
				}

				vec2 = decay_vector3_sXX(vector_min(vector_add(vec0, vec_signed_max_error), vector_set(1.0F)), num_bits);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
					//INFO("value: " << (int)value);
					//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
					//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
					//CHECK(false);
				}

				if (!(vector_all_near_equal3(vec0, vec1, 2.0F*error) && vector_all_greater_equal3(vec1, vector_set(-1.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))
				{
					++num_errors;
					//INFO("value: " << (int)value);
					//INFO("vec0: " << vector_get_x(vec0) << " " << vector_get_y(vec0) << " " << vector_get_z(vec0));
					//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
					//CHECK(false);
				}

#else

				if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
					num_errors++;

#endif
	

#ifdef ACL_UNIT_TEST

				vec0 = vec_unsigned;

#else

				vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);

#endif

				vec1 = decay_vector3_uXX(vec0, num_bits);

#ifdef ACL_UNIT_TEST

				vec2 = decay_vector3_uXX(vector_max(vector_sub(vec0, vec_max_error), vector_set(0.0F)), num_bits);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
					//INFO("value: " << (int)value);
					//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
					//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
					//CHECK(false);
				}

				vec2 = decay_vector3_uXX(vector_min(vector_add(vec0, vec_max_error), vector_set(1.0F)), num_bits);
				if (!(vector_all_near_equal3(vec1, vec2, 0.0F)))
				{
					++num_errors;
					//INFO("value: " << (int)value);
					//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
					//INFO("vec2: " << vector_get_x(vec2) << " " << vector_get_y(vec2) << " " << vector_get_z(vec2));
					//CHECK(false);
				}

				if (!(vector_all_near_equal3(vec0, vec1, error) && vector_all_greater_equal3(vec1, vector_set(0.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))
				{
					++num_errors;
					//INFO("value: " << (int)value);
					//INFO("vec0: " << vector_get_x(vec0) << " " << vector_get_y(vec0) << " " << vector_get_z(vec0));
					//INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1) << " " << vector_get_z(vec1));
					//CHECK(false);
				}

#else

				if (!vector_all_near_equal3(vec0, vec1, 1.0E-6F))
					num_errors++;
#endif
					
			}

#ifdef ACL_UNIT_TEST

			REQUIRE(num_errors == 0);

#endif

		}

		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector2_64", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp0;
		UnalignedBuffer tmp1;
		Vector4_32 vec0 = vector_set(6123.123812F, 19237.01293127F, 0.913912387F, 0.1816253F);
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

			memcpy_bits(&tmp1.buffer[0], offset, &tmp0.buffer[0], 0, 64);
			Vector4_32 vec1 = unpack_vector2_64_unsafe(&tmp1.buffer[0], offset);
			if (!vector_all_near_equal2(vec0, vec1, 1.0E-6F))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("pack_vector2_XX", "[math][vector4][packing]")
{
	{
		UnalignedBuffer tmp0;
		alignas(16) uint8_t buffer[64];

		uint32_t num_errors = 0;

#ifdef ACL_UNIT_TEST

		Vector4_32 vec0;
		Vector4_32 vec1;
		Vector4_32 vec2;
		Vector4_32 vec_max_error;
		
#else

		Vector4_32 vec0 = vector_set(unpack_scalar_unsigned(0, 16), unpack_scalar_unsigned(12355, 16), unpack_scalar_unsigned(43222, 16), unpack_scalar_unsigned(54432, 16));
		pack_vector2_uXX_unsafe(vec0, 16, &buffer[0]);
		Vector4_32 vec1 = unpack_vector2_uXX_unsafe(16, &buffer[0], 0);
		if (!vector_all_near_equal2(vec0, vec1, 1.0E-6F))
			num_errors++;

#endif

		for (uint8_t bit_rate = 1; bit_rate < k_highest_bit_rate; ++bit_rate)
		{
			uint8_t num_bits = get_num_bits_at_bit_rate(bit_rate);
			uint32_t max_value = (1 << num_bits) - 1;

#ifdef ACL_UNIT_TEST

			INFO("num_bits: " << int(num_bits));
			const float error = std::min(1.0E-6F, 1.0F/(1 << (num_bits + 1)));

			const float max_error = 1.0F - std::nextafter((safe_to_float(max_value - 1) + 0.5F) / safe_to_float(max_value), 1.0F);
			INFO("max_error: " << max_error);
			vec_max_error = vector_set(max_error);

			vec0 = vector_set(0.0F, 1.0F, 0.0F);
			pack_vector2_uXX_unsafe(vec0, num_bits, &buffer[0]);
			vec1 = unpack_vector2_uXX_unsafe(num_bits, &buffer[0], 0);
			if (!vector_all_near_equal2(vec0, vec1, 0.0F))
			{
				INFO("vec1: " << vector_get_x(vec1) << " " << vector_get_y(vec1));
				REQUIRE(false);
			}

			for (uint32_t value = 0; value <= max_value; value += 2)
			{
				vec0 = vector_set(
					float(value)/max_value,
					float(value + 1)/max_value,
					0.0F);

#else

			for (uint32_t value = 0; value <= max_value; ++value)
			{
				const float value_unsigned = clamp(unpack_scalar_unsigned(value, num_bits), 0.0F, 1.0F);

				vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);

#endif

				pack_vector2_uXX_unsafe(vec0, num_bits, &buffer[0]);
				vec1 = unpack_vector2_uXX_unsafe(num_bits, &buffer[0], 0);

#ifdef ACL_UNIT_TEST

				pack_vector2_uXX_unsafe(vector_max(vector_sub(vec0, vec_max_error), vector_set(0.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector2_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal2(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}

				pack_vector2_uXX_unsafe(vector_min(vector_add(vec0, vec_max_error), vector_set(1.0F)), num_bits, &buffer[0]);
				vec2 = unpack_vector2_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!(vector_all_near_equal2(vec1, vec2, 0.0F)))
				{
					++num_errors;
				}

				if (!(vector_all_near_equal2(vec0, vec1, error) && vector_all_greater_equal3(vec1, vector_set(0.0F)) && vector_all_less_equal3(vec1, vector_set(1.0F))))

#else

				if (!vector_all_near_equal2(vec0, vec1, 1.0E-6F))

#endif

					num_errors++;

				{
					const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
					for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
					{
						const uint8_t offset = offsets[offset_idx];

						memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, num_bits * 4);

#ifdef ACL_UNIT_TEST

						vec2 = unpack_vector2_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!(vector_all_near_equal2(vec1, vec2, 0.0F)))

#else

						vec1 = unpack_vector2_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!vector_all_near_equal2(vec0, vec1, 1.0E-6F))

#endif

							num_errors++;
					}
				}
			}

#ifdef ACL_UNIT_TEST

			REQUIRE(num_errors == 0);

#endif

		}
		REQUIRE(num_errors == 0);
	}
}

TEST_CASE("misc vector4 packing", "[math][vector4][packing]")
{
	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_96) == 12);
	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_48) == 6);
	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_32) == 4);
}
