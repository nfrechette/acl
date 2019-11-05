#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/core/track_types.h"
#include "acl/math/vector4_32.h"
#include "acl/math/scalar_packing.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// vector4 packing and decay

	inline void ACL_SIMD_CALL pack_vector4_128(Vector4_32Arg0 vector, uint8_t* out_vector_data)
	{
		vector_unaligned_write(vector, out_vector_data);
	}

	inline Vector4_32 ACL_SIMD_CALL unpack_vector4_128(const uint8_t* vector_data)
	{
		return vector_unaligned_load_32(vector_data);
	}

	// Assumes the 'vector_data' is in big-endian order and is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector4_128_unsafe(const uint8_t* vector_data, uint32_t bit_offset)
	{
#if defined(ACL_SSE2_INTRINSICS)
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t x32 = uint32_t(vector_u64);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t y32 = uint32_t(vector_u64);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 8);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t z32 = uint32_t(vector_u64);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 12);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t w32 = uint32_t(vector_u64);

		return _mm_castsi128_ps(_mm_set_epi32(w32, z32, y32, x32));
#elif defined(ACL_NEON_INTRINSICS)
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t x64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;

		const uint64_t y64 = vector_u64 & uint64_t(0xFFFFFFFF00000000ULL);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 8);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t z64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 12);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;

		const uint64_t w64 = vector_u64 & uint64_t(0xFFFFFFFF00000000ULL);

		const uint32x2_t xy = vcreate_u32(x64 | y64);
		const uint32x2_t zw = vcreate_u32(z64 | w64);
		const uint32x4_t value_u32 = vcombine_u32(xy, zw);
		return vreinterpretq_f32_u32(value_u32);
#else
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t x64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t y64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 8);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t z64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 12);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t w64 = vector_u64;

		const float x = aligned_load<float>(&x64);
		const float y = aligned_load<float>(&y64);
		const float z = aligned_load<float>(&z64);
		const float w = aligned_load<float>(&w64);

		return vector_set(x, y, z, w);
#endif
	}

	inline void ACL_SIMD_CALL pack_vector4_64(Vector4_32Arg0 vector, bool is_unsigned, uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), 16) : pack_scalar_signed(vector_get_x(vector), 16);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), 16) : pack_scalar_signed(vector_get_y(vector), 16);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), 16) : pack_scalar_signed(vector_get_z(vector), 16);
		uint32_t vector_w = is_unsigned ? pack_scalar_unsigned(vector_get_w(vector), 16) : pack_scalar_signed(vector_get_w(vector), 16);

		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_x);
		data[1] = safe_static_cast<uint16_t>(vector_y);
		data[2] = safe_static_cast<uint16_t>(vector_z);
		data[3] = safe_static_cast<uint16_t>(vector_w);
	}

	inline Vector4_32 ACL_SIMD_CALL unpack_vector4_64(const uint8_t* vector_data, bool is_unsigned)
	{
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(vector_data);
		uint16_t x16 = data_ptr_u16[0];
		uint16_t y16 = data_ptr_u16[1];
		uint16_t z16 = data_ptr_u16[2];
		uint16_t w16 = data_ptr_u16[3];
		float x = is_unsigned ? unpack_scalar_unsigned(x16, 16) : unpack_scalar_signed(x16, 16);
		float y = is_unsigned ? unpack_scalar_unsigned(y16, 16) : unpack_scalar_signed(y16, 16);
		float z = is_unsigned ? unpack_scalar_unsigned(z16, 16) : unpack_scalar_signed(z16, 16);
		float w = is_unsigned ? unpack_scalar_unsigned(w16, 16) : unpack_scalar_signed(w16, 16);
		return vector_set(x, y, z, w);
	}

	inline void ACL_SIMD_CALL pack_vector4_32(Vector4_32Arg0 vector, bool is_unsigned, uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), 8) : pack_scalar_signed(vector_get_x(vector), 8);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), 8) : pack_scalar_signed(vector_get_y(vector), 8);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), 8) : pack_scalar_signed(vector_get_z(vector), 8);
		uint32_t vector_w = is_unsigned ? pack_scalar_unsigned(vector_get_w(vector), 8) : pack_scalar_signed(vector_get_w(vector), 8);

		out_vector_data[0] = safe_static_cast<uint8_t>(vector_x);
		out_vector_data[1] = safe_static_cast<uint8_t>(vector_y);
		out_vector_data[2] = safe_static_cast<uint8_t>(vector_z);
		out_vector_data[3] = safe_static_cast<uint8_t>(vector_w);
	}

	inline Vector4_32 ACL_SIMD_CALL unpack_vector4_32(const uint8_t* vector_data, bool is_unsigned)
	{
		uint8_t x8 = vector_data[0];
		uint8_t y8 = vector_data[1];
		uint8_t z8 = vector_data[2];
		uint8_t w8 = vector_data[3];
		float x = is_unsigned ? unpack_scalar_unsigned(x8, 8) : unpack_scalar_signed(x8, 8);
		float y = is_unsigned ? unpack_scalar_unsigned(y8, 8) : unpack_scalar_signed(y8, 8);
		float z = is_unsigned ? unpack_scalar_unsigned(z8, 8) : unpack_scalar_signed(z8, 8);
		float w = is_unsigned ? unpack_scalar_unsigned(w8, 8) : unpack_scalar_signed(w8, 8);
		return vector_set(x, y, z, w);
	}

	// Packs data in big-endian order and assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector4_uXX_unsafe(Vector4_32Arg0 vector, uint8_t num_bits, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_unsigned(vector_get_x(vector), num_bits);
		uint32_t vector_y = pack_scalar_unsigned(vector_get_y(vector), num_bits);
		uint32_t vector_z = pack_scalar_unsigned(vector_get_z(vector), num_bits);
		uint32_t vector_w = pack_scalar_unsigned(vector_get_w(vector), num_bits);

		uint64_t vector_u64 = static_cast<uint64_t>(vector_x) << (64 - num_bits * 1);
		vector_u64 |= static_cast<uint64_t>(vector_y) << (64 - num_bits * 2);
		vector_u64 |= static_cast<uint64_t>(vector_z) << (64 - num_bits * 3);
		vector_u64 = byte_swap(vector_u64);

		unaligned_write(vector_u64, out_vector_data);

		uint32_t vector_u32 = vector_w << (32 - num_bits);
		vector_u32 = byte_swap(vector_u32);
		memcpy_bits(out_vector_data, num_bits * 3, &vector_u32, 0, num_bits);
	}

	// Assumes the 'vector_data' is in big-endian order and padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector4_uXX_unsafe(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
	{
		ACL_ASSERT(num_bits <= 19, "This function does not support reading more than 19 bits per component");

		struct PackedTableEntry
		{
			explicit constexpr PackedTableEntry(uint8_t num_bits_)
				: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
				, mask((1 << num_bits_) - 1)
			{}

			float max_value;
			uint32_t mask;
		};

		// TODO: We technically don't need the first 3 entries, which could save a few bytes
		alignas(64) static constexpr PackedTableEntry k_packed_constants[20] =
		{
			PackedTableEntry(0), PackedTableEntry(1), PackedTableEntry(2), PackedTableEntry(3),
			PackedTableEntry(4), PackedTableEntry(5), PackedTableEntry(6), PackedTableEntry(7),
			PackedTableEntry(8), PackedTableEntry(9), PackedTableEntry(10), PackedTableEntry(11),
			PackedTableEntry(12), PackedTableEntry(13), PackedTableEntry(14), PackedTableEntry(15),
			PackedTableEntry(16), PackedTableEntry(17), PackedTableEntry(18), PackedTableEntry(19),
		};

#if defined(ACL_SSE2_INTRINSICS)
		const uint32_t bit_shift = 32 - num_bits;
		const __m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&k_packed_constants[num_bits].mask));
		const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants[num_bits].max_value);

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t w32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		__m128i int_value = _mm_set_epi32(w32, z32, y32, x32);
		int_value = _mm_and_si128(int_value, mask);
		const __m128 value = _mm_cvtepi32_ps(int_value);
		return _mm_mul_ps(value, inv_max_value);
#elif defined(ACL_NEON_INTRINSICS)
		const uint32_t bit_shift = 32 - num_bits;
		uint32x4_t mask = vdupq_n_u32(k_packed_constants[num_bits].mask);
		float inv_max_value = k_packed_constants[num_bits].max_value;

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t w32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		uint32x2_t xy = vcreate_u32(uint64_t(x32) | (uint64_t(y32) << 32));
		uint32x2_t zw = vcreate_u32(uint64_t(z32) | (uint64_t(w32) << 32));
		uint32x4_t value_u32 = vcombine_u32(xy, zw);
		value_u32 = vandq_u32(value_u32, mask);
		float32x4_t value_f32 = vcvtq_f32_u32(value_u32);
		return vmulq_n_f32(value_f32, inv_max_value);
#else
		const uint32_t bit_shift = 32 - num_bits;
		const uint32_t mask = k_packed_constants[num_bits].mask;
		const float inv_max_value = k_packed_constants[num_bits].max_value;

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t w32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		return vector_mul(vector_set(float(x32), float(y32), float(z32), float(w32)), inv_max_value);
#endif
	}

	// Assumes the 'vector_data' is in big-endian order and is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector2_64_unsafe(const uint8_t* vector_data, uint32_t bit_offset)
	{
#if defined(ACL_SSE2_INTRINSICS)
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t x32 = uint32_t(vector_u64);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t y32 = uint32_t(vector_u64);

		// TODO: Convert to u64 first before set1_epi64 or equivalent?
		return _mm_castsi128_ps(_mm_set_epi32(y32, x32, y32, x32));
#elif defined(ACL_NEON_INTRINSICS)
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t x64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;

		const uint64_t y64 = vector_u64 & uint64_t(0xFFFFFFFF00000000ULL);

		const uint32x2_t xy = vcreate_u32(x64 | y64);
		const uint32x4_t value_u32 = vcombine_u32(xy, xy);
		return vreinterpretq_f32_u32(value_u32);
#else
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t x64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t y64 = vector_u64;

		const float x = aligned_load<float>(&x64);
		const float y = aligned_load<float>(&y64);

		return vector_set(x, y, x, y);
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// vector3 packing and decay

	inline void ACL_SIMD_CALL pack_vector3_96(Vector4_32Arg0 vector, uint8_t* out_vector_data)
	{
		vector_unaligned_write3(vector, out_vector_data);
	}

	// Assumes the 'vector_data' is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_96_unsafe(const uint8_t* vector_data)
	{
		return vector_unaligned_load_32(vector_data);
	}

	// Assumes the 'vector_data' is in big-endian order and is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_96_unsafe(const uint8_t* vector_data, uint32_t bit_offset)
	{
#if defined(ACL_SSE2_INTRINSICS)
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t x32 = uint32_t(vector_u64);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t y32 = uint32_t(vector_u64);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 8);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint32_t z32 = uint32_t(vector_u64);

		return _mm_castsi128_ps(_mm_set_epi32(x32, z32, y32, x32));
#elif defined(ACL_NEON_INTRINSICS)
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t x64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;

		const uint64_t y64 = vector_u64 & uint64_t(0xFFFFFFFF00000000ULL);

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 8);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t z64 = vector_u64;

		const uint32x2_t xy = vcreate_u32(x64 | y64);
		const uint32x2_t z = vcreate_u32(z64);
		const uint32x4_t value_u32 = vcombine_u32(xy, z);
		return vreinterpretq_f32_u32(value_u32);
#else
		const uint32_t byte_offset = bit_offset / 8;
		const uint32_t shift_offset = bit_offset % 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t x64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 4);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t y64 = vector_u64;

		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 8);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= shift_offset;
		vector_u64 >>= 32;

		const uint64_t z64 = vector_u64;

		const float x = aligned_load<float>(&x64);
		const float y = aligned_load<float>(&y64);
		const float z = aligned_load<float>(&z64);

		return vector_set(x, y, z);
#endif
	}

	// Assumes the 'vector_data' is in big-endian order and is padded in order to load up to 16 bytes from it
	ACL_DEPRECATED("Use unpack_vector3_96_unsafe instead, to be removed in v2.0")
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_96(const uint8_t* vector_data, uint32_t bit_offset)
	{
		uint32_t byte_offset = bit_offset / 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= bit_offset % 8;
		vector_u64 >>= 64 - 32;

		const uint64_t x64 = vector_u64;

		bit_offset += 32;
		byte_offset = bit_offset / 8;
		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= bit_offset % 8;
		vector_u64 >>= 64 - 32;

		const uint64_t y64 = vector_u64;

		bit_offset += 32;
		byte_offset = bit_offset / 8;
		vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= bit_offset % 8;
		vector_u64 >>= 64 - 32;

		const uint64_t z64 = vector_u64;

		const float x = aligned_load<float>(&x64);
		const float y = aligned_load<float>(&y64);
		const float z = aligned_load<float>(&z64);

		return vector_set(x, y, z);
	}

	ACL_DEPRECATED("Use unpack_vector3_96_unsafe instead, to be removed in v2.0")
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_96(const uint8_t* vector_data)
	{
		return vector_unaligned_load3_32(vector_data);
	}

	// Assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector3_u48_unsafe(Vector4_32Arg0 vector, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_unsigned(vector_get_x(vector), 16);
		uint32_t vector_y = pack_scalar_unsigned(vector_get_y(vector), 16);
		uint32_t vector_z = pack_scalar_unsigned(vector_get_z(vector), 16);

		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_x);
		data[1] = safe_static_cast<uint16_t>(vector_y);
		data[2] = safe_static_cast<uint16_t>(vector_z);
	}

	// Assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector3_s48_unsafe(Vector4_32Arg0 vector, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_signed(vector_get_x(vector), 16);
		uint32_t vector_y = pack_scalar_signed(vector_get_y(vector), 16);
		uint32_t vector_z = pack_scalar_signed(vector_get_z(vector), 16);

		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_x);
		data[1] = safe_static_cast<uint16_t>(vector_y);
		data[2] = safe_static_cast<uint16_t>(vector_z);
	}

	ACL_DEPRECATED("Use pack_vector3_u48_unsafe and pack_vector3_s48_unsafe instead, to be removed in v2.0")
	inline void ACL_SIMD_CALL pack_vector3_48(Vector4_32Arg0 vector, bool is_unsigned, uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), 16) : pack_scalar_signed(vector_get_x(vector), 16);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), 16) : pack_scalar_signed(vector_get_y(vector), 16);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), 16) : pack_scalar_signed(vector_get_z(vector), 16);

		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_x);
		data[1] = safe_static_cast<uint16_t>(vector_y);
		data[2] = safe_static_cast<uint16_t>(vector_z);
	}

	// Assumes the 'vector_data' is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_u48_unsafe(const uint8_t* vector_data)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128i zero = _mm_setzero_si128();
		__m128i x16y16z16 = _mm_loadu_si128((const __m128i*)vector_data);
		__m128i x32y32z32 = _mm_unpacklo_epi16(x16y16z16, zero);
		__m128 value = _mm_cvtepi32_ps(x32y32z32);
		return _mm_mul_ps(value, _mm_set_ps1(1.0F / 65535.0F));
#elif defined(ACL_NEON_INTRINSICS)
		uint8x8_t x8y8z8 = vld1_u8(vector_data);
		uint16x4_t x16y16z16 = vreinterpret_u16_u8(x8y8z8);
		uint32x4_t x32y32z32 = vmovl_u16(x16y16z16);

		float32x4_t value = vcvtq_f32_u32(x32y32z32);
		return vmulq_n_f32(value, 1.0F / 65535.0F);
#else
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(vector_data);
		uint16_t x16 = data_ptr_u16[0];
		uint16_t y16 = data_ptr_u16[1];
		uint16_t z16 = data_ptr_u16[2];
		float x = unpack_scalar_unsigned(x16, 16);
		float y = unpack_scalar_unsigned(y16, 16);
		float z = unpack_scalar_unsigned(z16, 16);
		return vector_set(x, y, z);
#endif
	}

	// Assumes the 'vector_data' is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_s48_unsafe(const uint8_t* vector_data)
	{
		const Vector4_32 unsigned_value = unpack_vector3_u48_unsafe(vector_data);
		return vector_neg_mul_sub(unsigned_value, -2.0F, vector_set(-1.0F));
	}

	ACL_DEPRECATED("Use unpack_vector3_u48_unsafe and unpack_vector3_s48_unsafe instead, to be removed in v2.0")
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_48(const uint8_t* vector_data, bool is_unsigned)
	{
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(vector_data);
		uint16_t x16 = data_ptr_u16[0];
		uint16_t y16 = data_ptr_u16[1];
		uint16_t z16 = data_ptr_u16[2];
		float x = is_unsigned ? unpack_scalar_unsigned(x16, 16) : unpack_scalar_signed(x16, 16);
		float y = is_unsigned ? unpack_scalar_unsigned(y16, 16) : unpack_scalar_signed(y16, 16);
		float z = is_unsigned ? unpack_scalar_unsigned(z16, 16) : unpack_scalar_signed(z16, 16);
		return vector_set(x, y, z);
	}

	inline Vector4_32 ACL_SIMD_CALL decay_vector3_u48(Vector4_32Arg0 input)
	{
		ACL_ASSERT(vector_all_greater_equal3(input, vector_zero_32()) && vector_all_less_equal3(input, vector_set(1.0F)), "Expected normalized unsigned input value: %f, %f, %f", vector_get_x(input), vector_get_y(input), vector_get_z(input));

		const float max_value = float((1 << 16) - 1);
		const float inv_max_value = 1.0F / max_value;

		const Vector4_32 packed = vector_symmetric_round(vector_mul(input, max_value));
		const Vector4_32 decayed = vector_mul(packed, inv_max_value);
		return decayed;
	}

	inline Vector4_32 ACL_SIMD_CALL decay_vector3_s48(Vector4_32Arg0 input)
	{
		const Vector4_32 half = vector_set(0.5F);
		const Vector4_32 unsigned_input = vector_mul_add(input, half, half);

		ACL_ASSERT(vector_all_greater_equal3(unsigned_input, vector_zero_32()) && vector_all_less_equal3(unsigned_input, vector_set(1.0F)), "Expected normalized unsigned input value: %f, %f, %f", vector_get_x(unsigned_input), vector_get_y(unsigned_input), vector_get_z(unsigned_input));

		const float max_value = safe_to_float((1 << 16) - 1);
		const float inv_max_value = 1.0F / max_value;

		const Vector4_32 packed = vector_symmetric_round(vector_mul(unsigned_input, max_value));
		const Vector4_32 decayed = vector_mul(packed, inv_max_value);
		return vector_neg_mul_sub(decayed, -2.0F, vector_set(-1.0F));
	}

	inline void ACL_SIMD_CALL pack_vector3_32(Vector4_32Arg0 vector, uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, uint8_t* out_vector_data)
	{
		ACL_ASSERT(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");

		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), XBits) : pack_scalar_signed(vector_get_x(vector), XBits);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), YBits) : pack_scalar_signed(vector_get_y(vector), YBits);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), ZBits) : pack_scalar_signed(vector_get_z(vector), ZBits);

		uint32_t vector_u32 = (vector_x << (YBits + ZBits)) | (vector_y << ZBits) | vector_z;

		// Written 2 bytes at a time to ensure safe alignment
		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_u32 >> 16);
		data[1] = safe_static_cast<uint16_t>(vector_u32 & 0xFFFF);
	}

	inline Vector4_32 ACL_SIMD_CALL decay_vector3_u32(Vector4_32Arg0 input, uint8_t XBits, uint8_t YBits, uint8_t ZBits)
	{
		ACL_ASSERT(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");
		ACL_ASSERT(vector_all_greater_equal3(input, vector_zero_32()) && vector_all_less_equal(input, vector_set(1.0F)), "Expected normalized unsigned input value: %f, %f, %f", vector_get_x(input), vector_get_y(input), vector_get_z(input));

		const float max_value_x = float((1 << XBits) - 1);
		const float max_value_y = float((1 << YBits) - 1);
		const float max_value_z = float((1 << ZBits) - 1);
		const Vector4_32 max_value = vector_set(max_value_x, max_value_y, max_value_z, max_value_z);
		const Vector4_32 inv_max_value = vector_reciprocal(max_value);

		const Vector4_32 packed = vector_symmetric_round(vector_mul(input, max_value));
		const Vector4_32 decayed = vector_mul(packed, inv_max_value);
		return decayed;
	}

	inline Vector4_32 ACL_SIMD_CALL decay_vector3_s32(Vector4_32Arg0 input, uint8_t XBits, uint8_t YBits, uint8_t ZBits)
	{
		const Vector4_32 half = vector_set(0.5F);
		const Vector4_32 unsigned_input = vector_mul_add(input, half, half);

		ACL_ASSERT(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");
		ACL_ASSERT(vector_all_greater_equal3(unsigned_input, vector_zero_32()) && vector_all_less_equal(unsigned_input, vector_set(1.0F)), "Expected normalized unsigned input value: %f, %f, %f", vector_get_x(unsigned_input), vector_get_y(unsigned_input), vector_get_z(unsigned_input));

		const float max_value_x = float((1 << XBits) - 1);
		const float max_value_y = float((1 << YBits) - 1);
		const float max_value_z = float((1 << ZBits) - 1);
		const Vector4_32 max_value = vector_set(max_value_x, max_value_y, max_value_z, max_value_z);
		const Vector4_32 inv_max_value = vector_reciprocal(max_value);

		const Vector4_32 packed = vector_symmetric_round(vector_mul(unsigned_input, max_value));
		const Vector4_32 decayed = vector_mul(packed, inv_max_value);
		return vector_neg_mul_sub(decayed, -2.0F, vector_set(-1.0F));
	}

	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_32(uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, const uint8_t* vector_data)
	{
		ACL_ASSERT(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");

		// Read 2 bytes at a time to ensure safe alignment
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(vector_data);
		uint32_t vector_u32 = (safe_static_cast<uint32_t>(data_ptr_u16[0]) << 16) | safe_static_cast<uint32_t>(data_ptr_u16[1]);
		uint32_t x32 = vector_u32 >> (YBits + ZBits);
		uint32_t y32 = (vector_u32 >> ZBits) & ((1 << YBits) - 1);
		uint32_t z32 = vector_u32 & ((1 << ZBits) - 1);
		float x = is_unsigned ? unpack_scalar_unsigned(x32, XBits) : unpack_scalar_signed(x32, XBits);
		float y = is_unsigned ? unpack_scalar_unsigned(y32, YBits) : unpack_scalar_signed(y32, YBits);
		float z = is_unsigned ? unpack_scalar_unsigned(z32, ZBits) : unpack_scalar_signed(z32, ZBits);
		return vector_set(x, y, z);
	}

	// Assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector3_u24_unsafe(Vector4_32Arg0 vector, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_unsigned(vector_get_x(vector), 8);
		uint32_t vector_y = pack_scalar_unsigned(vector_get_y(vector), 8);
		uint32_t vector_z = pack_scalar_unsigned(vector_get_z(vector), 8);

		out_vector_data[0] = safe_static_cast<uint8_t>(vector_x);
		out_vector_data[1] = safe_static_cast<uint8_t>(vector_y);
		out_vector_data[2] = safe_static_cast<uint8_t>(vector_z);
	}

	// Assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector3_s24_unsafe(Vector4_32Arg0 vector, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_signed(vector_get_x(vector), 8);
		uint32_t vector_y = pack_scalar_signed(vector_get_y(vector), 8);
		uint32_t vector_z = pack_scalar_signed(vector_get_z(vector), 8);

		out_vector_data[0] = safe_static_cast<uint8_t>(vector_x);
		out_vector_data[1] = safe_static_cast<uint8_t>(vector_y);
		out_vector_data[2] = safe_static_cast<uint8_t>(vector_z);
	}

	ACL_DEPRECATED("Use pack_vector3_u24_unsafe and pack_vector3_s24_unsafe instead, to be removed in v2.0")
	inline void ACL_SIMD_CALL pack_vector3_24(Vector4_32Arg0 vector, bool is_unsigned, uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), 8) : pack_scalar_signed(vector_get_x(vector), 8);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), 8) : pack_scalar_signed(vector_get_y(vector), 8);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), 8) : pack_scalar_signed(vector_get_z(vector), 8);

		out_vector_data[0] = safe_static_cast<uint8_t>(vector_x);
		out_vector_data[1] = safe_static_cast<uint8_t>(vector_y);
		out_vector_data[2] = safe_static_cast<uint8_t>(vector_z);
	}

	// Assumes the 'vector_data' is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_u24_unsafe(const uint8_t* vector_data)
	{
#if defined(ACL_SSE2_INTRINSICS) && 0
		// This implementation leverages fast fixed point coercion, it relies on the
		// input being positive and normalized as well as fixed point (division by 256, not 255)
		// TODO: Enable this, it's a bit faster but requires compensating with the clip range to avoid losing precision
		__m128i zero = _mm_setzero_si128();
		__m128i exponent = _mm_set1_epi32(0x3f800000);

		__m128i x8y8z8 = _mm_loadu_si128((const __m128i*)vector_data);
		__m128i x16y16z16 = _mm_unpacklo_epi8(x8y8z8, zero);
		__m128i x32y32z32 = _mm_unpacklo_epi16(x16y16z16, zero);
		__m128i segment_extent_i32 = _mm_or_si128(_mm_slli_epi32(x32y32z32, 23 - 8), exponent);
		return _mm_sub_ps(_mm_castsi128_ps(segment_extent_i32), _mm_castsi128_ps(exponent));
#elif defined(ACL_SSE2_INTRINSICS)
		__m128i zero = _mm_setzero_si128();
		__m128i x8y8z8 = _mm_loadu_si128((const __m128i*)vector_data);
		__m128i x16y16z16 = _mm_unpacklo_epi8(x8y8z8, zero);
		__m128i x32y32z32 = _mm_unpacklo_epi16(x16y16z16, zero);
		__m128 value = _mm_cvtepi32_ps(x32y32z32);
		return _mm_mul_ps(value, _mm_set_ps1(1.0F / 255.0F));
#elif defined(ACL_NEON_INTRINSICS)
		uint8x8_t x8y8z8 = vld1_u8(vector_data);
		uint16x8_t x16y16z16 = vmovl_u8(x8y8z8);
		uint32x4_t x32y32z32 = vmovl_u16(vget_low_u16(x16y16z16));

		float32x4_t value = vcvtq_f32_u32(x32y32z32);
		return vmulq_n_f32(value, 1.0F / 255.0F);
#else
		uint8_t x8 = vector_data[0];
		uint8_t y8 = vector_data[1];
		uint8_t z8 = vector_data[2];
		float x = unpack_scalar_unsigned(x8, 8);
		float y = unpack_scalar_unsigned(y8, 8);
		float z = unpack_scalar_unsigned(z8, 8);
		return vector_set(x, y, z);
#endif
	}

	// Assumes the 'vector_data' is padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_s24_unsafe(const uint8_t* vector_data)
	{
		const Vector4_32 unsigned_value = unpack_vector3_u24_unsafe(vector_data);
		return vector_neg_mul_sub(unsigned_value, -2.0F, vector_set(-1.0F));
	}

	ACL_DEPRECATED("Use unpack_vector3_u24_unsafe and unpack_vector3_s24_unsafe instead, to be removed in v2.0")
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_24(const uint8_t* vector_data, bool is_unsigned)
	{
		uint8_t x8 = vector_data[0];
		uint8_t y8 = vector_data[1];
		uint8_t z8 = vector_data[2];
		float x = is_unsigned ? unpack_scalar_unsigned(x8, 8) : unpack_scalar_signed(x8, 8);
		float y = is_unsigned ? unpack_scalar_unsigned(y8, 8) : unpack_scalar_signed(y8, 8);
		float z = is_unsigned ? unpack_scalar_unsigned(z8, 8) : unpack_scalar_signed(z8, 8);
		return vector_set(x, y, z);
	}

	// Packs data in big-endian order and assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector3_uXX_unsafe(Vector4_32Arg0 vector, uint8_t num_bits, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_unsigned(vector_get_x(vector), num_bits);
		uint32_t vector_y = pack_scalar_unsigned(vector_get_y(vector), num_bits);
		uint32_t vector_z = pack_scalar_unsigned(vector_get_z(vector), num_bits);

		uint64_t vector_u64 = static_cast<uint64_t>(vector_x) << (64 - num_bits * 1);
		vector_u64 |= static_cast<uint64_t>(vector_y) << (64 - num_bits * 2);
		vector_u64 |= static_cast<uint64_t>(vector_z) << (64 - num_bits * 3);
		vector_u64 = byte_swap(vector_u64);

		unaligned_write(vector_u64, out_vector_data);
	}

	// Packs data in big-endian order and assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector3_sXX_unsafe(Vector4_32Arg0 vector, uint8_t num_bits, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_signed(vector_get_x(vector), num_bits);
		uint32_t vector_y = pack_scalar_signed(vector_get_y(vector), num_bits);
		uint32_t vector_z = pack_scalar_signed(vector_get_z(vector), num_bits);

		uint64_t vector_u64 = static_cast<uint64_t>(vector_x) << (64 - num_bits * 1);
		vector_u64 |= static_cast<uint64_t>(vector_y) << (64 - num_bits * 2);
		vector_u64 |= static_cast<uint64_t>(vector_z) << (64 - num_bits * 3);
		vector_u64 = byte_swap(vector_u64);

		unaligned_write(vector_u64, out_vector_data);
	}

	// Assumes the 'out_vector_data' is padded in order to write up to 8 bytes to it
	ACL_DEPRECATED("Use pack_vector3_uXX_unsafe and pack_vector3_sXX_unsafe instead, to be removed in v2.0")
	inline void ACL_SIMD_CALL pack_vector3_n(Vector4_32Arg0 vector, uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), XBits) : pack_scalar_signed(vector_get_x(vector), XBits);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), YBits) : pack_scalar_signed(vector_get_y(vector), YBits);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), ZBits) : pack_scalar_signed(vector_get_z(vector), ZBits);

		uint64_t vector_u64 = (static_cast<uint64_t>(vector_x) << (YBits + ZBits)) | (static_cast<uint64_t>(vector_y) << ZBits) | static_cast<uint64_t>(vector_z);

		unaligned_write(vector_u64, out_vector_data);
	}

	inline Vector4_32 ACL_SIMD_CALL decay_vector3_uXX(Vector4_32Arg0 input, uint32_t num_bits)
	{
		ACL_ASSERT(vector_all_greater_equal3(input, vector_zero_32()) && vector_all_less_equal3(input, vector_set(1.0F)), "Expected normalized unsigned input value: %f, %f, %f", vector_get_x(input), vector_get_y(input), vector_get_z(input));

		const float max_value = safe_to_float((1 << num_bits) - 1);
		const float inv_max_value = 1.0F / max_value;

		const Vector4_32 packed = vector_symmetric_round(vector_mul(input, max_value));
		const Vector4_32 decayed = vector_mul(packed, inv_max_value);
		return decayed;
	}

	inline Vector4_32 ACL_SIMD_CALL decay_vector3_sXX(Vector4_32Arg0 input, uint32_t num_bits)
	{
		const Vector4_32 half = vector_set(0.5F);
		const Vector4_32 unsigned_input = vector_mul_add(input, half, half);

		ACL_ASSERT(vector_all_greater_equal3(unsigned_input, vector_zero_32()) && vector_all_less_equal3(unsigned_input, vector_set(1.0F)), "Expected normalized unsigned input value: %f, %f, %f", vector_get_x(unsigned_input), vector_get_y(unsigned_input), vector_get_z(unsigned_input));

		const float max_value = safe_to_float((1 << num_bits) - 1);
		const float inv_max_value = 1.0F / max_value;

		const Vector4_32 packed = vector_symmetric_round(vector_mul(unsigned_input, max_value));
		const Vector4_32 decayed = vector_mul(packed, inv_max_value);
		return vector_neg_mul_sub(decayed, -2.0F, vector_set(-1.0F));
	}

	// Assumes the 'vector_data' is in big-endian order and padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_uXX_unsafe(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
	{
		ACL_ASSERT(num_bits <= 19, "This function does not support reading more than 19 bits per component");

		struct PackedTableEntry
		{
			explicit constexpr PackedTableEntry(uint8_t num_bits_)
				: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
				, mask((1 << num_bits_) - 1)
			{}

			float max_value;
			uint32_t mask;
		};

		// TODO: We technically don't need the first 3 entries, which could save a few bytes
		alignas(64) static constexpr PackedTableEntry k_packed_constants[20] =
		{
			PackedTableEntry(0), PackedTableEntry(1), PackedTableEntry(2), PackedTableEntry(3),
			PackedTableEntry(4), PackedTableEntry(5), PackedTableEntry(6), PackedTableEntry(7),
			PackedTableEntry(8), PackedTableEntry(9), PackedTableEntry(10), PackedTableEntry(11),
			PackedTableEntry(12), PackedTableEntry(13), PackedTableEntry(14), PackedTableEntry(15),
			PackedTableEntry(16), PackedTableEntry(17), PackedTableEntry(18), PackedTableEntry(19),
		};

#if defined(ACL_SSE2_INTRINSICS)
		const uint32_t bit_shift = 32 - num_bits;
		const __m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&k_packed_constants[num_bits].mask));
		const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants[num_bits].max_value);

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		__m128i int_value = _mm_set_epi32(x32, z32, y32, x32);
		int_value = _mm_and_si128(int_value, mask);
		const __m128 value = _mm_cvtepi32_ps(int_value);
		return _mm_mul_ps(value, inv_max_value);
#elif defined(ACL_NEON_INTRINSICS)
		const uint32_t bit_shift = 32 - num_bits;
		uint32x4_t mask = vdupq_n_u32(k_packed_constants[num_bits].mask);
		float inv_max_value = k_packed_constants[num_bits].max_value;

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		uint32x2_t xy = vcreate_u32(uint64_t(x32) | (uint64_t(y32) << 32));
		uint32x2_t z = vcreate_u32(uint64_t(z32));
		uint32x4_t value_u32 = vcombine_u32(xy, z);
		value_u32 = vandq_u32(value_u32, mask);
		float32x4_t value_f32 = vcvtq_f32_u32(value_u32);
		return vmulq_n_f32(value_f32, inv_max_value);
#else
		const uint32_t bit_shift = 32 - num_bits;
		const uint32_t mask = k_packed_constants[num_bits].mask;
		const float inv_max_value = k_packed_constants[num_bits].max_value;

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		return vector_mul(vector_set(float(x32), float(y32), float(z32)), inv_max_value);
#endif
	}

	// Assumes the 'vector_data' is in big-endian order and padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_sXX_unsafe(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
	{
		ACL_ASSERT(num_bits * 3 <= 64, "Attempting to read too many bits");

		const Vector4_32 unsigned_value = unpack_vector3_uXX_unsafe(num_bits, vector_data, bit_offset);
		return vector_neg_mul_sub(unsigned_value, -2.0F, vector_set(-1.0F));
	}

	// Assumes the 'vector_data' is in big-endian order and padded in order to load up to 8 bytes from it
	ACL_DEPRECATED("Use unpack_vector3_uXX_unsafe and unpack_vector3_sXX_unsafe instead, to be removed in v2.0")
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_n(uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, const uint8_t* vector_data)
	{
		uint64_t vector_u64 = *safe_ptr_cast<const uint64_t>(vector_data);
		uint32_t x64 = safe_static_cast<uint32_t>(vector_u64 >> (YBits + ZBits));
		uint32_t y64 = safe_static_cast<uint32_t>((vector_u64 >> ZBits) & ((1 << YBits) - 1));
		uint32_t z64 = safe_static_cast<uint32_t>(vector_u64 & ((1 << ZBits) - 1));
		float x = is_unsigned ? unpack_scalar_unsigned(x64, XBits) : unpack_scalar_signed(x64, XBits);
		float y = is_unsigned ? unpack_scalar_unsigned(y64, YBits) : unpack_scalar_signed(y64, YBits);
		float z = is_unsigned ? unpack_scalar_unsigned(z64, ZBits) : unpack_scalar_signed(z64, ZBits);
		return vector_set(x, y, z);
	}

	// Assumes the 'vector_data' is in big-endian order and padded in order to load up to 12 bytes from it
	ACL_DEPRECATED("Use unpack_vector3_uXX_unsafe and unpack_vector3_sXX_unsafe instead, to be removed in v2.0")
	inline Vector4_32 ACL_SIMD_CALL unpack_vector3_n(uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, const uint8_t* vector_data, uint32_t bit_offset)
	{
		uint8_t num_bits_to_read = XBits + YBits + ZBits;

		uint32_t byte_offset = bit_offset / 8;
		uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
		vector_u64 = byte_swap(vector_u64);
		vector_u64 <<= bit_offset % 8;
		vector_u64 >>= 64 - num_bits_to_read;

		const uint32_t x32 = safe_static_cast<uint32_t>(vector_u64 >> (YBits + ZBits));
		const uint32_t y32 = safe_static_cast<uint32_t>((vector_u64 >> ZBits) & ((1 << YBits) - 1));
		uint32_t z32;

		if (num_bits_to_read + (bit_offset % 8) > 64)
		{
			// Larger values can be split over 2x u64 entries
			bit_offset += XBits + YBits;
			byte_offset = bit_offset / 8;
			vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
			vector_u64 = byte_swap(vector_u64);
			vector_u64 <<= bit_offset % 8;
			vector_u64 >>= 64 - ZBits;
			z32 = safe_static_cast<uint32_t>(vector_u64);
		}
		else
			z32 = safe_static_cast<uint32_t>(vector_u64 & ((1 << ZBits) - 1));

		const float x = is_unsigned ? unpack_scalar_unsigned(x32, XBits) : unpack_scalar_signed(x32, XBits);
		const float y = is_unsigned ? unpack_scalar_unsigned(y32, YBits) : unpack_scalar_signed(y32, YBits);
		const float z = is_unsigned ? unpack_scalar_unsigned(z32, ZBits) : unpack_scalar_signed(z32, ZBits);
		return vector_set(x, y, z);
	}

	//////////////////////////////////////////////////////////////////////////
	// vector2 packing and decay

	// Packs data in big-endian order and assumes the 'out_vector_data' is padded in order to write up to 16 bytes to it
	inline void ACL_SIMD_CALL pack_vector2_uXX_unsafe(Vector4_32Arg0 vector, uint8_t num_bits, uint8_t* out_vector_data)
	{
		uint32_t vector_x = pack_scalar_unsigned(vector_get_x(vector), num_bits);
		uint32_t vector_y = pack_scalar_unsigned(vector_get_y(vector), num_bits);

		uint64_t vector_u64 = static_cast<uint64_t>(vector_x) << (64 - num_bits * 1);
		vector_u64 |= static_cast<uint64_t>(vector_y) << (64 - num_bits * 2);
		vector_u64 = byte_swap(vector_u64);

		unaligned_write(vector_u64, out_vector_data);
	}

	// Assumes the 'vector_data' is in big-endian order and padded in order to load up to 16 bytes from it
	inline Vector4_32 ACL_SIMD_CALL unpack_vector2_uXX_unsafe(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
	{
		ACL_ASSERT(num_bits <= 19, "This function does not support reading more than 19 bits per component");

		struct PackedTableEntry
		{
			explicit constexpr PackedTableEntry(uint8_t num_bits_)
				: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
				, mask((1 << num_bits_) - 1)
			{}

			float max_value;
			uint32_t mask;
		};

		// TODO: We technically don't need the first 3 entries, which could save a few bytes
		alignas(64) static constexpr PackedTableEntry k_packed_constants[20] =
		{
			PackedTableEntry(0), PackedTableEntry(1), PackedTableEntry(2), PackedTableEntry(3),
			PackedTableEntry(4), PackedTableEntry(5), PackedTableEntry(6), PackedTableEntry(7),
			PackedTableEntry(8), PackedTableEntry(9), PackedTableEntry(10), PackedTableEntry(11),
			PackedTableEntry(12), PackedTableEntry(13), PackedTableEntry(14), PackedTableEntry(15),
			PackedTableEntry(16), PackedTableEntry(17), PackedTableEntry(18), PackedTableEntry(19),
		};

#if defined(ACL_SSE2_INTRINSICS)
		const uint32_t bit_shift = 32 - num_bits;
		const __m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&k_packed_constants[num_bits].mask));
		const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants[num_bits].max_value);

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		__m128i int_value = _mm_set_epi32(y32, x32, y32, x32);
		int_value = _mm_and_si128(int_value, mask);
		const __m128 value = _mm_cvtepi32_ps(int_value);
		return _mm_mul_ps(value, inv_max_value);
#elif defined(ACL_NEON_INTRINSICS)
		const uint32_t bit_shift = 32 - num_bits;
		uint32x2_t mask = vdup_n_u32(k_packed_constants[num_bits].mask);
		float inv_max_value = k_packed_constants[num_bits].max_value;

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

		uint32x2_t xy = vcreate_u32(uint64_t(x32) | (uint64_t(y32) << 32));
		xy = vand_u32(xy, mask);
		float32x2_t value_f32 = vcvt_f32_u32(xy);
		float32x2_t result = vmul_n_f32(value_f32, inv_max_value);
		return vcombine_f32(result, result);
#else
		const uint32_t bit_shift = 32 - num_bits;
		const uint32_t mask = k_packed_constants[num_bits].mask;
		const float inv_max_value = k_packed_constants[num_bits].max_value;

		uint32_t byte_offset = bit_offset / 8;
		uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		bit_offset += num_bits;

		byte_offset = bit_offset / 8;
		vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
		vector_u32 = byte_swap(vector_u32);
		const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

		return vector_mul(vector_set(float(x32), float(y32), 0.0F, 0.0F), inv_max_value);
#endif
	}

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint32_t get_packed_vector_size(VectorFormat8 format)
	{
		switch (format)
		{
		case VectorFormat8::Vector3_96:		return sizeof(float) * 3;
		case VectorFormat8::Vector3_48:		return sizeof(uint16_t) * 3;
		case VectorFormat8::Vector3_32:		return sizeof(uint32_t);
		case VectorFormat8::Vector3_Variable:
		default:
			ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			return 0;
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
