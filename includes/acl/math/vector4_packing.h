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

#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/core/track_types.h"
#include "acl/math/vector4_32.h"
#include "acl/math/scalar_packing.h"

#include <cstdint>

namespace acl
{
	inline void pack_vector4_128(const Vector4_32& vector, uint8_t* out_vector_data)
	{
		vector_unaligned_write(vector, out_vector_data);
	}

	inline Vector4_32 unpack_vector4_128(const uint8_t* vector_data)
	{
		return vector_unaligned_load_32(vector_data);
	}

	inline void pack_vector4_64(const Vector4_32& vector, bool is_unsigned, uint8_t* out_vector_data)
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

	inline Vector4_32 unpack_vector4_64(const uint8_t* vector_data, bool is_unsigned)
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

	inline void pack_vector4_32(const Vector4_32& vector, bool is_unsigned, uint8_t* out_vector_data)
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

	inline Vector4_32 unpack_vector4_32(const uint8_t* vector_data, bool is_unsigned)
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

	inline void pack_vector3_96(const Vector4_32& vector, uint8_t* out_vector_data)
	{
		vector_unaligned_write3(vector, out_vector_data);
	}

	inline Vector4_32 unpack_vector3_96(const uint8_t* vector_data)
	{
		return vector_unaligned_load3_32(vector_data);
	}

	// Assumes the 'vector_data' is in big-endian order
	inline Vector4_32 unpack_vector3_96(const uint8_t* vector_data, uint64_t bit_offset)
	{
		uint64_t byte_offset = bit_offset / 8;
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

	inline void pack_vector3_48(const Vector4_32& vector, bool is_unsigned , uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), 16) : pack_scalar_signed(vector_get_x(vector), 16);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), 16) : pack_scalar_signed(vector_get_y(vector), 16);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), 16) : pack_scalar_signed(vector_get_z(vector), 16);

		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_x);
		data[1] = safe_static_cast<uint16_t>(vector_y);
		data[2] = safe_static_cast<uint16_t>(vector_z);
	}

	inline Vector4_32 unpack_vector3_48(const uint8_t* vector_data, bool is_unsigned)
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

	inline void pack_vector3_32(const Vector4_32& vector, uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, uint8_t* out_vector_data)
	{
		ACL_ENSURE(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");

		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), XBits) : pack_scalar_signed(vector_get_x(vector), XBits);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), YBits) : pack_scalar_signed(vector_get_y(vector), YBits);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), ZBits) : pack_scalar_signed(vector_get_z(vector), ZBits);

		uint32_t vector_u32 = (vector_x << (YBits + ZBits)) | (vector_y << ZBits) | vector_z;

		// Written 2 bytes at a time to ensure safe alignment
		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_u32 >> 16);
		data[1] = safe_static_cast<uint16_t>(vector_u32 & 0xFFFF);
	}

	inline Vector4_32 unpack_vector3_32(uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, const uint8_t* vector_data)
	{
		ACL_ENSURE(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");

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

	inline void pack_vector3_24(const Vector4_32& vector, bool is_unsigned, uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), 8) : pack_scalar_signed(vector_get_x(vector), 8);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), 8) : pack_scalar_signed(vector_get_y(vector), 8);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), 8) : pack_scalar_signed(vector_get_z(vector), 8);

		out_vector_data[0] = safe_static_cast<uint8_t>(vector_x);
		out_vector_data[1] = safe_static_cast<uint8_t>(vector_y);
		out_vector_data[2] = safe_static_cast<uint8_t>(vector_z);
	}

	inline Vector4_32 unpack_vector3_24(const uint8_t* vector_data, bool is_unsigned)
	{
		uint8_t x8 = vector_data[0];
		uint8_t y8 = vector_data[1];
		uint8_t z8 = vector_data[2];
		float x = is_unsigned ? unpack_scalar_unsigned(x8, 8) : unpack_scalar_signed(x8, 8);
		float y = is_unsigned ? unpack_scalar_unsigned(y8, 8) : unpack_scalar_signed(y8, 8);
		float z = is_unsigned ? unpack_scalar_unsigned(z8, 8) : unpack_scalar_signed(z8, 8);
		return vector_set(x, y, z);
	}

	inline void pack_vector3_n(const Vector4_32& vector, uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, uint8_t* out_vector_data)
	{
		uint32_t vector_x = is_unsigned ? pack_scalar_unsigned(vector_get_x(vector), XBits) : pack_scalar_signed(vector_get_x(vector), XBits);
		uint32_t vector_y = is_unsigned ? pack_scalar_unsigned(vector_get_y(vector), YBits) : pack_scalar_signed(vector_get_y(vector), YBits);
		uint32_t vector_z = is_unsigned ? pack_scalar_unsigned(vector_get_z(vector), ZBits) : pack_scalar_signed(vector_get_z(vector), ZBits);

		uint64_t vector_u64 = (static_cast<uint64_t>(vector_x) << (YBits + ZBits)) | (static_cast<uint64_t>(vector_y) << ZBits) | static_cast<uint64_t>(vector_z);

		unaligned_write(vector_u64, out_vector_data);
	}

	inline Vector4_32 unpack_vector3_n(uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, const uint8_t* vector_data)
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

	// Assumes the 'vector_data' is in big-endian order
	inline Vector4_32 unpack_vector3_n(uint8_t XBits, uint8_t YBits, uint8_t ZBits, bool is_unsigned, const uint8_t* vector_data, uint64_t bit_offset)
	{
		uint8_t num_bits_to_read = XBits + YBits + ZBits;

		uint64_t byte_offset = bit_offset / 8;
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
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			return 0;
		}
	}
}
