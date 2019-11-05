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

#include "acl/core/iallocator.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/math/transform_32.h"
#include "acl/compression/stream/track_stream.h"
#include "acl/compression/stream/normalize_streams.h"
#include "acl/compression/stream/convert_rotation_streams.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace impl
	{
		inline Vector4_32 ACL_SIMD_CALL load_rotation_sample(const uint8_t* ptr, RotationFormat8 format, uint8_t bit_rate, bool is_normalized)
		{
			switch (format)
			{
			case RotationFormat8::Quat_128:
				return unpack_vector4_128(ptr);
			case RotationFormat8::QuatDropW_96:
				return unpack_vector3_96_unsafe(ptr);
			case RotationFormat8::QuatDropW_48:
				return is_normalized ? unpack_vector3_u48_unsafe(ptr) : unpack_vector3_s48_unsafe(ptr);
			case RotationFormat8::QuatDropW_32:
				return unpack_vector3_32(11, 11, 10, is_normalized, ptr);
			case RotationFormat8::QuatDropW_Variable:
			{
				if (is_constant_bit_rate(bit_rate))
				{
					ACL_ASSERT(is_normalized, "Cannot drop a constant track if it isn't normalized");
					return unpack_vector3_u48_unsafe(ptr);
				}
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96_unsafe(ptr);
				else
				{
					const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					if (is_normalized)
						return unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, ptr, 0);
					else
						return unpack_vector3_sXX_unsafe(num_bits_at_bit_rate, ptr, 0);
				}
			}
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return vector_zero_32();
			}
		}

		inline Vector4_32 ACL_SIMD_CALL load_vector_sample(const uint8_t* ptr, VectorFormat8 format, uint8_t bit_rate)
		{
			switch (format)
			{
			case VectorFormat8::Vector3_96:
				return unpack_vector3_96_unsafe(ptr);
			case VectorFormat8::Vector3_48:
				return unpack_vector3_u48_unsafe(ptr);
			case VectorFormat8::Vector3_32:
				return unpack_vector3_32(11, 11, 10, true, ptr);
			case VectorFormat8::Vector3_Variable:
				ACL_ASSERT(bit_rate != k_invalid_bit_rate, "Invalid bit rate!");
				if (is_constant_bit_rate(bit_rate))
					return unpack_vector3_u48_unsafe(ptr);
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96_unsafe(ptr);
				else
				{
					const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					return unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, ptr, 0);
				}
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
				return vector_zero_32();
			}
		}

		inline Quat_32 ACL_SIMD_CALL rotation_to_quat_32(Vector4_32Arg0 rotation, RotationFormat8 format)
		{
			switch (format)
			{
			case RotationFormat8::Quat_128:
				return vector_to_quat(rotation);
			case RotationFormat8::QuatDropW_96:
			case RotationFormat8::QuatDropW_48:
			case RotationFormat8::QuatDropW_32:
			case RotationFormat8::QuatDropW_Variable:
				return quat_from_positive_w(rotation);
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return quat_identity_32();
			}
		}
	}

	inline Quat_32 ACL_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_rotations_normalized = clip_context->are_rotations_normalized;

		const RotationFormat8 format = bone_steams.rotations.get_rotation_format();
		const uint8_t bit_rate = bone_steams.rotations.get_bit_rate();

		if (format == RotationFormat8::QuatDropW_Variable && is_constant_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_rotation = impl::load_rotation_sample(quantized_ptr, format, bit_rate, are_rotations_normalized);

		if (are_rotations_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

				packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	inline Quat_32 ACL_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_rotations_normalized = clip_context->are_rotations_normalized;
		const RotationFormat8 format = bone_steams.rotations.get_rotation_format();

		Vector4_32 rotation;
		if (is_constant_bit_rate(bit_rate))
		{
			const uint8_t* quantized_ptr = raw_bone_steams.rotations.get_raw_sample_ptr(segment->clip_sample_offset);
			rotation = impl::load_rotation_sample(quantized_ptr, RotationFormat8::Quat_128, k_invalid_bit_rate, are_rotations_normalized);
			rotation = convert_rotation(rotation, RotationFormat8::Quat_128, format);
		}
		else if (is_raw_bit_rate(bit_rate))
		{
			const uint8_t* quantized_ptr = raw_bone_steams.rotations.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
			rotation = impl::load_rotation_sample(quantized_ptr, RotationFormat8::Quat_128, k_invalid_bit_rate, are_rotations_normalized);
			rotation = convert_rotation(rotation, RotationFormat8::Quat_128, format);
		}
		else
		{
			const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);
			rotation = impl::load_rotation_sample(quantized_ptr, format, k_invalid_bit_rate, are_rotations_normalized);
		}

		// Pack and unpack at our desired bit rate
		const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
		Vector4_32 packed_rotation;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ASSERT(are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");
			ACL_ASSERT(segment->are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_rotation = normalize_sample(rotation, clip_bone_range.rotation);

			packed_rotation = decay_vector3_u48(normalized_rotation);
		}
		else if (is_raw_bit_rate(bit_rate))
			packed_rotation = rotation;
		else if (are_rotations_normalized)
			packed_rotation = decay_vector3_uXX(rotation, num_bits_at_bit_rate);
		else
			packed_rotation = decay_vector3_sXX(rotation, num_bits_at_bit_rate);

		if (are_rotations_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

				packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	inline Quat_32 ACL_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index, RotationFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_rotations_normalized = clip_context->are_rotations_normalized && !bone_steams.is_rotation_constant;
		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);
		const RotationFormat8 format = bone_steams.rotations.get_rotation_format();

		const Vector4_32 rotation = impl::load_rotation_sample(quantized_ptr, format, k_invalid_bit_rate, are_rotations_normalized);

		// Pack and unpack in our desired format
		Vector4_32 packed_rotation;

		switch (desired_format)
		{
		case RotationFormat8::Quat_128:
		case RotationFormat8::QuatDropW_96:
			packed_rotation = rotation;
			break;
		case RotationFormat8::QuatDropW_48:
			packed_rotation = are_rotations_normalized ? decay_vector3_u48(rotation) : decay_vector3_s48(rotation);
			break;
		case RotationFormat8::QuatDropW_32:
			packed_rotation = are_rotations_normalized ? decay_vector3_u32(rotation, 11, 11, 10) : decay_vector3_s32(rotation, 11, 11, 10);
			break;
		default:
			ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(desired_format));
			packed_rotation = vector_zero_32();
			break;
		}

		if (are_rotations_normalized)
		{
			if (segment->are_rotations_normalized)
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

				packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	inline Vector4_32 ACL_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_translations_normalized = clip_context->are_translations_normalized;

		const VectorFormat8 format = bone_steams.translations.get_vector_format();
		const uint8_t bit_rate = bone_steams.translations.get_bit_rate();

		if (format == VectorFormat8::Vector3_Variable && is_constant_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_translation = impl::load_vector_sample(quantized_ptr, format, bit_rate);

		if (are_translations_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_translations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

				packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	inline Vector4_32 ACL_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const VectorFormat8 format = bone_steams.translations.get_vector_format();

		const uint8_t* quantized_ptr;
		if (is_constant_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.translations.get_raw_sample_ptr(segment->clip_sample_offset);
		else if (is_raw_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.translations.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
		else
			quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

		const Vector4_32 translation = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		ACL_ASSERT(clip_context->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

		// Pack and unpack at our desired bit rate
		Vector4_32 packed_translation;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ASSERT(segment->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_translation = normalize_sample(translation, clip_bone_range.translation);

			packed_translation = decay_vector3_u48(normalized_translation);
		}
		else if (is_raw_bit_rate(bit_rate))
			packed_translation = translation;
		else
		{
			const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			packed_translation = decay_vector3_uXX(translation, num_bits_at_bit_rate);
		}

		if (!is_raw_bit_rate(bit_rate))
		{
			if (segment->are_translations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

				packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	inline Vector4_32 ACL_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index, VectorFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_translations_normalized = clip_context->are_translations_normalized && !bone_steams.is_translation_constant;
		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);
		const VectorFormat8 format = bone_steams.translations.get_vector_format();

		const Vector4_32 translation = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		// Pack and unpack in our desired format
		Vector4_32 packed_translation;

		switch (desired_format)
		{
		case VectorFormat8::Vector3_96:
			packed_translation = translation;
			break;
		case VectorFormat8::Vector3_48:
			ACL_ASSERT(are_translations_normalized, "Translations must be normalized to support this format");
			packed_translation = decay_vector3_u48(translation);
			break;
		case VectorFormat8::Vector3_32:
			ACL_ASSERT(are_translations_normalized, "Translations must be normalized to support this format");
			packed_translation = decay_vector3_u32(translation, 11, 11, 10);
			break;
		default:
			ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
			packed_translation = vector_zero_32();
			break;
		}

		if (are_translations_normalized)
		{
			if (segment->are_translations_normalized)
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
				Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

				packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	inline Vector4_32 ACL_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_scales_normalized = clip_context->are_scales_normalized;

		const VectorFormat8 format = bone_steams.scales.get_vector_format();
		const uint8_t bit_rate = bone_steams.scales.get_bit_rate();

		if (format == VectorFormat8::Vector3_Variable && is_constant_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_scale = impl::load_vector_sample(quantized_ptr, format, bit_rate);

		if (are_scales_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_scales_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

				packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	inline Vector4_32 ACL_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const VectorFormat8 format = bone_steams.scales.get_vector_format();

		const uint8_t* quantized_ptr;
		if (is_constant_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.scales.get_raw_sample_ptr(segment->clip_sample_offset);
		else if (is_raw_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.scales.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
		else
			quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);

		const Vector4_32 scale = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		ACL_ASSERT(clip_context->are_scales_normalized, "Scales must be normalized to support variable bit rates.");

		// Pack and unpack at our desired bit rate
		Vector4_32 packed_scale;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ASSERT(segment->are_scales_normalized, "Translations must be normalized to support variable bit rates.");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_scale = normalize_sample(scale, clip_bone_range.scale);

			packed_scale = decay_vector3_u48(normalized_scale);
		}
		else if (is_raw_bit_rate(bit_rate))
			packed_scale = scale;
		else
		{
			const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			packed_scale = decay_vector3_uXX(scale, num_bits_at_bit_rate);
		}

		if (!is_raw_bit_rate(bit_rate))
		{
			if (segment->are_scales_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

				packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	inline Vector4_32 ACL_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index, VectorFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_scales_normalized = clip_context->are_scales_normalized && !bone_steams.is_scale_constant;
		const uint8_t* quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);
		const VectorFormat8 format = bone_steams.scales.get_vector_format();

		const Vector4_32 scale = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		// Pack and unpack in our desired format
		Vector4_32 packed_scale;

		switch (desired_format)
		{
		case VectorFormat8::Vector3_96:
			packed_scale = scale;
			break;
		case VectorFormat8::Vector3_48:
			ACL_ASSERT(are_scales_normalized, "Scales must be normalized to support this format");
			packed_scale = decay_vector3_u48(scale);
			break;
		case VectorFormat8::Vector3_32:
			ACL_ASSERT(are_scales_normalized, "Scales must be normalized to support this format");
			packed_scale = decay_vector3_u32(scale, 11, 11, 10);
			break;
		default:
			ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
			packed_scale = scale;
			break;
		}

		if (are_scales_normalized)
		{
			if (segment->are_scales_normalized)
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
				Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

				packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	namespace acl_impl
	{
		struct sample_context
		{
			uint32_t track_index;

			uint32_t sample_key;
			float sample_time;

			BoneBitRate bit_rates;
		};

		inline uint32_t get_uniform_sample_key(const SegmentContext& segment, float sample_time)
		{
			uint32_t key0 = 0;
			uint32_t key1 = 0;
			float interpolation_alpha = 0.0F;

			// Our samples are uniform, grab the nearest samples
			const ClipContext* clip_context = segment.clip;
			find_linear_interpolation_samples_with_sample_rate(clip_context->num_samples, clip_context->sample_rate, sample_time, SampleRoundingPolicy::Nearest, key0, key1, interpolation_alpha);

			// Offset for the current segment and clamp
			key0 = key0 - segment.clip_sample_offset;
			if (key0 >= segment.num_samples)
			{
				key0 = 0;
				interpolation_alpha = 1.0F;
			}

			key1 = key1 - segment.clip_sample_offset;
			if (key1 >= segment.num_samples)
			{
				key1 = segment.num_samples - 1;
				interpolation_alpha = 0.0F;
			}

			// When we sample uniformly, we always round to the nearest sample.
			// As such, we don't need to interpolate.
			return interpolation_alpha == 0.0F ? key0 : key1;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Quat_32 ACL_SIMD_CALL sample_rotation(const sample_context& context, const BoneStreams& bone_stream)
		{
			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = quat_identity_32();
			else if (bone_stream.is_rotation_constant)
				rotation = quat_normalize(get_rotation_sample(bone_stream, 0));
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.rotations.get_num_samples();
					const float sample_rate = bone_stream.rotations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				const Quat_32 sample0 = get_rotation_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Quat_32 sample1 = get_rotation_sample(bone_stream, key1);
					rotation = quat_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					rotation = quat_normalize(sample0);
			}

			return rotation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Quat_32 ACL_SIMD_CALL sample_rotation(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_rotation_variable, RotationFormat8 rotation_format)
		{
			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = quat_identity_32();
			else if (bone_stream.is_rotation_constant)
			{
				if (is_rotation_variable)
					rotation = get_rotation_sample(raw_bone_stream, 0);
				else
					rotation = get_rotation_sample(raw_bone_stream, 0, rotation_format);

				rotation = quat_normalize(rotation);
			}
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.rotations.get_num_samples();
					const float sample_rate = bone_stream.rotations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				Quat_32 sample0;
				Quat_32 sample1;
				if (is_rotation_variable)
				{
					sample0 = get_rotation_sample(bone_stream, raw_bone_stream, key0, context.bit_rates.rotation);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_rotation_sample(bone_stream, raw_bone_stream, key1, context.bit_rates.rotation);
				}
				else
				{
					sample0 = get_rotation_sample(bone_stream, key0, rotation_format);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_rotation_sample(bone_stream, key1, rotation_format);
				}

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
					rotation = quat_lerp(sample0, sample1, interpolation_alpha);
				else
					rotation = quat_normalize(sample0);
			}

			return rotation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_translation(const sample_context& context, const BoneStreams& bone_stream)
		{
			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = vector_zero_32();
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(bone_stream, 0);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.translations.get_num_samples();
					const float sample_rate = bone_stream.translations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				const Vector4_32 sample0 = get_translation_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Vector4_32 sample1 = get_translation_sample(bone_stream, key1);
					translation = vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					translation = sample0;
			}

			return translation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_translation(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_translation_variable, VectorFormat8 translation_format)
		{
			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = vector_zero_32();
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(raw_bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.translations.get_num_samples();
					const float sample_rate = bone_stream.translations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_translation_variable)
				{
					sample0 = get_translation_sample(bone_stream, raw_bone_stream, key0, context.bit_rates.translation);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_translation_sample(bone_stream, raw_bone_stream, key1, context.bit_rates.translation);
				}
				else
				{
					sample0 = get_translation_sample(bone_stream, key0, translation_format);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_translation_sample(bone_stream, key1, translation_format);
				}

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
					translation = vector_lerp(sample0, sample1, interpolation_alpha);
				else
					translation = sample0;
			}

			return translation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_scale(const sample_context& context, const BoneStreams& bone_stream, Vector4_32Arg0 default_scale)
		{
			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(bone_stream, 0);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.scales.get_num_samples();
					const float sample_rate = bone_stream.scales.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				const Vector4_32 sample0 = get_scale_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Vector4_32 sample1 = get_scale_sample(bone_stream, key1);
					scale = vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					scale = sample0;
			}

			return scale;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_scale(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_scale_variable, VectorFormat8 scale_format, Vector4_32Arg0 default_scale)
		{
			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(raw_bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.scales.get_num_samples();
					const float sample_rate = bone_stream.scales.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_scale_variable)
				{
					sample0 = get_scale_sample(bone_stream, raw_bone_stream, key0, context.bit_rates.scale);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_scale_sample(bone_stream, raw_bone_stream, key1, context.bit_rates.scale);
				}
				else
				{
					sample0 = get_scale_sample(bone_stream, key0, scale_format);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_scale_sample(bone_stream, key1, scale_format);
				}

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
					scale = vector_lerp(sample0, sample1, interpolation_alpha);
				else
					scale = sample0;
			}

			return scale;
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, Transform_32* out_local_pose)
	{
		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;

				const BoneStreams& bone_stream = bone_streams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
		else
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;

				const BoneStreams& bone_stream = bone_streams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
	}

	inline void sample_stream(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, uint16_t bone_index, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.track_index = bone_index;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		const BoneStreams& bone_stream = bone_streams[bone_index];

		Quat_32 rotation;
		Vector4_32 translation;
		Vector4_32 scale;
		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
			translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;
		}
		else
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
			translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;
		}

		out_local_pose[bone_index] = transform_set(rotation, translation, scale);
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, uint16_t bone_index, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;

				const BoneStreams& bone_stream = bone_streams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
		else
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;

				const BoneStreams& bone_stream = bone_streams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;
				context.bit_rates = bit_rates[bone_index];

				const BoneStreams& bone_stream = bone_streams[bone_index];
				const BoneStreams& raw_bone_steam = raw_bone_steams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
		else
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;
				context.bit_rates = bit_rates[bone_index];

				const BoneStreams& bone_stream = bone_streams[bone_index];
				const BoneStreams& raw_bone_steam = raw_bone_steams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
	}

	inline void sample_stream(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, uint16_t bone_index, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.track_index = bone_index;
		context.sample_key = sample_key;
		context.sample_time = sample_time;
		context.bit_rates = bit_rates[bone_index];

		const BoneStreams& bone_stream = bone_streams[bone_index];
		const BoneStreams& raw_bone_stream = raw_bone_steams[bone_index];

		Quat_32 rotation;
		Vector4_32 translation;
		Vector4_32 scale;
		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
			translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;
		}
		else
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
			translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;
		}

		out_local_pose[bone_index] = transform_set(rotation, translation, scale);
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, uint16_t bone_index, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;
				context.bit_rates = bit_rates[current_bone_index];

				const BoneStreams& bone_stream = bone_streams[current_bone_index];
				const BoneStreams& raw_bone_stream = raw_bone_steams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
		else
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;
				context.bit_rates = bit_rates[current_bone_index];

				const BoneStreams& bone_stream = bone_streams[current_bone_index];
				const BoneStreams& raw_bone_stream = raw_bone_steams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, uint32_t sample_index, Transform_32* out_local_pose)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			const uint32_t rotation_sample_index = !bone_stream.is_rotation_constant ? sample_index : 0;
			const Quat_32 rotation = get_rotation_sample(bone_stream, rotation_sample_index);

			const uint32_t translation_sample_index = !bone_stream.is_translation_constant ? sample_index : 0;
			const Vector4_32 translation = get_translation_sample(bone_stream, translation_sample_index);

			const uint32_t scale_sample_index = !bone_stream.is_scale_constant ? sample_index : 0;
			const Vector4_32 scale = get_scale_sample(bone_stream, scale_sample_index);

			out_local_pose[bone_index] = transform_set(rotation, translation, scale);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
