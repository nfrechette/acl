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
#include "acl/core/algorithm_types.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/scalar_packing.h"

#include <stdint.h>

namespace acl
{
	inline void pack_quat_128(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		quat_unaligned_write(rotation, out_rotation_data);
	}

	inline Quat_32 unpack_quat_128(const uint8_t* data_ptr)
	{
		return quat_unaligned_load(data_ptr);
	}

	inline void pack_quat_96(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		Vector4_32 rotation_xyz = quat_to_vector(quat_ensure_positive_w(rotation));
		vector_unaligned_write3(rotation_xyz, out_rotation_data);
	}

	inline Quat_32 unpack_quat_96(const uint8_t* data_ptr)
	{
		Vector4_32 rotation_xyz = vector_unaligned_load3(data_ptr);
		return quat_from_positive_w(rotation_xyz);
	}

	inline void pack_quat_48(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		Vector4_32 rotation_xyz = quat_to_vector(quat_ensure_positive_w(rotation));

		size_t rotation_x = pack_scalar_signed(vector_get_x(rotation_xyz), 16);
		size_t rotation_y = pack_scalar_signed(vector_get_y(rotation_xyz), 16);
		size_t rotation_z = pack_scalar_signed(vector_get_z(rotation_xyz), 16);

		uint16_t* data = safe_ptr_cast<uint16_t>(out_rotation_data);
		data[0] = safe_static_cast<uint16_t>(rotation_x);
		data[1] = safe_static_cast<uint16_t>(rotation_y);
		data[2] = safe_static_cast<uint16_t>(rotation_z);
	}

	inline Quat_32 unpack_quat_48(const uint8_t* data_ptr)
	{
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(data_ptr);
		size_t x = data_ptr_u16[0];
		size_t y = data_ptr_u16[1];
		size_t z = data_ptr_u16[2];
		Vector4_32 rotation_xyz = vector_set(unpack_scalar_signed(x, 16), unpack_scalar_signed(y, 16), unpack_scalar_signed(z, 16));
		return quat_from_positive_w(rotation_xyz);
	}

	inline void pack_quat_32(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		Vector4_32 rotation_xyz = quat_to_vector(quat_ensure_positive_w(rotation));

		size_t rotation_x = pack_scalar_signed(vector_get_x(rotation_xyz), 11);
		size_t rotation_y = pack_scalar_signed(vector_get_y(rotation_xyz), 11);
		size_t rotation_z = pack_scalar_signed(vector_get_z(rotation_xyz), 10);

		uint32_t rotation_u32 = safe_static_cast<uint32_t>((rotation_x << 21) | (rotation_y << 10) | rotation_z);

		// Written 2 bytes at a time to ensure safe alignment
		uint16_t* data = safe_ptr_cast<uint16_t>(out_rotation_data);
		data[0] = safe_static_cast<uint16_t>(rotation_u32 >> 16);
		data[1] = safe_static_cast<uint16_t>(rotation_u32 & 0xFFFF);
	}

	inline Quat_32 unpack_quat_32(const uint8_t* data_ptr)
	{
		// Read 2 bytes at a time to ensure safe alignment
		const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(data_ptr);
		uint32_t rotation_u32 = (safe_static_cast<uint32_t>(data_ptr_u16[0]) << 16) | safe_static_cast<uint32_t>(data_ptr_u16[1]);
		size_t x = rotation_u32 >> 21;
		size_t y = (rotation_u32 >> 10) & ((1 << 11) - 1);
		size_t z = rotation_u32 & ((1 << 10) - 1);
		Vector4_32 rotation_xyz = vector_set(unpack_scalar_signed(x, 11), unpack_scalar_signed(y, 11), unpack_scalar_signed(z, 10));
		return quat_from_positive_w(rotation_xyz);
	}

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint32_t get_packed_rotation_size(RotationFormat8 format)
	{
		switch (format)
		{
		case RotationFormat8::Quat_128:	return sizeof(float) * 4;
		case RotationFormat8::Quat_96:	return sizeof(float) * 3;
		case RotationFormat8::Quat_48:	return sizeof(uint16_t) * 3;
		case RotationFormat8::Quat_32:	return sizeof(uint32_t);
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			return 0;
		}
	}

	inline void pack_rotation(const Quat_32& rotation, RotationFormat8 format, uint8_t* out_rotation_data)
	{
		switch (format)
		{
		case RotationFormat8::Quat_128:		pack_quat_128(rotation, out_rotation_data); break;
		case RotationFormat8::Quat_96:		pack_quat_96(rotation, out_rotation_data); break;
		case RotationFormat8::Quat_48:		pack_quat_48(rotation, out_rotation_data); break;
		case RotationFormat8::Quat_32:		pack_quat_32(rotation, out_rotation_data); break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			break;
		}
	}

	inline Quat_32 unpack_rotation(RotationFormat8 format, const uint8_t* rotation_data)
	{
		switch (format)
		{
		case RotationFormat8::Quat_128:		return unpack_quat_128(rotation_data);
		case RotationFormat8::Quat_96:		return unpack_quat_96(rotation_data);
		case RotationFormat8::Quat_48:		return unpack_quat_48(rotation_data);
		case RotationFormat8::Quat_32:		return unpack_quat_32(rotation_data);
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			return quat_identity_32();
		}
	}
}
