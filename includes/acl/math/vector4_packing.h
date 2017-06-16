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

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint32_t get_packed_vector_size(VectorFormat8 format)
	{
		switch (format)
		{
		case VectorFormat8::Vector3_96:		return sizeof(float) * 3;
		case VectorFormat8::Vector3_48:		return sizeof(uint16_t) * 3;
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			return 0;
		}
	}

	inline void pack_vector(const Vector4_32& vector, VectorFormat8 format, uint8_t* out_vector_data)
	{
		switch (format)
		{
		case VectorFormat8::Vector3_96:		pack_vector3_96(vector, out_vector_data); break;
		case VectorFormat8::Vector3_48:		pack_vector3_48(vector, out_vector_data); break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			break;
		}
	}

	inline Vector4_32 unpack_vector(VectorFormat8 format, const uint8_t* vector_data)
	{
		switch (format)
		{
		case VectorFormat8::Vector3_96:		return unpack_vector3_96(vector_data);
		case VectorFormat8::Vector3_48:		return unpack_vector3_48(vector_data);
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			return vector_zero_32();
		}
	}
}
