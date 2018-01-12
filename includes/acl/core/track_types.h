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

#include <cstdint>

namespace acl
{
	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The rotation format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class RotationFormat8 : uint8_t
	{
		Quat_128					= 0,	// Full precision quaternion, [x,y,z,w] stored with float32
		QuatDropW_96				= 1,	// Full precision quaternion, [x,y,z] stored with float32 (w is dropped)
		QuatDropW_48				= 2,	// Quantized quaternion, [x,y,z] stored with [16,16,16] bits (w is dropped)
		QuatDropW_32				= 3,	// Quantized quaternion, [x,y,z] stored with [11,11,10] bits (w is dropped)
		QuatDropW_Variable			= 4,	// Quantized quaternion, [x,y,z] stored with [N,N,N] bits (w is dropped, same number of bits per component)
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
		Vector3_96					= 0,	// Full precision vector3, [x,y,z] stored with float32
		Vector3_48					= 1,	// Quantized vector3, [x,y,z] stored with [16,16,16] bits
		Vector3_32					= 2,	// Quantized vector3, [x,y,z] stored with [11,11,10] bits
		Vector3_Variable			= 3,	// Quantized vector3, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	union TrackFormat8
	{
		RotationFormat8 rotation;
		VectorFormat8 vector;

		TrackFormat8() {}
		TrackFormat8(RotationFormat8 format) : rotation(format) {}
		TrackFormat8(VectorFormat8 format) : vector(format) {}
	};

	enum class AnimationTrackType8 : uint8_t
	{
		Rotation,
		Translation,
		Scale,
	};

	enum class RotationVariant8 : uint8_t
	{
		Quat,
		QuatDropW,
		//QuatDropLargest,
		//QuatLog,
	};

	enum class TimeSeriesType8 : uint8_t
	{
		Constant,
		ConstantDefault,
		Varying,
	};

	//////////////////////////////////////////////////////////////////////////

	// Bit rate 0 is reserved for tracks that are constant in a segment
	constexpr uint8_t k_bit_rate_num_bits[] = { 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 32 };

	constexpr uint8_t k_invalid_bit_rate = 0xFF;
	constexpr uint8_t k_lowest_bit_rate = 1;
	constexpr uint8_t k_highest_bit_rate = sizeof(k_bit_rate_num_bits) - 1;
	constexpr uint8_t k_num_bit_rates = sizeof(k_bit_rate_num_bits);

	static_assert(k_num_bit_rates == 19, "Expecting 19 bit rates");

	// If all tracks are variable, no need for any extra padding except at the very end of the data
	// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
	constexpr uint8_t k_mixed_packing_alignment_num_bits = 16;

	inline uint8_t get_num_bits_at_bit_rate(uint8_t bit_rate)
	{
		ACL_ENSURE(bit_rate <= k_highest_bit_rate, "Invalid bit rate: %u", bit_rate);
		return k_bit_rate_num_bits[bit_rate];
	}

	// Track is constant, our constant sample is stored in the range information
	constexpr bool is_constant_bit_rate(uint8_t bit_rate) { return bit_rate == 0; }
	// Pack 72 really isn't great, it barely improves the error of most clips with high error
	// Disabled for now but code left in to test
	constexpr bool is_pack_72_bit_rate(uint8_t bit_rate) { return false; }
	constexpr bool is_raw_bit_rate(uint8_t bit_rate) { return bit_rate == k_highest_bit_rate; }

	struct BoneBitRate
	{
		uint8_t rotation;
		uint8_t translation;
		uint8_t scale;
	};

	//////////////////////////////////////////////////////////////////////////

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
		case RotationVariant8::QuatDropW:		return RotationFormat8::QuatDropW_32;
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
		case RotationVariant8::QuatDropW:		return RotationFormat8::QuatDropW_96;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %u", (uint32_t)variant);
			return RotationFormat8::Quat_128;
		}
	}

	// TODO: constexpr
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
