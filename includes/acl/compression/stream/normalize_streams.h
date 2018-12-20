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
#include "acl/core/enum_utils.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/compression/stream/clip_context.h"

#include <stdint.h>

namespace acl
{
	namespace impl
	{
		inline TrackStreamRange calculate_track_range(const TrackStream& stream)
		{
			Vector4_32 min = vector_set(1e10f);
			Vector4_32 max = vector_set(-1e10f);

			const uint32_t num_samples = stream.get_num_samples();
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const Vector4_32 sample = stream.get_raw_sample<Vector4_32>(sample_index);

				min = vector_min(min, sample);
				max = vector_max(max, sample);
			}

			return TrackStreamRange::from_min_max(min, max);
		}

		inline void extract_bone_ranges_impl(SegmentContext& segment, BoneRanges* bone_ranges)
		{
			const bool has_scale = segment_context_has_scale(segment);

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];
				BoneRanges& bone_range = bone_ranges[bone_index];

				bone_range.rotation = calculate_track_range(bone_stream.rotations);
				bone_range.translation = calculate_track_range(bone_stream.translations);

				if (has_scale)
					bone_range.scale = calculate_track_range(bone_stream.scales);
				else
					bone_range.scale = TrackStreamRange();
			}
		}
	}

	inline void extract_clip_bone_ranges(IAllocator& allocator, ClipContext& clip_context)
	{
		clip_context.ranges = allocate_type_array<BoneRanges>(allocator, clip_context.num_bones);

		ACL_ASSERT(clip_context.num_segments == 1, "ClipContext must contain a single segment!");
		SegmentContext& segment = clip_context.segments[0];

		impl::extract_bone_ranges_impl(segment, clip_context.ranges);
	}

	inline void extract_segment_bone_ranges(IAllocator& allocator, ClipContext& clip_context)
	{
		const Vector4_32 one = vector_set(1.0f);
		const Vector4_32 zero = vector_zero_32();
		const float max_range_value_flt = float((1 << k_segment_range_reduction_num_bits_per_component) - 1);
		const Vector4_32 max_range_value = vector_set(max_range_value_flt);
		const Vector4_32 inv_max_range_value = vector_set(1.0f / max_range_value_flt);
		const bool has_scale = clip_context.has_scale;

		// Segment ranges are always normalized and live between [0.0 ... 1.0]

		auto fixup_range = [&](const TrackStreamRange& range)
		{
			// In our compressed format, we store the minimum value of the track range quantized on 8 bits.
			// To get the best accuracy, we pick the value closest to the true minimum that is slightly lower.
			// This is to ensure that we encompass the lowest value even after quantization.
			const Vector4_32 range_min = range.get_min();
			const Vector4_32 scaled_min = vector_mul(range_min, max_range_value);
			const Vector4_32 quantized_min0 = vector_clamp(vector_floor(scaled_min), zero, max_range_value);
			const Vector4_32 quantized_min1 = vector_max(vector_sub(quantized_min0, one), zero);

			const Vector4_32 padded_range_min0 = vector_mul(quantized_min0, inv_max_range_value);
			const Vector4_32 padded_range_min1 = vector_mul(quantized_min1, inv_max_range_value);

			// Check if min0 is below or equal to our original range minimum value, if it is, it is good
			// enough to use otherwise min1 is guaranteed to be lower.
			const Vector4_32 is_min0_lower_mask = vector_less_equal(padded_range_min0, range_min);
			const Vector4_32 padded_range_min = vector_blend(is_min0_lower_mask, padded_range_min0, padded_range_min1);

			// The story is different for the extent. We do not store the max, instead we use the extent
			// for performance reasons: a single mul/add is required to reconstruct the original value.
			// Now that our minimum value changed, our extent also changed.
			// We want to pick the extent value that brings us closest to our original max value while
			// being slightly larger to encompass it.
			const Vector4_32 range_max = range.get_max();
			const Vector4_32 range_extent = vector_sub(range_max, padded_range_min);
			const Vector4_32 scaled_extent = vector_mul(range_extent, max_range_value);
			const Vector4_32 quantized_extent0 = vector_clamp(vector_ceil(scaled_extent), zero, max_range_value);
			const Vector4_32 quantized_extent1 = vector_min(vector_add(quantized_extent0, one), max_range_value);

			const Vector4_32 padded_range_extent0 = vector_mul(quantized_extent0, inv_max_range_value);
			const Vector4_32 padded_range_extent1 = vector_mul(quantized_extent1, inv_max_range_value);

			// Check if extent0 is above or equal to our original range maximum value, if it is, it is good
			// enough to use otherwise extent1 is guaranteed to be higher.
			const Vector4_32 is_extent0_higher_mask = vector_greater_equal(padded_range_extent0, range_max);
			const Vector4_32 padded_range_extent = vector_blend(is_extent0_higher_mask, padded_range_extent0, padded_range_extent1);

			return TrackStreamRange::from_min_extent(padded_range_min, padded_range_extent);
		};

		for (SegmentContext& segment : clip_context.segment_iterator())
		{
			segment.ranges = allocate_type_array<BoneRanges>(allocator, segment.num_bones);

			impl::extract_bone_ranges_impl(segment, segment.ranges);

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];
				BoneRanges& bone_range = segment.ranges[bone_index];

				if (bone_stream.is_rotation_animated() && clip_context.are_rotations_normalized)
					bone_range.rotation = fixup_range(bone_range.rotation);

				if (bone_stream.is_translation_animated() && clip_context.are_translations_normalized)
					bone_range.translation = fixup_range(bone_range.translation);

				if (has_scale && bone_stream.is_scale_animated() && clip_context.are_scales_normalized)
					bone_range.scale = fixup_range(bone_range.scale);
			}
		}
	}

	inline Vector4_32 ACL_SIMD_CALL normalize_sample(Vector4_32Arg0 sample, const TrackStreamRange& range)
	{
		const Vector4_32 range_min = range.get_min();
		const Vector4_32 range_extent = range.get_extent();
		const Vector4_32 is_range_zero_mask = vector_less_than(range_extent, vector_set(0.000000001f));

		Vector4_32 normalized_sample = vector_div(vector_sub(sample, range_min), range_extent);
		// Clamp because the division might be imprecise
		normalized_sample = vector_min(normalized_sample, vector_set(1.0f));
		return vector_blend(is_range_zero_mask, vector_zero_32(), normalized_sample);
	}

	inline void normalize_rotation_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint16_t num_bones)
	{
		const Vector4_32 one = vector_set(1.0f);
		const Vector4_32 zero = vector_zero_32();

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Vector4_32));

			// Constant or default tracks are not normalized
			if (!bone_stream.is_rotation_animated())
				continue;

			const uint32_t num_samples = bone_stream.rotations.get_num_samples();

			const Vector4_32 range_min = bone_range.rotation.get_min();
			const Vector4_32 range_extent = bone_range.rotation.get_extent();
			const Vector4_32 is_range_zero_mask = vector_less_than(range_extent, vector_set(0.000000001f));

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				const Vector4_32 rotation = bone_stream.rotations.get_raw_sample<Vector4_32>(sample_index);
				Vector4_32 normalized_rotation = vector_div(vector_sub(rotation, range_min), range_extent);
				// Clamp because the division might be imprecise
				normalized_rotation = vector_min(normalized_rotation, one);
				normalized_rotation = vector_blend(is_range_zero_mask, zero, normalized_rotation);

#if defined(ACL_HAS_ASSERT_CHECKS)
				switch (bone_stream.rotations.get_rotation_format())
				{
				case RotationFormat8::Quat_128:
					ACL_ASSERT(vector_all_greater_equal(normalized_rotation, zero) && vector_all_less_equal(normalized_rotation, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_rotation), vector_get_y(normalized_rotation), vector_get_z(normalized_rotation), vector_get_w(normalized_rotation));
					break;
				case RotationFormat8::QuatDropW_96:
				case RotationFormat8::QuatDropW_48:
				case RotationFormat8::QuatDropW_32:
				case RotationFormat8::QuatDropW_Variable:
					ACL_ASSERT(vector_all_greater_equal3(normalized_rotation, zero) && vector_all_less_equal3(normalized_rotation, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_rotation), vector_get_y(normalized_rotation), vector_get_z(normalized_rotation));
					break;
				}
#endif

				bone_stream.rotations.set_raw_sample(sample_index, normalized_rotation);
			}
		}
	}

	inline void normalize_translation_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint16_t num_bones)
	{
		const Vector4_32 one = vector_set(1.0f);
		const Vector4_32 zero = vector_zero_32();

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", bone_stream.translations.get_sample_size(), sizeof(Vector4_32));

			// Constant or default tracks are not normalized
			if (!bone_stream.is_translation_animated())
				continue;

			const uint32_t num_samples = bone_stream.translations.get_num_samples();

			const Vector4_32 range_min = bone_range.translation.get_min();
			const Vector4_32 range_extent = bone_range.translation.get_extent();
			const Vector4_32 is_range_zero_mask = vector_less_than(range_extent, vector_set(0.000000001f));

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				const Vector4_32 translation = bone_stream.translations.get_raw_sample<Vector4_32>(sample_index);
				Vector4_32 normalized_translation = vector_div(vector_sub(translation, range_min), range_extent);
				// Clamp because the division might be imprecise
				normalized_translation = vector_min(normalized_translation, one);
				normalized_translation = vector_blend(is_range_zero_mask, zero, normalized_translation);

				ACL_ASSERT(vector_all_greater_equal3(normalized_translation, zero) && vector_all_less_equal3(normalized_translation, one), "Invalid normalized translation. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_translation), vector_get_y(normalized_translation), vector_get_z(normalized_translation));

				bone_stream.translations.set_raw_sample(sample_index, normalized_translation);
			}
		}
	}

	inline void normalize_scale_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint16_t num_bones)
	{
		const Vector4_32 one = vector_set(1.0f);
		const Vector4_32 zero = vector_zero_32();

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", bone_stream.scales.get_sample_size(), sizeof(Vector4_32));

			// Constant or default tracks are not normalized
			if (!bone_stream.is_scale_animated())
				continue;

			const uint32_t num_samples = bone_stream.scales.get_num_samples();

			const Vector4_32 range_min = bone_range.scale.get_min();
			const Vector4_32 range_extent = bone_range.scale.get_extent();
			const Vector4_32 is_range_zero_mask = vector_less_than(range_extent, vector_set(0.000000001f));

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				const Vector4_32 scale = bone_stream.scales.get_raw_sample<Vector4_32>(sample_index);
				Vector4_32 normalized_scale = vector_div(vector_sub(scale, range_min), range_extent);
				// Clamp because the division might be imprecise
				normalized_scale = vector_min(normalized_scale, one);
				normalized_scale = vector_blend(is_range_zero_mask, zero, normalized_scale);

				ACL_ASSERT(vector_all_greater_equal3(normalized_scale, zero) && vector_all_less_equal3(normalized_scale, one), "Invalid normalized scale. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_scale), vector_get_y(normalized_scale), vector_get_z(normalized_scale));

				bone_stream.scales.set_raw_sample(sample_index, normalized_scale);
			}
		}
	}

	inline void normalize_clip_streams(ClipContext& clip_context, RangeReductionFlags8 range_reduction)
	{
		ACL_ASSERT(clip_context.num_segments == 1, "ClipContext must contain a single segment!");
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
						range_data_size += k_segment_range_reduction_num_bytes_per_component * 8;
					else
						range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && bone_stream.is_translation_animated())
					range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;

				if (has_scale && are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && bone_stream.is_scale_animated())
					range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;
			}

			segment.range_data_size = range_data_size;
		}
	}
}
