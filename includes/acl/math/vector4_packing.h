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
#include "acl/core/memory.h"
#include "acl/math/vector4_32.h"
#include "acl/math/scalar_packing.h"

#include <stdint.h>

namespace acl
{
	inline void pack_vector4_128(const Vector4_32& vector, uint8_t* out_vector_data)
	{
		vector_unaligned_write(vector, out_vector_data);
	}

	inline Vector4_32 unpack_vector4_128(const uint8_t* vector_data)
	{
		return vector_unaligned_load(vector_data);
	}

	inline void pack_vector3_96(const Vector4_32& vector, uint8_t* out_vector_data)
	{
		vector_unaligned_write3(vector, out_vector_data);
	}

	inline Vector4_32 unpack_vector3_96(const uint8_t* vector_data)
	{
		return vector_unaligned_load3(vector_data);
	}

	inline void pack_vector3_48(const Vector4_32& vector, uint8_t* out_vector_data)
	{
		size_t vector_x = pack_scalar_signed(vector_get_x(vector), 16);
		size_t vector_y = pack_scalar_signed(vector_get_y(vector), 16);
		size_t vector_z = pack_scalar_signed(vector_get_z(vector), 16);

		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_x);
		data[1] = safe_static_cast<uint16_t>(vector_y);
		data[2] = safe_static_cast<uint16_t>(vector_z);
	}

	inline Vector4_32 unpack_vector3_48(const uint8_t* vector_data)
	{
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(vector_data);
		size_t x = data_ptr_u16[0];
		size_t y = data_ptr_u16[1];
		size_t z = data_ptr_u16[2];
		return vector_set(unpack_scalar_signed(x, 16), unpack_scalar_signed(y, 16), unpack_scalar_signed(z, 16));
	}

	template<size_t XBits, size_t YBits, size_t ZBits>
	inline void pack_vector3_32(const Vector4_32& vector, uint8_t* out_vector_data)
	{
		static_assert(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");

		size_t vector_x = pack_scalar_signed(vector_get_x(vector), XBits);
		size_t vector_y = pack_scalar_signed(vector_get_y(vector), YBits);
		size_t vector_z = pack_scalar_signed(vector_get_z(vector), ZBits);

		uint32_t vector_u32 = safe_static_cast<uint32_t>((vector_x << (YBits + ZBits)) | (vector_y << ZBits) | vector_z);

		// Written 2 bytes at a time to ensure safe alignment
		uint16_t* data = safe_ptr_cast<uint16_t>(out_vector_data);
		data[0] = safe_static_cast<uint16_t>(vector_u32 >> 16);
		data[1] = safe_static_cast<uint16_t>(vector_u32 & 0xFFFF);
	}

	template<size_t XBits, size_t YBits, size_t ZBits>
	inline Vector4_32 unpack_vector3_32(const uint8_t* vector_data)
	{
		static_assert(XBits + YBits + ZBits == 32, "Sum of XYZ bits does not equal 32!");

		// Read 2 bytes at a time to ensure safe alignment
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(vector_data);
		uint32_t vector_u32 = (safe_static_cast<uint32_t>(data_ptr_u16[0]) << 16) | safe_static_cast<uint32_t>(data_ptr_u16[1]);
		size_t x = vector_u32 >> (YBits + ZBits);
		size_t y = (vector_u32 >> ZBits) & ((1 << YBits) - 1);
		size_t z = vector_u32 & ((1 << ZBits) - 1);
		return vector_set(unpack_scalar_signed(x, XBits), unpack_scalar_signed(y, YBits), unpack_scalar_signed(z, ZBits));
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

	constexpr uint32_t get_range_reduction_vector_size(VectorFormat8 format)
	{
		return sizeof(float) * 6;
	}
}
