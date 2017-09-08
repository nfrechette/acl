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

#include "acl/core/memory.h"
#include "acl/core/error.h"
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/math/transform_32.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline Quat_32 get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		bool are_rotations_normalized = clip_context->are_rotations_normalized;

		RotationFormat8 format = bone_steams.rotations.get_rotation_format();
		uint8_t bit_rate = bone_steams.rotations.get_bit_rate();

		if (format == RotationFormat8::QuatDropW_Variable && is_pack_0_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_rotation;

		switch (format)
		{
		case RotationFormat8::Quat_128:
			packed_rotation = unpack_vector4_128(quantized_ptr);
			break;
		case RotationFormat8::QuatDropW_96:
			packed_rotation = unpack_vector3_96(quantized_ptr);
			break;
		case RotationFormat8::QuatDropW_48:
			packed_rotation = unpack_vector3_48(quantized_ptr, are_rotations_normalized);
			break;
		case RotationFormat8::QuatDropW_32:
			packed_rotation = unpack_vector3_32(11, 11, 10, are_rotations_normalized, quantized_ptr);
			break;
		case RotationFormat8::QuatDropW_Variable:
		{
			if (is_pack_0_bit_rate(bit_rate))
			{
				ACL_ENSURE(are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
				packed_rotation = unpack_vector3_48(quantized_ptr, true);
#else
				packed_rotation = unpack_vector3_96(quantized_ptr);
#endif
			}
			else if (is_pack_72_bit_rate(bit_rate))
				packed_rotation = unpack_vector3_72(are_rotations_normalized, quantized_ptr);
			else if (is_pack_96_bit_rate(bit_rate))
				packed_rotation = unpack_vector3_96(quantized_ptr);
			else
			{
				uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
				packed_rotation = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, are_rotations_normalized, quantized_ptr);
			}
		}
		break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			packed_rotation = vector_zero_32();
			break;
		}

		if (segment->are_rotations_normalized && !is_pack_0_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
		}

		if (are_rotations_normalized)
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		switch (format)
		{
		case RotationFormat8::Quat_128:
			return vector_to_quat(packed_rotation);
		case RotationFormat8::QuatDropW_96:
		case RotationFormat8::QuatDropW_48:
		case RotationFormat8::QuatDropW_32:
		case RotationFormat8::QuatDropW_Variable:
			return quat_from_positive_w(packed_rotation);
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			return quat_identity_32();
		}
	}

	inline Quat_32 get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		bool are_rotations_normalized = clip_context->are_rotations_normalized;

		if (is_pack_0_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);

		Vector4_32 rotation;

		RotationFormat8 format = bone_steams.rotations.get_rotation_format();
		switch (format)
		{
		case RotationFormat8::Quat_128:
			rotation = unpack_vector4_128(quantized_ptr);
			break;
		case RotationFormat8::QuatDropW_96:
			rotation = unpack_vector3_96(quantized_ptr);
			break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			rotation = vector_zero_32();
			break;
		}

		// Pack and unpack at our desired bit rate
		uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
		uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_rotation;

		if (is_pack_0_bit_rate(bit_rate))
		{
			ACL_ENSURE(are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");
			ACL_ENSURE(segment->are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

			rotation = vector_mul_add(rotation, segment_range_extent, segment_range_min);

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
			pack_vector3_48(rotation, true, &raw_data[0]);
			packed_rotation = unpack_vector3_48(&raw_data[0], true);
#else
			pack_vector3_96(rotation, &raw_data[0]);
			packed_rotation = unpack_vector3_96(&raw_data[0]);
#endif
		}
		else if (is_pack_72_bit_rate(bit_rate))
		{
			pack_vector3_72(rotation, are_rotations_normalized, &raw_data[0]);
			packed_rotation = unpack_vector3_72(are_rotations_normalized, &raw_data[0]);
		}
		else if (is_pack_96_bit_rate(bit_rate))
		{
			pack_vector3_96(rotation, &raw_data[0]);
			packed_rotation = unpack_vector3_96(&raw_data[0]);
		}
		else
		{
			pack_vector3_n(rotation, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, are_rotations_normalized, &raw_data[0]);
			packed_rotation = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, are_rotations_normalized, &raw_data[0]);
		}

		if (segment->are_rotations_normalized && !is_pack_0_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
		}

		if (are_rotations_normalized)
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		switch (format)
		{
		case RotationFormat8::Quat_128:
			return vector_to_quat(packed_rotation);
		case RotationFormat8::QuatDropW_96:
		case RotationFormat8::QuatDropW_48:
		case RotationFormat8::QuatDropW_32:
		case RotationFormat8::QuatDropW_Variable:
			return quat_from_positive_w(packed_rotation);
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			return quat_identity_32();
		}
	}

	inline Quat_32 get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index, RotationFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		bool are_rotations_normalized = clip_context->are_rotations_normalized;
		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);

		Vector4_32 rotation;

		RotationFormat8 format = bone_steams.rotations.get_rotation_format();
		switch (format)
		{
		case RotationFormat8::Quat_128:
			rotation = unpack_vector4_128(quantized_ptr);
			break;
		case RotationFormat8::QuatDropW_96:
			rotation = unpack_vector3_96(quantized_ptr);
			break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			rotation = vector_zero_32();
			break;
		}

		// Pack and unpack in our desired format
		uint8_t raw_data[16] = { 0 };
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

			Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
		}

		if (are_rotations_normalized)
		{
			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		switch (format)
		{
		case RotationFormat8::Quat_128:
			return vector_to_quat(packed_rotation);
		case RotationFormat8::QuatDropW_96:
		case RotationFormat8::QuatDropW_48:
		case RotationFormat8::QuatDropW_32:
		case RotationFormat8::QuatDropW_Variable:
			return quat_from_positive_w(packed_rotation);
		default:
			ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
			return quat_identity_32();
		}
	}

	inline Vector4_32 get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		bool are_translations_normalized = clip_context->are_translations_normalized;

		VectorFormat8 format = bone_steams.translations.get_vector_format();
		uint8_t bit_rate = bone_steams.translations.get_bit_rate();

		if (format == VectorFormat8::Vector3_Variable && is_pack_0_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_translation;

		switch (format)
		{
		case VectorFormat8::Vector3_96:
			packed_translation = unpack_vector3_96(quantized_ptr);
			break;
		case VectorFormat8::Vector3_48:
			packed_translation = unpack_vector3_48(quantized_ptr, are_translations_normalized);
			break;
		case VectorFormat8::Vector3_32:
			packed_translation = unpack_vector3_32(11, 11, 10, are_translations_normalized, quantized_ptr);
			break;
		case VectorFormat8::Vector3_Variable:
		{
			ACL_ENSURE(are_translations_normalized, "Translations must be normalized to support variable bit rates.");

			if (is_pack_0_bit_rate(bit_rate))
			{
#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
				packed_translation = unpack_vector3_48(quantized_ptr, true);
#else
				packed_translation = unpack_vector3_96(quantized_ptr);
#endif
			}
			else if (is_pack_72_bit_rate(bit_rate))
				packed_translation = unpack_vector3_72(true, quantized_ptr);
			else if (is_pack_96_bit_rate(bit_rate))
				packed_translation = unpack_vector3_96(quantized_ptr);
			else
			{
				uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
				packed_translation = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, quantized_ptr);
			}
		}
		break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			packed_translation = vector_zero_32();
			break;
		}

		if (segment->are_translations_normalized && !is_pack_0_bit_rate(bit_rate))
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

	inline Vector4_32 get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		bool are_translations_normalized = clip_context->are_translations_normalized;

		if (is_pack_0_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

		Vector4_32 translation;

		VectorFormat8 format = bone_steams.translations.get_vector_format();
		switch (format)
		{
		case VectorFormat8::Vector3_96:
			translation = unpack_vector3_96(quantized_ptr);
			break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			translation = vector_zero_32();
			break;
		}

		ACL_ENSURE(are_translations_normalized, "Translations must be normalized to support variable bit rates.");

		// Pack and unpack at our desired bit rate
		uint8_t raw_data[16] = { 0 };
		Vector4_32 packed_translation;

		if (is_pack_0_bit_rate(bit_rate))
		{
			ACL_ENSURE(segment->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

			translation = vector_mul_add(translation, segment_range_extent, segment_range_min);

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
			pack_vector3_48(translation, true, &raw_data[0]);
			packed_translation = unpack_vector3_48(&raw_data[0], true);
#else
			pack_vector3_96(translation, &raw_data[0]);
			packed_translation = unpack_vector3_96(&raw_data[0]);
#endif
		}
		else if (is_pack_72_bit_rate(bit_rate))
		{
			pack_vector3_72(translation, true, &raw_data[0]);
			packed_translation = unpack_vector3_72(true, &raw_data[0]);
		}
		else if (is_pack_96_bit_rate(bit_rate))
		{
			pack_vector3_96(translation, &raw_data[0]);
			packed_translation = unpack_vector3_96(&raw_data[0]);
		}
		else
		{
			uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			pack_vector3_n(translation, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, &raw_data[0]);
			packed_translation = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, &raw_data[0]);
		}

		if (segment->are_translations_normalized && !is_pack_0_bit_rate(bit_rate))
		{
			const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

			Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
			Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
		}

		const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

		Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
		Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

		return vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
	}

	inline Vector4_32 get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index, VectorFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		bool are_translations_normalized = clip_context->are_translations_normalized;
		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

		Vector4_32 translation;

		VectorFormat8 format = bone_steams.translations.get_vector_format();
		switch (format)
		{
		case VectorFormat8::Vector3_96:
			translation = unpack_vector3_96(quantized_ptr);
			break;
		default:
			ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
			translation = vector_zero_32();
			break;
		}

		// Pack and unpack in our desired format
		uint8_t raw_data[16] = { 0 };
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

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, Transform_32* out_local_pose)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_animated() && !is_pack_0_bit_rate(bone_stream.rotations.get_bit_rate()))
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
			else
			{
				rotation = get_rotation_sample(bone_stream, 0);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_animated() && !is_pack_0_bit_rate(bone_stream.translations.get_bit_rate()))
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
			else
			{
				translation = get_translation_sample(bone_stream, 0);
			}

			out_local_pose[bone_index] = transform_set(rotation, translation);
		}
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, uint16_t bone_index, Transform_32* out_local_pose)
	{
		uint16_t current_bone_index = bone_index;
		while (current_bone_index != INVALID_BONE_INDEX)
		{
			const BoneStreams& bone_stream = bone_streams[current_bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_animated())
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
			else
			{
				rotation = get_rotation_sample(bone_stream, 0);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_animated())
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
			else
			{
				translation = get_translation_sample(bone_stream, 0);
			}

			out_local_pose[current_bone_index] = transform_set(rotation, translation);
			current_bone_index = bone_stream.parent_bone_index;
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, Transform_32* out_local_pose)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_animated())
			{
				uint32_t num_samples = bone_stream.rotations.get_num_samples();
				float duration = bone_stream.rotations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Quat_32 sample0;
				Quat_32 sample1;
				if (is_rotation_variable)
				{
					uint8_t bit_rate = bit_rates[bone_index].rotation;

					sample0 = get_rotation_sample(bone_stream, key0, bit_rate);
					sample1 = get_rotation_sample(bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_rotation_sample(bone_stream, key0, rotation_format);
					sample1 = get_rotation_sample(bone_stream, key1, rotation_format);
				}

				rotation = quat_lerp(sample0, sample1, interpolation_alpha);
			}
			else
			{
				if (is_rotation_variable)
					rotation = get_rotation_sample(bone_stream, 0);
				else
					rotation = get_rotation_sample(bone_stream, 0, rotation_format);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_animated())
			{
				uint32_t num_samples = bone_stream.translations.get_num_samples();
				float duration = bone_stream.translations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_translation_variable)
				{
					uint8_t bit_rate = bit_rates[bone_index].translation;

					sample0 = get_translation_sample(bone_stream, key0, bit_rate);
					sample1 = get_translation_sample(bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_translation_sample(bone_stream, key0, translation_format);
					sample1 = get_translation_sample(bone_stream, key1, translation_format);
				}

				translation = vector_lerp(sample0, sample1, interpolation_alpha);
			}
			else
			{
				translation = get_translation_sample(bone_stream, 0, VectorFormat8::Vector3_96);
			}

			out_local_pose[bone_index] = transform_set(rotation, translation);
		}
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, uint16_t bone_index, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, Transform_32* out_local_pose)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);

		uint16_t current_bone_index = bone_index;
		while (current_bone_index != INVALID_BONE_INDEX)
		{
			const BoneStreams& bone_stream = bone_streams[current_bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_animated())
			{
				uint32_t num_samples = bone_stream.rotations.get_num_samples();
				float duration = bone_stream.rotations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Quat_32 sample0;
				Quat_32 sample1;
				if (is_rotation_variable)
				{
					uint8_t bit_rate = bit_rates[current_bone_index].rotation;

					sample0 = get_rotation_sample(bone_stream, key0, bit_rate);
					sample1 = get_rotation_sample(bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_rotation_sample(bone_stream, key0, rotation_format);
					sample1 = get_rotation_sample(bone_stream, key1, rotation_format);
				}

				rotation = quat_lerp(sample0, sample1, interpolation_alpha);
			}
			else
			{
				if (is_rotation_variable)
					rotation = get_rotation_sample(bone_stream, 0);
				else
					rotation = get_rotation_sample(bone_stream, 0, rotation_format);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_animated())
			{
				uint32_t num_samples = bone_stream.translations.get_num_samples();
				float duration = bone_stream.translations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_translation_variable)
				{
					uint8_t bit_rate = bit_rates[current_bone_index].translation;

					sample0 = get_translation_sample(bone_stream, key0, bit_rate);
					sample1 = get_translation_sample(bone_stream, key1, bit_rate);
				}
				else
				{
					sample0 = get_translation_sample(bone_stream, key0, translation_format);
					sample1 = get_translation_sample(bone_stream, key1, translation_format);
				}

				translation = vector_lerp(sample0, sample1, interpolation_alpha);
			}
			else
			{
				translation = get_translation_sample(bone_stream, 0, VectorFormat8::Vector3_96);
			}

			out_local_pose[current_bone_index] = transform_set(rotation, translation);
			current_bone_index = bone_stream.parent_bone_index;
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, uint32_t sample_index, Transform_32* out_local_pose)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			uint32_t rotation_sample_index = bone_stream.is_rotation_animated() ? sample_index : 0;
			Quat_32 rotation = get_rotation_sample(bone_stream, rotation_sample_index);

			uint32_t translation_sample_index = bone_stream.is_translation_animated() ? sample_index : 0;
			Vector4_32 translation = get_translation_sample(bone_stream, translation_sample_index);

			out_local_pose[bone_index] = transform_set(rotation, translation);
		}
	}
}
