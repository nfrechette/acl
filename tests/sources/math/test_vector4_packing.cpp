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
			if (std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) != 0)
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
			if (std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) != 0)
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

	{
		UnalignedBuffer tmp0;
		UnalignedBuffer tmp1;
		Vector4_32 vec0 = vector_set(6123.123812f, 19237.01293127f, 0.913912387f);
		pack_vector3_96(vec0, &tmp0.buffer[0]);
		Vector4_32 vec1 = unpack_vector3_96(&tmp0.buffer[0]);
		REQUIRE(std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) == 0);

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
			if (std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) != 0)
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
			pack_vector3_48(vec0, false, &tmp0.buffer[0]);
			Vector4_32 vec1 = unpack_vector3_48(&tmp0.buffer[0], false);
			if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
				num_errors++;

			vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
			pack_vector3_48(vec0, true, &tmp0.buffer[0]);
			vec1 = unpack_vector3_48(&tmp0.buffer[0], true);
			if (std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) != 0)
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
			if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
				num_errors++;

			vec0 = vector_set(value_unsigned_xy, value_unsigned_xy, value_unsigned_z);
			pack_vector3_32(vec0, 11, 11, 10, true, &tmp0.buffer[0]);
			vec1 = unpack_vector3_32(11, 11, 10, true, &tmp0.buffer[0]);
			if (std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) != 0)
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
			vec1 = unpack_vector3_u24(&tmp0.buffer[0]);
			if (!vector_all_near_equal3(vec0, vec1, 1.0e-6f))
				num_errors++;
		}
		REQUIRE(num_errors == 0);
	}

	{
		UnalignedBuffer tmp0;
		alignas(8) uint8_t buffer[64];

		uint32_t num_errors = 0;
		for (uint32_t value = 0; value < 65536; ++value)
		{
			const float value_signed = unpack_scalar_signed(value, 16);
			const float value_unsigned = unpack_scalar_unsigned(value, 16);

			Vector4_32 vec0 = vector_set(value_signed, value_signed, value_signed);
			pack_vector3_n(vec0, 16, 16, 16, false, &buffer[0]);
			Vector4_32 vec1 = unpack_vector3_n(16, 16, 16, false, &buffer[0]);
			if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
				num_errors++;

			{
				uint16_t x = unaligned_load<uint16_t>(&buffer[0]);
				x = byte_swap(x);
				unaligned_write(x, &buffer[0]);

				uint16_t y = unaligned_load<uint16_t>(&buffer[2]);
				y = byte_swap(y);
				unaligned_write(y, &buffer[2]);

				uint16_t z = unaligned_load<uint16_t>(&buffer[4]);
				z = byte_swap(z);
				unaligned_write(z, &buffer[4]);

				const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
				for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
				{
					const uint8_t offset = offsets[offset_idx];

					memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, 48);
					vec1 = unpack_vector3_n(16, 16, 16, false, &tmp0.buffer[0], offset);
					if (!vector_all_near_equal(vec0, vec1, 1.0e-6f))
						num_errors++;
				}
			}

			vec0 = vector_set(value_unsigned, value_unsigned, value_unsigned);
			pack_vector3_n(vec0, 16, 16, 16, true, &buffer[0]);
			vec1 = unpack_vector3_n(16, 16, 16, true, &buffer[0]);
			if (std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) != 0)
				num_errors++;

			{
				uint16_t x = unaligned_load<uint16_t>(&buffer[0]);
				x = byte_swap(x);
				unaligned_write(x, &buffer[0]);

				uint16_t y = unaligned_load<uint16_t>(&buffer[2]);
				y = byte_swap(y);
				unaligned_write(y, &buffer[2]);

				uint16_t z = unaligned_load<uint16_t>(&buffer[4]);
				z = byte_swap(z);
				unaligned_write(z, &buffer[4]);

				const uint8_t offsets[] = { 0, 1, 5, 31, 32, 33, 63, 64, 65, 93 };
				for (uint8_t offset_idx = 0; offset_idx < get_array_size(offsets); ++offset_idx)
				{
					const uint8_t offset = offsets[offset_idx];

					memcpy_bits(&tmp0.buffer[0], offset, &buffer[0], 0, 48);
					vec1 = unpack_vector3_n(16, 16, 16, true, &tmp0.buffer[0], offset);
					if (std::memcmp(&vec0, &vec1, sizeof(Vector4_32)) != 0)
						num_errors++;
				}
			}
		}
		REQUIRE(num_errors == 0);
	}

	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_96) == 12);
	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_48) == 6);
	REQUIRE(get_packed_vector_size(VectorFormat8::Vector3_32) == 4);

}
