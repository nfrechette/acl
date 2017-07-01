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

#include "acl/core/enum_utils.h"

#include <stdint.h>

namespace acl
{
	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The algorithm type is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class AlgorithmType8 : uint8_t
	{
		UniformlySampled			= 0,
		//LinearKeyReduction			= 1,
		//SplineKeyReduction			= 2,
	};

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The rotation format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class RotationFormat8 : uint8_t
	{
		Quat_128				= 0,	// Full precision quaternion, [x,y,z,w] stored with float32
		QuatDropW_96			= 1,	// Full precision quaternion, [x,y,z] stored with float32 (w is dropped)
		QuatDropW_48			= 2,	// Quantized quaternion, [x,y,z] stored with [16,16,16] bits (w is dropped)
		QuatDropW_32			= 3,	// Quantized quaternion, [x,y,z] stored with [11,11,10] bits (w is dropped)
		QuatDropW_Variable		= 4,	// Quantized quaternion, [x,y,z] stored with [N,N,N] bits (w is dropped, same number of bits per component)
		// TODO: Implement these
		//Quat_32_Largest,		// Quantized quaternion, [?,?,?] stored with [10,10,10] bits (largest component is dropped, component index stored on 2 bits)
		//QuatLog_96,			// Full precision quaternion logarithm, [x,y,z] stored with float 32
		//QuatLog_48,			// Quantized quaternion logarithm, [x,y,z] stored with [16,16,16] bits
		//QuatLog_32,			// Quantized quaternion logarithm, [x,y,z] stored with [11,11,10] bits
		//QuatLog_Variable,		// Quantized quaternion logarithm, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The vector format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class VectorFormat8 : uint8_t
	{
		Vector3_96				= 0,	// Full precision vector3, [x,y,z] stored with float32
		Vector3_48				= 1,	// Quantized vector3, [x,y,z] stored with [16,16,16] bits
		Vector3_32				= 2,	// Quantized vector3, [x,y,z] stored with [11,11,10] bits
		Vector3_Variable		= 3,	// Quantized vector3, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	union TrackFormat8
	{
		RotationFormat8 rotation;
		VectorFormat8 vector;

		TrackFormat8() {}
		TrackFormat8(RotationFormat8 format) : rotation(format) {}
		TrackFormat8(VectorFormat8 format) : vector(format) {}
	};

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The range reduction strategy is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class RangeReductionFlags8 : uint8_t
	{
		None					= 0x00,

		// Flags to determine which tracks have range reduction applied
		Rotations				= 0x01,
		Translations			= 0x02,
		//Scales					= 0x04,		// TODO: Implement this
		//Properties				= 0x08,		// TODO: Implement this

		// Flags to determine how range reduction behaves
		PerClip					= 0x10,
		//PerSegment				= 0x20,		// TODO: Implement this
	};

	ACL_IMPL_ENUM_FLAGS_OPERATORS(RangeReductionFlags8)

	enum class AnimationTrackType8 : uint8_t
	{
		Rotation = 0,
		Translation = 1,
		// TODO: Scale
	};

	enum class RotationVariant8 : uint8_t
	{
		Quat,
		QuatDropW,
		//QuatDropLargest,
		//QuatLog,
	};

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline bool is_valid_algorithm_type(AlgorithmType8 type)
	{
		switch (type)
		{
			case AlgorithmType8::UniformlySampled:
			//case AlgorithmType8::LinearKeyReduction:
			//case AlgorithmType8::SplineKeyReduction:
				return true;
			default:
				return false;
		}
	}

	// TODO: constexpr
	inline const char* get_algorithm_name(AlgorithmType8 type)
	{
		switch (type)
		{
			case AlgorithmType8::UniformlySampled:		return "Uniformly Sampled";
			//case AlgorithmType8::LinearKeyReduction:	return "Linear Key Reduction";
			//case AlgorithmType8::SplineKeyReduction:	return "Spline Key Reduction";
			default:									return "<Invalid>";
		}
	}

	// TODO: constexpr
	inline const char* get_rotation_format_name(RotationFormat8 format)
	{
		switch (format)
		{
			case RotationFormat8::Quat_128:				return "Quat 128";
			case RotationFormat8::QuatDropW_96:			return "Quat Drop W 96";
			case RotationFormat8::QuatDropW_48:			return "Quat Drop W 48";
			case RotationFormat8::QuatDropW_32:			return "Quat Drop W 32";
			case RotationFormat8::QuatDropW_Variable:	return "Quat Drop W Variable";
			default:									return "<Invalid>";
		}
	}

	// TODO: constexpr
	inline const char* get_vector_format_name(VectorFormat8 format)
	{
		switch (format)
		{
		case VectorFormat8::Vector3_96:			return "Vector3 96";
		case VectorFormat8::Vector3_48:			return "Vector3 48";
		case VectorFormat8::Vector3_32:			return "Vector3 32";
		case VectorFormat8::Vector3_Variable:	return "Vector3 Variable";
		default:								return "<Invalid>";
		}
	}

	// TODO: constexpr
	inline const char* get_range_reduction_name(RangeReductionFlags8 flags)
	{
		switch (flags)
		{
		case RangeReductionFlags8::None:
			return "None";
		case RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations:
			return "Per Clip Rotations";
		case RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations:
			return "Per Clip Translations";
		case RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations:
			return "Per Clip Rotations, Translations";
		default:
			return "<Invalid>";
		}
	}

	// TODO: constexpr
	inline RotationVariant8 get_rotation_variant(RotationFormat8 rotation_format)
	{
		switch (rotation_format)
		{
		case RotationFormat8::Quat_128:
			return RotationVariant8::Quat;
		case RotationFormat8::QuatDropW_96:
		case RotationFormat8::QuatDropW_48:
		case RotationFormat8::QuatDropW_32:
		case RotationFormat8::QuatDropW_Variable:
			return RotationVariant8::QuatDropW;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(rotation_format));
			return RotationVariant8::Quat;
		}
	}

	// TODO: constexpr
	inline RotationFormat8 get_lowest_variant_precision(RotationVariant8 variant)
	{
		switch (variant)
		{
		case RotationVariant8::Quat:			return RotationFormat8::Quat_128;
		case RotationVariant8::QuatDropW:	return RotationFormat8::QuatDropW_32;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %u", (uint32_t)variant);
			return RotationFormat8::Quat_128;
		}
	}

	// TODO: constexpr
	inline RotationFormat8 get_highest_variant_precision(RotationVariant8 variant)
	{
		switch (variant)
		{
		case RotationVariant8::Quat:			return RotationFormat8::Quat_128;
		case RotationVariant8::QuatDropW:	return RotationFormat8::QuatDropW_96;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %u", (uint32_t)variant);
			return RotationFormat8::Quat_128;
		}
	}

	inline bool is_rotation_format_variable(RotationFormat8 rotation_format)
	{
		switch (rotation_format)
		{
		case RotationFormat8::Quat_128:
		case RotationFormat8::QuatDropW_96:
		case RotationFormat8::QuatDropW_48:
		case RotationFormat8::QuatDropW_32:
			return false;
		case RotationFormat8::QuatDropW_Variable:
			return true;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(rotation_format));
			return false;
		}
	}

	constexpr bool is_vector_format_variable(VectorFormat8 format)
	{
		return format == VectorFormat8::Vector3_Variable;
	}
}
