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

#include <acl/math/vector4_packing.h>

#include <cstring>

using namespace acl;

#if defined(ACL_SSE2_INTRINSICS)
// We need the 4 bytes that contain our value.
// The input is in big-endian order, byte 0 is the first byte
// 320 bytes
static constexpr uint8_t shuffle_values[20][16] =
{
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 0 = num_bits
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 1
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 2
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 3
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 4
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 5
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 6
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 7
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 8
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 9
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 10
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 11
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 12
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 13
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 14
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 15
	{ 3, 2, 1, 0, 3, 2, 1, 0, 7, 6, 5, 4, 0x80, 0x80, 0x80, 0x80 },	// 16
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 17
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 18
	{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 19
};

// 320 bytes
static constexpr uint32_t shift_values[20][4] =
{
	{ 0, 0, 0, 0 },		// 0  = num_bits
	{ 31, 30, 29, 0 },	// 1
	{ 30, 28, 27, 0 },	// 2
	{ 30, 28, 27, 0 },	// 3
	{ 30, 28, 27, 0 },	// 4
	{ 30, 28, 27, 0 },	// 5
	{ 30, 28, 27, 0 },	// 6
	{ 30, 28, 27, 0 },	// 7
	{ 30, 28, 27, 0 },	// 8
	{ 30, 28, 27, 0 },	// 9
	{ 30, 28, 27, 0 },	// 10
	{ 30, 28, 27, 0 },	// 11
	{ 30, 28, 27, 0 },	// 12
	{ 30, 28, 27, 0 },	// 13
	{ 30, 28, 27, 0 },	// 14
	{ 30, 28, 27, 0 },	// 15
	{ 16, 0, 16, 0 },	// 16
	{ 30, 28, 27, 0 },	// 17
	{ 30, 28, 27, 0 },	// 18
	{ 30, 28, 27, 0 },	// 19
};

// 80 bytes
static constexpr uint32_t mask_values[20] =
{
	(1 << 0) - 1, (1 << 1) - 1, (1 << 2) - 1, (1 << 3) - 1,
	(1 << 4) - 1, (1 << 5) - 1, (1 << 6) - 1, (1 << 7) - 1,
	(1 << 8) - 1, (1 << 9) - 1, (1 << 10) - 1, (1 << 11) - 1,
	(1 << 12) - 1, (1 << 13) - 1, (1 << 14) - 1, (1 << 15) - 1,
	(1 << 16) - 1, (1 << 17) - 1, (1 << 18) - 1, (1 << 19) - 1,
};

// 80 bytes
static constexpr float max_values[20] =
{
	1.0f, 1.0f / float((1 << 1) - 1), 1.0f / float((1 << 2) - 1), 1.0f / float((1 << 3) - 1),
	1.0f / float((1 << 4) - 1), 1.0f / float((1 << 5) - 1), 1.0f / float((1 << 6) - 1), 1.0f / float((1 << 7) - 1),
	1.0f / float((1 << 8) - 1), 1.0f / float((1 << 9) - 1), 1.0f / float((1 << 10) - 1), 1.0f / float((1 << 11) - 1),
	1.0f / float((1 << 12) - 1), 1.0f / float((1 << 13) - 1), 1.0f / float((1 << 14) - 1), 1.0f / float((1 << 15) - 1),
	1.0f / float((1 << 16) - 1), 1.0f / float((1 << 17) - 1), 1.0f / float((1 << 18) - 1), 1.0f / float((1 << 19) - 1),
};

Vector4_32 unpack_vector3_n_o12(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	uint32_t byte_offset = bit_offset / 8;
	__m128i bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Select the bytes we need and byte swap them
	__m128i vector_xyz = _mm_shuffle_epi8(bytes, _mm_loadu_si128((const __m128i*)(&shuffle_values[num_bits][0])));

	__m128i shift_offset = _mm_sub_epi32(_mm_loadu_si128((const __m128i*)(&shift_values[num_bits][0])), _mm_set1_epi32(bit_offset % 8));
	__m128i shift_offset_x = _mm_shuffle_epi32(shift_offset, _MM_SHUFFLE(3, 3, 3, 0));
	__m128i shift_offset_y = _mm_shuffle_epi32(shift_offset, _MM_SHUFFLE(3, 3, 3, 1));
	__m128i shift_offset_z = _mm_shuffle_epi32(shift_offset, _MM_SHUFFLE(3, 3, 3, 2));
	__m128i vector_x = _mm_srl_epi32(vector_xyz, shift_offset_x);
	__m128i vector_y = _mm_srl_epi32(vector_xyz, shift_offset_y);
	__m128i vector_z = _mm_srl_epi32(vector_xyz, shift_offset_z);
	__m128i vector_xxyy = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(vector_x), _mm_castsi128_ps(vector_y), _MM_SHUFFLE(1, 1, 0, 0)));
	vector_xyz = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(vector_xxyy), _mm_castsi128_ps(vector_z), _MM_SHUFFLE(2, 2, 2, 0)));

	__m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&mask_values[num_bits]));
	vector_xyz = _mm_and_si128(vector_xyz, mask);

	__m128 value = _mm_cvtepi32_ps(vector_xyz);
	__m128 inv_max_value = _mm_load_ps1(&max_values[num_bits]);
	return _mm_mul_ps(value, inv_max_value);
}
#endif

TEST_CASE("vector4 packing math", "[math][vector4][packing]")
{
	struct UnalignedBuffer
	{
		uint32_t padding0;
		uint16_t padding1;
		uint8_t buffer[250];
	};
	static_assert((offsetof(UnalignedBuffer, buffer) % 2) == 0, "Minimum packing alignment is 2");

	{
		UnalignedBuffer tmp;
		Vector4_32 vec0 = vector_set(6123.123812f, 19237.01293127f, 1891.019231829f, 0.913912387f);
		pack_vector4_128(vec0, &tmp.buffer[0]);
		Vector4_32 vec1 = unpack_vector4_128(&tmp.buffer[0]);
		REQUIRE(std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) == 0);
	}

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
			if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
				num_errors++;

			vec0 = vector_set(value_unsigned);
			pack_vector4_64(vec0, true, &tmp.buffer[0]);
			vec1 = unpack_vector4_64(&tmp.buffer[0], true);
			if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

	{
		UnalignedBuffer tmp;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 256; ++value)
		{
			const float value_signed = unpack_scalar_signed(value, 8);
			const float value_unsigned = unpack_scalar_unsigned(value, 8);

			Vector4_32 vec0 = vector_set(value_signed);
			pack_vector4_32(vec0, false, &tmp.buffer[0]);
			Vector4_32 vec1 = unpack_vector4_32(&tmp.buffer[0], false);
			if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
				num_errors++;

			vec0 = vector_set(value_unsigned);
			pack_vector4_32(vec0, true, &tmp.buffer[0]);
			vec1 = unpack_vector4_32(&tmp.buffer[0], true);
			if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

	{
		UnalignedBuffer tmp0;
		UnalignedBuffer tmp1;
		Vector4_32 vec0 = vector_set(6123.123812f, 19237.01293127f, 0.913912387f);
		pack_vector3_96(vec0, &tmp0.buffer[0]);
		Vector4_32 vec1 = unpack_vector3_96_unsafe(&tmp0.buffer[0]);
		REQUIRE(vector_all_near_equal3(vec0, vec1, 1.0e-6f));

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
			vec1 = unpack_vector3_96(&tmp1.buffer[0], offset);
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

	{
		UnalignedBuffer tmp0;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 65536; ++value)
		{
			const float value_signed = unpack_scalar_signed(value, 16);
			const float value_unsigned = unpack_scalar_unsigned(value, 16);

			Vector4_32 vec0 = vector_set(value_signed, value_signed, value_signed);
			pack_vector3_s48(vec0, &tmp0.buffer[0]);
			Vector4_32 vec1 = unpack_vector3_s48(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;

			vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
			pack_vector3_u48(vec0, &tmp0.buffer[0]);
			vec1 = unpack_vector3_u48_unsafe(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

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
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;

			vec0 = vector_set(value_unsigned_xy, value_unsigned_xy, value_unsigned_z);
			pack_vector3_32(vec0, 11, 11, 10, true, &tmp0.buffer[0]);
			vec1 = unpack_vector3_32(11, 11, 10, true, &tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

	{
		UnalignedBuffer tmp0;
		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 256; ++value)
		{
			const float value_signed = unpack_scalar_signed(value, 8);
			const float value_unsigned = unpack_scalar_unsigned(value, 8);

			Vector4_32 vec0 = vector_set(value_signed, value_signed, value_signed);
			pack_vector3_s24(vec0, &tmp0.buffer[0]);
			Vector4_32 vec1 = unpack_vector3_s24(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;

			vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
			pack_vector3_u24(vec0, &tmp0.buffer[0]);
			vec1 = unpack_vector3_u24_unsafe(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

	{
		UnalignedBuffer tmp0;
		alignas(16) uint8_t buffer[64];

		uint32_t num_errors = 0;
		Vector4_32 vec0 = vector_set(unpack_scalar_signed(0, 16), unpack_scalar_signed(12355, 16), unpack_scalar_signed(43222, 16));
		pack_vector3_sXX(vec0, 16, &buffer[0]);
		Vector4_32 vec1 = unpack_vector3_sXX_unsafe(16, &buffer[0], 0);
		if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
			num_errors++;

		vec0 = vector_set(unpack_scalar_unsigned(0, 16), unpack_scalar_unsigned(12355, 16), unpack_scalar_unsigned(43222, 16));
		pack_vector3_uXX(vec0, 16, &buffer[0]);
		vec1 = unpack_vector3_uXX_unsafe(16, &buffer[0], 0);
		if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
			num_errors++;

#if defined(ACL_SSE2_INTRINSICS)
		// HACK
		vec1 = unpack_vector3_n_o12(16, &buffer[0], 0);
		if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
			num_errors++;
		// HACK
#endif

		for (uint8_t bit_rate = 1; bit_rate < k_highest_bit_rate; ++bit_rate)
		{
			uint8_t num_bits = get_num_bits_at_bit_rate(bit_rate);
			uint32_t max_value = (1 << num_bits) - 1;
			for (uint32_t value = 0; value <= max_value; ++value)
			{
				const float value_signed = unpack_scalar_signed(value, num_bits);
				const float value_unsigned = unpack_scalar_unsigned(value, num_bits);

				vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
				pack_vector3_uXX(vec0, num_bits, &buffer[0]);
				vec1 = unpack_vector3_uXX_unsafe(num_bits, &buffer[0], 0);
				if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
					num_errors++;

				{
					const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
					for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
					{
						const uint8_t offset = offsets[offset_idx];

						memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, num_bits * 3);
						vec1 = unpack_vector3_uXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
							num_errors++;
					}
				}

				vec0 = vector_set(value_signed, value_signed, value_signed);
				pack_vector3_sXX(vec0, num_bits, &buffer[0]);
				vec1 = unpack_vector3_sXX_unsafe(num_bits, &buffer[0], 0);
				if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
					num_errors++;

				{
					const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
					for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
					{
						const uint8_t offset = offsets[offset_idx];

						memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, num_bits * 3);
						vec1 = unpack_vector3_sXX_unsafe(num_bits, &tmp0.buffer[0], offset);
						if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
							num_errors++;
					}
				}
			}
		}
		REQUIRE(num_errors == 0);
	}

	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_96) == 12);
	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_48) == 6);
	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_32) == 4);

}
