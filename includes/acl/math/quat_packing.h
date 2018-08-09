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
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/scalar_packing.h"
#include "acl/math/vector4_packing.h"

#include <cstdint>

namespace acl
{
	inline void pack_quat_128(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		pack_vector4_128(quat_to_vector(rotation), out_rotation_data);
	}

	inline Quat_32 unpack_quat_128(const uint8_t* data_ptr)
	{
		return vector_to_quat(unpack_vector4_128(data_ptr));
	}

	inline void pack_quat_96(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		Vector4_32 rotation_xyz = quat_to_vector(quat_ensure_positive_w(rotation));
		pack_vector3_96(rotation_xyz, out_rotation_data);
	}

	// Assumes the 'data_ptr' is padded in order to load up to 16 bytes from it
	inline Quat_32 unpack_quat_96_unsafe(const uint8_t* data_ptr)
	{
		Vector4_32 rotation_xyz = unpack_vector3_96_unsafe(data_ptr);
		return quat_from_positive_w(rotation_xyz);
	}

	ACL_DEPRECATED("Use unpack_quat_96_unsafe instead, to be removed in v2.0")
	inline Quat_32 unpack_quat_96(const uint8_t* data_ptr)
	{
		Vector4_32 rotation_xyz = vector_unaligned_load3_32(data_ptr);
		return quat_from_positive_w(rotation_xyz);
	}

	inline void pack_quat_48(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		Vector4_32 rotation_xyz = quat_to_vector(quat_ensure_positive_w(rotation));
		pack_vector3_s48_unsafe(rotation_xyz, out_rotation_data);
	}

	inline Quat_32 unpack_quat_48(const uint8_t* data_ptr)
	{
		Vector4_32 rotation_xyz = unpack_vector3_s48_unsafe(data_ptr);
		return quat_from_positive_w(rotation_xyz);
	}

	inline void pack_quat_32(const Quat_32& rotation, uint8_t* out_rotation_data)
	{
		Vector4_32 rotation_xyz = quat_to_vector(quat_ensure_positive_w(rotation));
		pack_vector3_32(rotation_xyz, 11, 11, 10, false, out_rotation_data);
	}

	inline Quat_32 unpack_quat_32(const uint8_t* data_ptr)
	{
		Vector4_32 rotation_xyz = unpack_vector3_32(11, 11, 10, false, data_ptr);
		return quat_from_positive_w(rotation_xyz);
	}

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint32_t get_packed_rotation_size(RotationFormat8 format)
	{
		switch (format)
		{
		case RotationFormat8::Quat_128:	return sizeof(float) * 4;
		case RotationFormat8::QuatDropW_96:	return sizeof(float) * 3;
		case RotationFormat8::QuatDropW_48:	return sizeof(uint16_t) * 3;
		case RotationFormat8::QuatDropW_32:	return sizeof(uint32_t);
		case RotationFormat8::QuatDropW_Variable:
		default:
			ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			return 0;
		}
	}

	inline uint32_t get_range_reduction_rotation_size(RotationFormat8 format)
	{
		switch (format)
		{
		case RotationFormat8::Quat_128:
			return sizeof(float) * 8;
		case RotationFormat8::QuatDropW_96:
		case RotationFormat8::QuatDropW_48:
		case RotationFormat8::QuatDropW_32:
		case RotationFormat8::QuatDropW_Variable:
			return sizeof(float) * 6;
		default:
			ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			return 0;
		}
	}
}
