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

#include <stdint.h>

namespace acl
{
	namespace impl
	{
		inline Vector4_32 load_rotation_sample(const uint8_t* ptr, RotationFormat8 format, uint8_t bit_rate, bool is_normalized)
		{
			switch (format)
			{
			case RotationFormat8::Quat_128:
				return unpack_vector4_128(ptr);
			case RotationFormat8::QuatDropW_96:
				return unpack_vector3_96(ptr);
			case RotationFormat8::QuatDropW_48:
				return unpack_vector3_48(ptr, is_normalized);
			case RotationFormat8::QuatDropW_32:
				return unpack_vector3_32(11, 11, 10, is_normalized, ptr);
			case RotationFormat8::QuatDropW_Variable:
			{
				if (is_constant_bit_rate(bit_rate))
				{
					ACL_ENSURE(is_normalized, "Cannot drop a constant track if it isn't normalized");
					return unpack_vector3_48(ptr, true);
				}
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96(ptr);
				else
				{
					const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					return unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, is_normalized, ptr);
				}
			}
			default:
				ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return vector_zero_32();
			}
		}

		inline Vector4_32 load_vector_sample(const uint8_t* ptr, VectorFormat8 format, uint8_t bit_rate)
		{
			switch (format)
			{
			case VectorFormat8::Vector3_96:
				return unpack_vector3_96(ptr);
			case VectorFormat8::Vector3_48:
				return unpack_vector3_48(ptr, true);
			case VectorFormat8::Vector3_32:
				return unpack_vector3_32(11, 11, 10, true, ptr);
			case VectorFormat8::Vector3_Variable:
				ACL_ENSURE(bit_rate != k_invalid_bit_rate, "Invalid bit rate!");
				if (is_constant_bit_rate(bit_rate))
					return unpack_vector3_48(ptr, true);
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96(ptr);
				else
				{
					const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					return unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, ptr);
				}
			default:
				ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
				return vector_zero_32();
			}
		}

		inline Quat_32 rotation_to_quat_32(const Vector4_32& rotation, RotationFormat8 format)
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
				ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return quat_identity_32();
			}
		}
	}

	inline Quat_32 get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
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

		if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate) && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
			const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
		}

		if (are_rotations_normalized && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	inline Quat_32 get_rotation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
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
		uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
		alignas(8) uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_rotation;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ENSURE(are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");
			ACL_ENSURE(segment->are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_rotation = normalize_sample(rotation, clip_bone_range.rotation);

			pack_vector3_48(normalized_rotation, true, &raw_data[0]);
			packed_rotation = unpack_vector3_48(&raw_data[0], true);
		}
		else if (is_raw_bit_rate(bit_rate))
		{
			pack_vector3_96(rotation, &raw_data[0]);
			packed_rotation = unpack_vector3_96(&raw_data[0]);
		}
		else
		{
			pack_vector3_n(rotation, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, are_rotations_normalized, &raw_data[0]);
			packed_rotation = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, are_rotations_normalized, &raw_data[0]);
		}

		if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate) && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
			const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
		}

		if (are_rotations_normalized && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	inline Quat_32 get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index, RotationFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_rotations_normalized = clip_context->are_rotations_normalized && !bone_steams.is_rotation_constant;
		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);
		const RotationFormat8 format = bone_steams.rotations.get_rotation_format();

		const Vector4_32 rotation = impl::load_rotation_sample(quantized_ptr, format, k_invalid_bit_rate, are_rotations_normalized);

		// Pack and unpack in our desired format
		alignas(8) uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_rotation;

		switch (desired_format)
		{
		case RotationFormat8::Quat_128:
		case RotationFormat8::QuatDropW_96:
			packed_rotation = rotation;
			break;
		case RotationFormat8::QuatDropW_48:
			pack_vector3_48(rotation, are_rotations_normalized, &raw_data[0]);
			packed_rotation = unpack_vector3_48(&raw_data[0], are_rotations_normalized);
			break;
		case RotationFormat8::QuatDropW_32:
			pack_vector3_32(rotation, 11, 11, 10, are_rotations_normalized, &raw_data[0]);
			packed_rotation = unpack_vector3_32(11, 11, 10, are_rotations_normalized, &raw_data[0]);
			break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(desired_format));
			packed_rotation = vector_zero_32();
			break;
		}

		if (segment->are_rotations_normalized)
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
			const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
		}

		if (are_rotations_normalized)
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	inline Vector4_32 get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
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

		if (segment->are_translations_normalized && !is_constant_bit_rate(bit_rate) && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			const Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
			const Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
		}

		if (are_translations_normalized && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	inline Vector4_32 get_translation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
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

		ACL_ENSURE(clip_context->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

		// Pack and unpack at our desired bit rate
		alignas(8) uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_translation;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ENSURE(segment->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_translation = normalize_sample(translation, clip_bone_range.translation);

			pack_vector3_48(normalized_translation, true, &raw_data[0]);
			packed_translation = unpack_vector3_48(&raw_data[0], true);
		}
		else if (is_raw_bit_rate(bit_rate))
		{
			pack_vector3_96(translation, &raw_data[0]);
			packed_translation = unpack_vector3_96(&raw_data[0]);
		}
		else
		{
			const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			pack_vector3_n(translation, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, &raw_data[0]);
			packed_translation = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, &raw_data[0]);
		}

		if (segment->are_translations_normalized && !is_constant_bit_rate(bit_rate) && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			const Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
			const Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
		}

		if (!is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	inline Vector4_32 get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index, VectorFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_translations_normalized = clip_context->are_translations_normalized && !bone_steams.is_translation_constant;
		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);
		const VectorFormat8 format = bone_steams.translations.get_vector_format();

		const Vector4_32 translation = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		// Pack and unpack in our desired format
		alignas(8) uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_translation;

		switch (desired_format)
		{
		case VectorFormat8::Vector3_96:
			packed_translation = translation;
			break;
		case VectorFormat8::Vector3_48:
			pack_vector3_48(translation, are_translations_normalized, &raw_data[0]);
			packed_translation = unpack_vector3_48(&raw_data[0], are_translations_normalized);
			break;
		case VectorFormat8::Vector3_32:
			pack_vector3_32(translation, 11, 11, 10, are_translations_normalized, &raw_data[0]);
			packed_translation = unpack_vector3_32(11, 11, 10, are_translations_normalized, &raw_data[0]);
			break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
			packed_translation = vector_zero_32();
			break;
		}

		if (segment->are_translations_normalized)
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
		}

		if (are_translations_normalized)
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	inline Vector4_32 get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index)
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

		if (segment->are_scales_normalized && !is_constant_bit_rate(bit_rate) && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			const Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
			const Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
		}

		if (are_scales_normalized && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	inline Vector4_32 get_scale_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
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

		ACL_ENSURE(clip_context->are_scales_normalized, "Scales must be normalized to support variable bit rates.");

		// Pack and unpack at our desired bit rate
		alignas(8) uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_scale;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ENSURE(segment->are_scales_normalized, "Translations must be normalized to support variable bit rates.");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_scale = normalize_sample(scale, clip_bone_range.scale);

			pack_vector3_48(normalized_scale, true, &raw_data[0]);
			packed_scale = unpack_vector3_48(&raw_data[0], true);
		}
		else if (is_raw_bit_rate(bit_rate))
		{
			pack_vector3_96(scale, &raw_data[0]);
			packed_scale = unpack_vector3_96(&raw_data[0]);
		}
		else
		{
			const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			pack_vector3_n(scale, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, &raw_data[0]);
			packed_scale = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, &raw_data[0]);
		}

		if (segment->are_scales_normalized && !is_constant_bit_rate(bit_rate) && !is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			const Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
			const Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
		}

		if (!is_raw_bit_rate(bit_rate))
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	inline Vector4_32 get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index, VectorFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_scales_normalized = clip_context->are_scales_normalized && !bone_steams.is_scale_constant;
		const uint8_t* quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);
		const VectorFormat8 format = bone_steams.scales.get_vector_format();

		const Vector4_32 scale = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		// Pack and unpack in our desired format
		alignas(8) uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_scale;

		switch (desired_format)
		{
		case VectorFormat8::Vector3_96:
			packed_scale = scale;
			break;
		case VectorFormat8::Vector3_48:
			pack_vector3_48(scale, are_scales_normalized, &raw_data[0]);
			packed_scale = unpack_vector3_48(&raw_data[0], are_scales_normalized);
			break;
		case VectorFormat8::Vector3_32:
			pack_vector3_32(scale, 11, 11, 10, are_scales_normalized, &raw_data[0]);
			packed_scale = unpack_vector3_32(11, 11, 10, are_scales_normalized, &raw_data[0]);
			break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
			packed_scale = vector_set(1.0f);
			break;
		}

		if (segment->are_scales_normalized)
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
		}

		if (are_scales_normalized)
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, Transform_32* out_local_pose)
	{
		const Quat_32 default_rotation = quat_identity_32();
		const Vector4_32 default_translation = vector_zero_32();
		const Vector4_32 default_scale = vector_set(1.0f);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = default_rotation;
			else if (bone_stream.is_rotation_constant || is_constant_bit_rate(bone_stream.rotations.get_bit_rate()))
				rotation = get_rotation_sample(bone_stream, 0);
			else
			{
				uint32_t num_samples = bone_stream.rotations.get_num_samples();
				float duration = bone_stream.rotations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Quat_32 sample0 = get_rotation_sample(bone_stream, key0);
				Quat_32 sample1 = get_rotation_sample(bone_stream, key1);
				rotation = quat_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = default_translation;
			else if (bone_stream.is_translation_constant || is_constant_bit_rate(bone_stream.translations.get_bit_rate()))
				translation = get_translation_sample(bone_stream, 0);
			else
			{
				uint32_t num_samples = bone_stream.translations.get_num_samples();
				float duration = bone_stream.translations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0 = get_translation_sample(bone_stream, key0);
				Vector4_32 sample1 = get_translation_sample(bone_stream, key1);
				translation = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant || is_constant_bit_rate(bone_stream.scales.get_bit_rate()))
				scale = get_scale_sample(bone_stream, 0);
			else
			{
				uint32_t num_samples = bone_stream.scales.get_num_samples();
				float duration = bone_stream.scales.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0 = get_scale_sample(bone_stream, key0);
				Vector4_32 sample1 = get_scale_sample(bone_stream, key1);
				scale = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			out_local_pose[bone_index] = transform_set(rotation, translation, scale);
		}
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, uint16_t bone_index, Transform_32* out_local_pose)
	{
		const Quat_32 default_rotation = quat_identity_32();
		const Vector4_32 default_translation = vector_zero_32();
		const Vector4_32 default_scale = vector_set(1.0f);

		uint16_t current_bone_index = bone_index;
		while (current_bone_index != k_invalid_bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[current_bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = default_rotation;
			else if (bone_stream.is_rotation_constant)
				rotation = get_rotation_sample(bone_stream, 0);
			else
			{
				uint32_t num_samples = bone_stream.rotations.get_num_samples();
				float duration = bone_stream.rotations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Quat_32 sample0 = get_rotation_sample(bone_stream, key0);
				Quat_32 sample1 = get_rotation_sample(bone_stream, key1);
				rotation = quat_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = default_translation;
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(bone_stream, 0);
			else
			{
				uint32_t num_samples = bone_stream.translations.get_num_samples();
				float duration = bone_stream.translations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0 = get_translation_sample(bone_stream, key0);
				Vector4_32 sample1 = get_translation_sample(bone_stream, key1);
				translation = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(bone_stream, 0);
			else
			{
				uint32_t num_samples = bone_stream.scales.get_num_samples();
				float duration = bone_stream.scales.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0 = get_scale_sample(bone_stream, key0);
				Vector4_32 sample1 = get_scale_sample(bone_stream, key1);
				scale = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
			current_bone_index = bone_stream.parent_bone_index;
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);
		const Quat_32 default_rotation = quat_identity_32();
		const Vector4_32 default_translation = vector_zero_32();
		const Vector4_32 default_scale = vector_set(1.0f);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneStreams& raw_bone_stream = raw_bone_steams[bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = default_rotation;
			else if (bone_stream.is_rotation_constant)
			{
				if (is_rotation_variable)
					rotation = get_rotation_sample(bone_stream, 0);
				else
					rotation = get_rotation_sample(bone_stream, 0, rotation_format);
			}
			else
			{
				const uint32_t num_samples = bone_stream.rotations.get_num_samples();
				const float duration = bone_stream.rotations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Quat_32 sample0;
				Quat_32 sample1;
				if (is_rotation_variable)
				{
					const uint8_t bit_rate = bit_rates[bone_index].rotation;

					sample0 = get_rotation_sample(bone_stream, raw_bone_stream, key0, bit_rate);
					sample1 = get_rotation_sample(bone_stream, raw_bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_rotation_sample(bone_stream, key0, rotation_format);
					sample1 = get_rotation_sample(bone_stream, key1, rotation_format);
				}

				rotation = quat_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = default_translation;
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				const uint32_t num_samples = bone_stream.translations.get_num_samples();
				const float duration = bone_stream.translations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_translation_variable)
				{
					const uint8_t bit_rate = bit_rates[bone_index].translation;

					sample0 = get_translation_sample(bone_stream, raw_bone_stream, key0, bit_rate);
					sample1 = get_translation_sample(bone_stream, raw_bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_translation_sample(bone_stream, key0, translation_format);
					sample1 = get_translation_sample(bone_stream, key1, translation_format);
				}

				translation = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				const uint32_t num_samples = bone_stream.scales.get_num_samples();
				const float duration = bone_stream.scales.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_scale_variable)
				{
					const uint8_t bit_rate = bit_rates[bone_index].scale;

					sample0 = get_scale_sample(bone_stream, raw_bone_stream, key0, bit_rate);
					sample1 = get_scale_sample(bone_stream, raw_bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_scale_sample(bone_stream, key0, scale_format);
					sample1 = get_scale_sample(bone_stream, key1, scale_format);
				}

				scale = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			out_local_pose[bone_index] = transform_set(rotation, translation, scale);
		}
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, uint16_t bone_index, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);
		const Quat_32 default_rotation = quat_identity_32();
		const Vector4_32 default_translation = vector_zero_32();
		const Vector4_32 default_scale = vector_set(1.0f);

		uint16_t current_bone_index = bone_index;
		while (current_bone_index != k_invalid_bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[current_bone_index];
			const BoneStreams& raw_bone_stream = raw_bone_steams[current_bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = default_rotation;
			else if (bone_stream.is_rotation_constant)
			{
				if (is_rotation_variable)
					rotation = get_rotation_sample(bone_stream, 0);
				else
					rotation = get_rotation_sample(bone_stream, 0, rotation_format);
			}
			else
			{
				const uint32_t num_samples = bone_stream.rotations.get_num_samples();
				const float duration = bone_stream.rotations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Quat_32 sample0;
				Quat_32 sample1;
				if (is_rotation_variable)
				{
					const uint8_t bit_rate = bit_rates[current_bone_index].rotation;

					sample0 = get_rotation_sample(bone_stream, raw_bone_stream, key0, bit_rate);
					sample1 = get_rotation_sample(bone_stream, raw_bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_rotation_sample(bone_stream, key0, rotation_format);
					sample1 = get_rotation_sample(bone_stream, key1, rotation_format);
				}

				rotation = quat_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = default_translation;
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				const uint32_t num_samples = bone_stream.translations.get_num_samples();
				const float duration = bone_stream.translations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_translation_variable)
				{
					const uint8_t bit_rate = bit_rates[current_bone_index].translation;

					sample0 = get_translation_sample(bone_stream, raw_bone_stream, key0, bit_rate);
					sample1 = get_translation_sample(bone_stream, raw_bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_translation_sample(bone_stream, key0, translation_format);
					sample1 = get_translation_sample(bone_stream, key1, translation_format);
				}

				translation = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				const uint32_t num_samples = bone_stream.scales.get_num_samples();
				const float duration = bone_stream.scales.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_scale_variable)
				{
					const uint8_t bit_rate = bit_rates[current_bone_index].scale;

					sample0 = get_scale_sample(bone_stream, raw_bone_stream, key0, bit_rate);
					sample1 = get_scale_sample(bone_stream, raw_bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_scale_sample(bone_stream, key0, scale_format);
					sample1 = get_scale_sample(bone_stream, key1, scale_format);
				}

				scale = vector_lerp(sample0, sample1, interpolation_alpha);
			}

			out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
			current_bone_index = bone_stream.parent_bone_index;
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, uint32_t sample_index, Transform_32* out_local_pose)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			const uint32_t rotation_sample_index = bone_stream.is_rotation_animated() ? sample_index : 0;
			const Quat_32 rotation = get_rotation_sample(bone_stream, rotation_sample_index);

			const uint32_t translation_sample_index = bone_stream.is_translation_animated() ? sample_index : 0;
			const Vector4_32 translation = get_translation_sample(bone_stream, translation_sample_index);

			const uint32_t scale_sample_index = bone_stream.is_scale_animated() ? sample_index : 0;
			const Vector4_32 scale = get_scale_sample(bone_stream, scale_sample_index);

			out_local_pose[bone_index] = transform_set(rotation, translation, scale);
		}
	}
}
