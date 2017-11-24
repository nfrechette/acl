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
#include "acl/core/enum_utils.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/research.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/compression/stream/clip_context.h"

#include <stdint.h>

namespace acl
{
	inline void extract_bone_ranges_impl(SegmentContext& segment, BoneRanges* bone_ranges)
	{
		const bool has_scale = segment_context_has_scale(segment);

		for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = segment.bone_streams[bone_index];

			Vector4 rotation_min = vector_set(Scalar(1e10));
			Vector4 rotation_max = vector_set(Scalar(-1e10));
			Vector4 translation_min = vector_set(Scalar(1e10));
			Vector4 translation_max = vector_set(Scalar(-1e10));
			Vector4 scale_min = vector_set(Scalar(1e10));
			Vector4 scale_max = vector_set(Scalar(-1e10));

			for (uint32_t sample_index = 0; sample_index < bone_stream.rotations.get_num_samples(); ++sample_index)
			{
				Quat rotation = bone_stream.rotations.get_raw_sample<Quat>(sample_index);

				rotation_min = vector_min(rotation_min, quat_to_vector(rotation));
				rotation_max = vector_max(rotation_max, quat_to_vector(rotation));
			}

			for (uint32_t sample_index = 0; sample_index < bone_stream.translations.get_num_samples(); ++sample_index)
			{
				Vector4 translation = bone_stream.translations.get_raw_sample<Vector4>(sample_index);

				translation_min = vector_min(translation_min, translation);
				translation_max = vector_max(translation_max, translation);
			}

			if (has_scale)
			{
				for (uint32_t sample_index = 0; sample_index < bone_stream.scales.get_num_samples(); ++sample_index)
				{
					Vector4 scale = bone_stream.scales.get_raw_sample<Vector4>(sample_index);

					scale_min = vector_min(scale_min, scale);
					scale_max = vector_max(scale_max, scale);
				}
			}

			BoneRanges& bone_range = bone_ranges[bone_index];
			bone_range.rotation = TrackStreamRange(rotation_min, rotation_max);
			bone_range.translation = TrackStreamRange(translation_min, translation_max);
			bone_range.scale = TrackStreamRange(scale_min, scale_max);
		}
	}

	inline void extract_clip_bone_ranges(Allocator& allocator, ClipContext& clip_context)
	{
		clip_context.ranges = allocate_type_array<BoneRanges>(allocator, clip_context.num_bones);

		ACL_ENSURE(clip_context.num_segments == 1, "ClipContext must contain a single segment!");
		SegmentContext& segment = clip_context.segments[0];

		extract_bone_ranges_impl(segment, clip_context.ranges);
	}

	inline void extract_segment_bone_ranges(Allocator& allocator, ClipContext& clip_context)
	{
		uint8_t buffer[8] = {0};
		const Vector4 padding = vector_set(ArithmeticImpl::cast(unpack_scalar_unsigned(1, ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE)));
		const Vector4 one = vector_set(Scalar(1.0));
		const Vector4 zero = ArithmeticImpl::vector_zero();
		const bool has_scale = clip_context.has_scale;

		for (SegmentContext& segment : clip_context.segment_iterator())
		{
			segment.ranges = allocate_type_array<BoneRanges>(allocator, segment.num_bones);

			extract_bone_ranges_impl(segment, segment.ranges);

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];
				BoneRanges& bone_range = segment.ranges[bone_index];

				if (bone_stream.is_rotation_animated() && clip_context.are_rotations_normalized)
				{
					Vector4 rotation_range_min = vector_max(vector_sub(bone_range.rotation.get_min(), padding), zero);
					Vector4 rotation_range_max = vector_min(vector_add(bone_range.rotation.get_max(), padding), one);

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
					pack_vector4_32(rotation_range_min, true, &buffer[0]);
					rotation_range_min = unpack_vector4_32(&buffer[0], true);
					pack_vector4_32(rotation_range_max, true, &buffer[0]);
					rotation_range_max = unpack_vector4_32(&buffer[0], true);
#else
					pack_vector4_64(rotation_range_min, true, &buffer[0]);
					rotation_range_min = unpack_vector4_64(&buffer[0], true);
					pack_vector4_64(rotation_range_max, true, &buffer[0]);
					rotation_range_max = unpack_vector4_64(&buffer[0], true);
#endif

					bone_range.rotation = TrackStreamRange(rotation_range_min, rotation_range_max);
				}

				if (bone_stream.is_translation_animated() && clip_context.are_translations_normalized)
				{
					Vector4 translation_range_min = vector_max(vector_sub(bone_range.translation.get_min(), padding), zero);
					Vector4 translation_range_max = vector_min(vector_add(bone_range.translation.get_max(), padding), one);

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
					pack_vector3_24(translation_range_min, true, &buffer[0]);
					translation_range_min = unpack_vector3_24(&buffer[0], true);
					pack_vector3_24(translation_range_max, true, &buffer[0]);
					translation_range_max = unpack_vector3_24(&buffer[0], true);
#else
					pack_vector3_48(translation_range_min, true, &buffer[0]);
					translation_range_min = unpack_vector3_48(&buffer[0], true);
					pack_vector3_48(translation_range_max, true, &buffer[0]);
					translation_range_max = unpack_vector3_48(&buffer[0], true);
#endif

					bone_range.translation = TrackStreamRange(translation_range_min, translation_range_max);
				}

				if (has_scale && bone_stream.is_scale_animated() && clip_context.are_scales_normalized)
				{
					Vector4 scale_range_min = vector_max(vector_sub(bone_range.scale.get_min(), padding), zero);
					Vector4 scale_range_max = vector_min(vector_add(bone_range.scale.get_max(), padding), one);

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
					pack_vector3_24(scale_range_min, true, &buffer[0]);
					scale_range_min = unpack_vector3_24(&buffer[0], true);
					pack_vector3_24(scale_range_max, true, &buffer[0]);
					scale_range_max = unpack_vector3_24(&buffer[0], true);
#else
					pack_vector3_48(scale_range_min, true, &buffer[0]);
					scale_range_min = unpack_vector3_48(&buffer[0], true);
					pack_vector3_48(scale_range_max, true, &buffer[0]);
					scale_range_max = unpack_vector3_48(&buffer[0], true);
#endif

					bone_range.scale = TrackStreamRange(scale_range_min, scale_range_max);
				}
			}
		}
	}

	inline void normalize_rotation_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint16_t num_bones)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			//ACL_ENSURE(bone_stream.rotations.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Vector4_32));

			// Constant or default tracks are not normalized
			if (!bone_stream.is_rotation_animated())
				continue;

			const uint32_t num_samples = bone_stream.rotations.get_num_samples();

			const Vector4 range_min = bone_range.rotation.get_min();
			const Vector4 range_extent = bone_range.rotation.get_extent();

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				const Vector4 rotation = bone_stream.rotations.get_raw_sample<Vector4>(sample_index);
				Vector4 normalized_rotation = vector_div(vector_sub(rotation, range_min), range_extent);
				const Vector4 is_range_zero_mask = vector_less_than(range_extent, vector_set(Scalar(0.000000001)));
				normalized_rotation = vector_blend(is_range_zero_mask, ArithmeticImpl::vector_zero(), normalized_rotation);

#if defined(ACL_USE_ERROR_CHECKS)
				switch (bone_stream.rotations.get_rotation_format())
				{
				case RotationFormat8::Quat_128:
					ACL_ENSURE(vector_all_greater_equal(normalized_rotation, ArithmeticImpl::vector_zero()) && vector_all_less_equal(normalized_rotation, vector_set(Scalar(1.0))), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_rotation), vector_get_y(normalized_rotation), vector_get_z(normalized_rotation), vector_get_w(normalized_rotation));
					break;
				case RotationFormat8::QuatDropW_96:
				case RotationFormat8::QuatDropW_48:
				case RotationFormat8::QuatDropW_32:
				case RotationFormat8::QuatDropW_Variable:
					ACL_ENSURE(vector_all_greater_equal3(normalized_rotation, ArithmeticImpl::vector_zero()) && vector_all_less_equal3(normalized_rotation, vector_set(Scalar(1.0))), "Invalid normalized rotation. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_rotation), vector_get_y(normalized_rotation), vector_get_z(normalized_rotation));
					break;
				}
#endif

				bone_stream.rotations.set_raw_sample(sample_index, normalized_rotation);
			}
		}
	}

	inline void normalize_translation_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint16_t num_bones)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			//ACL_ENSURE(bone_stream.translations.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", bone_stream.translations.get_sample_size(), sizeof(Vector4_32));

			// Constant or default tracks are not normalized
			if (!bone_stream.is_translation_animated())
				continue;

			uint32_t num_samples = bone_stream.translations.get_num_samples();

			Vector4 range_min = bone_range.translation.get_min();
			Vector4 range_extent = bone_range.translation.get_extent();

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				Vector4 translation = bone_stream.translations.get_raw_sample<Vector4>(sample_index);
				Vector4 normalized_translation = vector_div(vector_sub(translation, range_min), range_extent);
				Vector4 is_range_zero_mask = vector_less_than(range_extent, vector_set(Scalar(0.000000001)));
				normalized_translation = vector_blend(is_range_zero_mask, ArithmeticImpl::vector_zero(), normalized_translation);

				ACL_ENSURE(vector_all_greater_equal3(normalized_translation, ArithmeticImpl::vector_zero()) && vector_all_less_equal3(normalized_translation, vector_set(Scalar(1.0))), "Invalid normalized translation. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_translation), vector_get_y(normalized_translation), vector_get_z(normalized_translation));

				bone_stream.translations.set_raw_sample(sample_index, normalized_translation);
			}
		}
	}

	inline void normalize_scale_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint16_t num_bones)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			//ACL_ENSURE(bone_stream.scales.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", bone_stream.scales.get_sample_size(), sizeof(Vector4_32));

			// Constant or default tracks are not normalized
			if (!bone_stream.is_scale_animated())
				continue;

			uint32_t num_samples = bone_stream.scales.get_num_samples();

			Vector4 range_min = bone_range.scale.get_min();
			Vector4 range_extent = bone_range.scale.get_extent();

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				Vector4 scale = bone_stream.scales.get_raw_sample<Vector4>(sample_index);
				Vector4 normalized_scale = vector_div(vector_sub(scale, range_min), range_extent);
				Vector4 is_range_zero_mask = vector_less_than(range_extent, vector_set(Scalar(0.000000001)));
				normalized_scale = vector_blend(is_range_zero_mask, ArithmeticImpl::vector_zero(), normalized_scale);

				ACL_ENSURE(vector_all_greater_equal3(normalized_scale, ArithmeticImpl::vector_zero()) && vector_all_less_equal3(normalized_scale, vector_set(Scalar(1.0))), "Invalid normalized scale. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_scale), vector_get_y(normalized_scale), vector_get_z(normalized_scale));

				bone_stream.scales.set_raw_sample(sample_index, normalized_scale);
			}
		}
	}

	inline void normalize_clip_streams(ClipContext& clip_context, RangeReductionFlags8 range_reduction)
	{
		ACL_ENSURE(clip_context.num_segments == 1, "ClipContext must contain a single segment!");
		SegmentContext& segment = clip_context.segments[0];

		const bool has_scale = segment_context_has_scale(segment);

		if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations))
		{
			normalize_rotation_streams(segment.bone_streams, clip_context.ranges, segment.num_bones);
			clip_context.are_rotations_normalized = true;
		}

		if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations))
		{
			normalize_translation_streams(segment.bone_streams, clip_context.ranges, segment.num_bones);
			clip_context.are_translations_normalized = true;
		}

		if (has_scale && are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales))
		{
			normalize_scale_streams(segment.bone_streams, clip_context.ranges, segment.num_bones);
			clip_context.are_scales_normalized = true;
		}
	}

	inline void normalize_segment_streams(ClipContext& clip_context, RangeReductionFlags8 range_reduction)
	{
		for (SegmentContext& segment : clip_context.segment_iterator())
		{
			if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations))
			{
				normalize_rotation_streams(segment.bone_streams, segment.ranges, segment.num_bones);
				segment.are_rotations_normalized = true;
			}

			if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations))
			{
				normalize_translation_streams(segment.bone_streams, segment.ranges, segment.num_bones);
				segment.are_translations_normalized = true;
			}

			const bool has_scale = segment_context_has_scale(segment);
			if (has_scale && are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales))
			{
				normalize_scale_streams(segment.bone_streams, segment.ranges, segment.num_bones);
				segment.are_scales_normalized = true;
			}

			uint32_t range_data_size = 0;

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) && bone_stream.is_rotation_animated())
				{
					if (bone_stream.rotations.get_rotation_format() == RotationFormat8::Quat_128)
						range_data_size += ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 8;
					else
						range_data_size += ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 6;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && bone_stream.is_translation_animated())
					range_data_size += ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 6;

				if (has_scale && are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && bone_stream.is_scale_animated())
					range_data_size += ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 6;
			}

			segment.range_data_size = range_data_size;
		}
	}
}
