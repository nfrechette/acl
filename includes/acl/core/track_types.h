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

#include "acl/core/error_result.h"
#include "acl/core/memory_utils.h"
#include "acl/core/impl/compiler_utils.h"

#include <rtm/scalarf.h>

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The rotation format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class rotation_format8 : uint8_t
	{
		quatf_full					= 0,	// Full precision quaternion, [x,y,z,w] stored with float32
		//quatf_variable			= 1,	// TODO Quantized quaternion, [x,y,z,w] stored with [N,N,N,N] bits (same number of bits per component)
		quatf_drop_w_full			= 2,	// Full precision quaternion, [x,y,z] stored with float32 (w is dropped)
		quatf_drop_w_variable		= 3,	// Quantized quaternion, [x,y,z] stored with [N,N,N] bits (w is dropped, same number of bits per component)

		//quatf_optimal				= 15,	// Mix of quatf_variable and quatf_drop_w_variable

		// TODO: Implement these?
		//quatf_drop_largest_full			// Full precision quaternion, [a,b,c] stored with float32 (largest component dropped)
		//quatf_drop_largest_variable		// Quantized quaternion, [a,b,c] stored with [N,N,N] bits (largest component dropped, same number of bits per component)
		//quatf_log_full,					// Full precision quaternion logarithm, [x,y,z] stored with float32
		//quatf_log_variable,				// Quantized quaternion logarithm, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The vector format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class vector_format8 : uint8_t
	{
		vector3f_full				= 0,	// Full precision vector3f, [x,y,z] stored with float32
		vector3f_variable			= 1,	// Quantized vector3f, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	union track_format8
	{
		rotation_format8 rotation;
		vector_format8 vector;

		track_format8() {}
		explicit track_format8(rotation_format8 format) : rotation(format) {}
		explicit track_format8(vector_format8 format) : vector(format) {}
	};

	enum class animation_track_type8 : uint8_t
	{
		rotation,
		translation,
		scale,
	};

	enum class rotation_variant8 : uint8_t
	{
		quat,
		quat_drop_w,
		//quat_drop_largest,
		//quat_log,
	};

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// We only support up to 4294967295 tracks. We reserve 4294967295 for the invalid index
	constexpr uint32_t k_invalid_track_index = 0xFFFFFFFFU;

	//////////////////////////////////////////////////////////////////////////
	// The various supported track types.
	// Note: be careful when changing values here as they might be serialized.
	enum class track_type8 : uint8_t
	{
		float1f		= 0,
		float2f		= 1,
		float3f		= 2,
		float4f		= 3,
		vector4f	= 4,

		//float1d	= 5,
		//float2d	= 6,
		//float3d	= 7,
		//float4d	= 8,
		//vector4d	= 9,

		//quatf		= 10,
		//quatd		= 11,

		qvvf		= 12,
		//qvvd		= 13,

		//int1i		= 14,
		//int2i		= 15,
		//int3i		= 16,
		//int4i		= 17,
		//vector4i	= 18,

		//int1q		= 19,
		//int2q		= 20,
		//int3q		= 21,
		//int4q		= 22,
		//vector4q	= 23,
	};

	//////////////////////////////////////////////////////////////////////////
	// The categories of track types.
	enum class track_category8 : uint8_t
	{
		scalarf		= 0,
		scalard		= 1,
		//scalari	= 2,
		//scalarq	= 3,
		transformf	= 4,
		transformd	= 5,
	};

	//////////////////////////////////////////////////////////////////////////
	// This structure describes the various settings for floating point scalar tracks.
	// Used by: float1f, float2f, float3f, float4f, vector4f
	struct track_desc_scalarf
	{
		//////////////////////////////////////////////////////////////////////////
		// The track category for this description.
		static constexpr track_category8 category = track_category8::scalarf;

		//////////////////////////////////////////////////////////////////////////
		// The track output index. When writing out the compressed data stream, this index
		// will be used instead of the track index. This allows custom reordering for things
		// like LOD sorting or skeleton remapping. A value of 'k_invalid_track_index' will strip the track
		// from the compressed data stream. Output indices must be unique and contiguous.
		uint32_t output_index = k_invalid_track_index;

		//////////////////////////////////////////////////////////////////////////
		// The per component precision threshold to try and attain when optimizing the bit rate.
		// If the error is below the precision threshold, we will remove bits until we reach it without
		// exceeding it. If the error is above the precision threshold, we will add more bits until
		// we lower it underneath.
		// Defaults to '0.00001'
		float precision = 0.00001F;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a scalar track description is valid or not.
		// It is valid if:
		//    - The precision is positive or zero and finite
		error_result is_valid() const
		{
			if (precision < 0.0F || !rtm::scalar_is_finite(precision))
				return error_result("Invalid precision");

			return error_result();
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// This structure describes the various settings for transform tracks.
	// Used by: quatf, qvvf
	struct track_desc_transformf
	{
		//////////////////////////////////////////////////////////////////////////
		// The track category for this description.
		static constexpr track_category8 category = track_category8::transformf;

		//////////////////////////////////////////////////////////////////////////
		// The track output index. When writing out the compressed data stream, this index
		// will be used instead of the track index. This allows custom reordering for things
		// like LOD sorting or skeleton remapping. A value of 'k_invalid_track_index' will strip the track
		// from the compressed data stream. Output indices must be unique and contiguous.
		uint32_t output_index = k_invalid_track_index;

		//////////////////////////////////////////////////////////////////////////
		// The index of the parent transform track or `k_invalid_track_index` if it has no parent.
		uint32_t parent_index = k_invalid_track_index;

		//////////////////////////////////////////////////////////////////////////
		// The shell precision threshold to try and attain when optimizing the bit rate.
		// If the error is below the precision threshold, we will remove bits until we reach it without
		// exceeding it. If the error is above the precision threshold, we will add more bits until
		// we lower it underneath.
		// Note that you will need to change this value if your units are not in centimeters.
		// Defaults to '0.01' centimeters
		float precision = 0.01F;

		//////////////////////////////////////////////////////////////////////////
		// The error is measured on a rigidly deformed shell around every transform at the specified distance.
		// Defaults to '3.0' centimeters
		float shell_distance = 3.0F;

		//////////////////////////////////////////////////////////////////////////
		// TODO: Use the precision and shell distance?

		//////////////////////////////////////////////////////////////////////////
		// Threshold angle when detecting if rotation tracks are constant or default.
		// See the rtm::quatf quat_near_identity for details about how the default threshold
		// was chosen. You will typically NEVER need to change this, the value has been
		// selected to be as safe as possible and is independent of game engine units.
		// Defaults to '0.00284714461' radians
		float constant_rotation_threshold_angle = 0.00284714461F;

		//////////////////////////////////////////////////////////////////////////
		// Threshold value to use when detecting if translation tracks are constant or default.
		// Note that you will need to change this value if your units are not in centimeters.
		// Defaults to '0.001' centimeters.
		float constant_translation_threshold = 0.001F;

		//////////////////////////////////////////////////////////////////////////
		// Threshold value to use when detecting if scale tracks are constant or default.
		// There are no units for scale as such a value that was deemed safe was selected
		// as a default.
		// Defaults to '0.00001'
		float constant_scale_threshold = 0.00001F;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a transform track description is valid or not.
		// It is valid if:
		//    - The precision is positive or zero and finite
		//    - The shell distance is positive or zero and finite
		//    - The constant rotation threshold angle is positive or zero and finite
		//    - The constant translation threshold is positive or zero and finite
		//    - The constant scale threshold is positive or zero and finite
		error_result is_valid() const
		{
			if (precision < 0.0F || !rtm::scalar_is_finite(precision))
				return error_result("Invalid precision");

			if (shell_distance < 0.0F || !rtm::scalar_is_finite(shell_distance))
				return error_result("Invalid shell_distance");

			if (constant_rotation_threshold_angle < 0.0F || !rtm::scalar_is_finite(constant_rotation_threshold_angle))
				return error_result("Invalid constant_rotation_threshold_angle");

			if (constant_translation_threshold < 0.0F || !rtm::scalar_is_finite(constant_translation_threshold))
				return error_result("Invalid constant_translation_threshold");

			if (constant_scale_threshold < 0.0F || !rtm::scalar_is_finite(constant_scale_threshold))
				return error_result("Invalid constant_scale_threshold");

			return error_result();
		}
	};

	//////////////////////////////////////////////////////////////////////////

	// Bit rate 0 is reserved for tracks that are constant in a segment
	constexpr uint8_t k_bit_rate_num_bits[] = { 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 32 };

	constexpr uint8_t k_invalid_bit_rate = 0xFF;
	constexpr uint8_t k_lowest_bit_rate = 1;
	constexpr uint8_t k_highest_bit_rate = sizeof(k_bit_rate_num_bits) - 1;
	constexpr uint32_t k_num_bit_rates = sizeof(k_bit_rate_num_bits);

	static_assert(k_num_bit_rates == 19, "Expecting 19 bit rates");

	inline uint32_t get_num_bits_at_bit_rate(uint8_t bit_rate)
	{
		ACL_ASSERT(bit_rate <= k_highest_bit_rate, "Invalid bit rate: %u", bit_rate);
		return k_bit_rate_num_bits[bit_rate];
	}

	// Track is constant, our constant sample is stored in the range information
	constexpr bool is_constant_bit_rate(uint8_t bit_rate) { return bit_rate == 0; }
	constexpr bool is_raw_bit_rate(uint8_t bit_rate) { return bit_rate == k_highest_bit_rate; }

	struct BoneBitRate
	{
		uint8_t rotation;
		uint8_t translation;
		uint8_t scale;
	};

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline const char* get_rotation_format_name(rotation_format8 format)
	{
		switch (format)
		{
		case rotation_format8::quatf_full:				return "quatf_full";
		case rotation_format8::quatf_drop_w_full:		return "quatf_drop_w_full";
		case rotation_format8::quatf_drop_w_variable:	return "quatf_drop_w_variable";
		default:										return "<Invalid>";
		}
	}

	inline bool get_rotation_format(const char* format, rotation_format8& out_format)
	{
		const char* quat_128_format = "Quat_128";	// ACL_DEPRECATED Legacy name, keep for backwards compatibility, remove in 3.0
		const char* quatf_full_format = "quatf_full";
		if (std::strncmp(format, quat_128_format, std::strlen(quat_128_format)) == 0
			|| std::strncmp(format, quatf_full_format, std::strlen(quatf_full_format)) == 0)
		{
			out_format = rotation_format8::quatf_full;
			return true;
		}

		const char* quatdropw_96_format = "QuatDropW_96";	// ACL_DEPRECATED Legacy name, keep for backwards compatibility, remove in 3.0
		const char* quatf_drop_w_full_format = "quatf_drop_w_full";
		if (std::strncmp(format, quatdropw_96_format, std::strlen(quatdropw_96_format)) == 0
			|| std::strncmp(format, quatf_drop_w_full_format, std::strlen(quatf_drop_w_full_format)) == 0)
		{
			out_format = rotation_format8::quatf_drop_w_full;
			return true;
		}

		const char* quatdropw_variable_format = "QuatDropW_Variable";	// ACL_DEPRECATED Legacy name, keep for backwards compatibility, remove in 3.0
		const char* quatf_drop_w_variable_format = "quatf_drop_w_variable";
		if (std::strncmp(format, quatdropw_variable_format, std::strlen(quatdropw_variable_format)) == 0
			|| std::strncmp(format, quatf_drop_w_variable_format, std::strlen(quatf_drop_w_variable_format)) == 0)
		{
			out_format = rotation_format8::quatf_drop_w_variable;
			return true;
		}

		return false;
	}

	// TODO: constexpr
	inline const char* get_vector_format_name(vector_format8 format)
	{
		switch (format)
		{
		case vector_format8::vector3f_full:			return "vector3f_full";
		case vector_format8::vector3f_variable:		return "vector3f_variable";
		default:									return "<Invalid>";
		}
	}

	inline bool get_vector_format(const char* format, vector_format8& out_format)
	{
		const char* vector3_96_format = "Vector3_96";	// ACL_DEPRECATED Legacy name, keep for backwards compatibility, remove in 3.0
		const char* vector3f_full_format = "vector3f_full";
		if (std::strncmp(format, vector3_96_format, std::strlen(vector3_96_format)) == 0
			|| std::strncmp(format, vector3f_full_format, std::strlen(vector3f_full_format)) == 0)
		{
			out_format = vector_format8::vector3f_full;
			return true;
		}

		const char* vector3_variable_format = "Vector3_Variable";	// ACL_DEPRECATED Legacy name, keep for backwards compatibility, remove in 3.0
		const char* vector3f_variable_format = "vector3f_variable";
		if (std::strncmp(format, vector3_variable_format, std::strlen(vector3_variable_format)) == 0
			|| std::strncmp(format, vector3f_variable_format, std::strlen(vector3f_variable_format)) == 0)
		{
			out_format = vector_format8::vector3f_variable;
			return true;
		}

		return false;
	}

	constexpr rotation_variant8 get_rotation_variant(rotation_format8 rotation_format)
	{
		return rotation_format == rotation_format8::quatf_full ? rotation_variant8::quat : rotation_variant8::quat_drop_w;
	}

	constexpr rotation_format8 get_highest_variant_precision(rotation_variant8 variant)
	{
		return variant == rotation_variant8::quat ? rotation_format8::quatf_full : rotation_format8::quatf_drop_w_full;
	}

	constexpr bool is_rotation_format_variable(rotation_format8 format)
	{
		return format == rotation_format8::quatf_drop_w_variable;
	}

	constexpr bool is_rotation_format_full_precision(rotation_format8 format)
	{
		return format == rotation_format8::quatf_full || format == rotation_format8::quatf_drop_w_full;
	}

	constexpr bool is_vector_format_variable(vector_format8 format)
	{
		return format == vector_format8::vector3f_variable;
	}

	constexpr bool is_vector_format_full_precision(vector_format8 format)
	{
		return format == vector_format8::vector3f_full;
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns the string representation for the provided track type.
	// TODO: constexpr
	inline const char* get_track_type_name(track_type8 type)
	{
		switch (type)
		{
		case track_type8::float1f:			return "float1f";
		case track_type8::float2f:			return "float2f";
		case track_type8::float3f:			return "float3f";
		case track_type8::float4f:			return "float4f";
		case track_type8::vector4f:			return "vector4f";
		case track_type8::qvvf:				return "qvvf";
		default:							return "<Invalid>";
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns the track type from its string representation.
	// Returns true on success, false otherwise.
	inline bool get_track_type(const char* type, track_type8& out_type)
	{
		// Entries in the same order as the enum integral value
		static const char* k_track_type_names[] =
		{
			"float1f",
			"float2f",
			"float3f",
			"float4f",
			"vector4f",

			"float1d",
			"float2d",
			"float3d",
			"float4d",
			"vector4d",

			"quatf",
			"quatd",

			"qvvf",
		};

		static_assert(get_array_size(k_track_type_names) == (size_t)track_type8::qvvf + 1, "Unexpected array size");

		for (size_t type_index = 0; type_index < get_array_size(k_track_type_names); ++type_index)
		{
			const char* type_name = k_track_type_names[type_index];
			if (std::strncmp(type, type_name, std::strlen(type_name)) == 0)
			{
				out_type = safe_static_cast<track_type8>(type_index);
				return true;
			}
		}

		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns the track category for the provided track type.
	inline track_category8 get_track_category(track_type8 type)
	{
		// Entries in the same order as the enum integral value
		static constexpr track_category8 k_track_type_to_category[]
		{
			track_category8::scalarf,		// float1f
			track_category8::scalarf,		// float2f
			track_category8::scalarf,		// float3f
			track_category8::scalarf,		// float4f
			track_category8::scalarf,		// vector4f

			track_category8::scalard,		// float1d
			track_category8::scalard,		// float2d
			track_category8::scalard,		// float3d
			track_category8::scalard,		// float4d
			track_category8::scalard,		// vector4d

			track_category8::transformf,	// quatf
			track_category8::transformd,	// quatd

			track_category8::transformf,	// qvvf
		};

		static_assert(get_array_size(k_track_type_to_category) == (size_t)track_type8::qvvf + 1, "Unexpected array size");

		ACL_ASSERT(type <= track_type8::qvvf, "Unexpected track type");
		return type <= track_type8::qvvf ? k_track_type_to_category[static_cast<uint32_t>(type)] : track_category8::scalarf;
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns the num of elements within a sample for the provided track type.
	inline uint32_t get_track_num_sample_elements(track_type8 type)
	{
		// Entries in the same order as the enum integral value
		static constexpr uint32_t k_track_type_to_num_elements[]
		{
			1,	// float1f
			2,	// float2f
			3,	// float3f
			4,	// float4f
			4,	// vector4f

			1,	// float1d
			2,	// float2d
			3,	// float3d
			4,	// float4d
			4,	// vector4d

			4,	// quatf
			4,	// quatd

			12,	// qvvf
		};

		static_assert(get_array_size(k_track_type_to_num_elements) == (size_t)track_type8::qvvf + 1, "Unexpected array size");

		ACL_ASSERT(type <= track_type8::qvvf, "Unexpected track type");
		return type <= track_type8::qvvf ? k_track_type_to_num_elements[static_cast<uint32_t>(type)] : 0;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
