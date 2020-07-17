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
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/enum_utils.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/compression/impl/clip_context.h"

#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline TrackStreamRange calculate_track_range(const TrackStream& stream)
		{
			rtm::vector4f min = rtm::vector_set(1e10F);
			rtm::vector4f max = rtm::vector_set(-1e10F);

			const uint32_t num_samples = stream.get_num_samples();
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::vector4f sample = stream.get_raw_sample<rtm::vector4f>(sample_index);

				min = rtm::vector_min(min, sample);
				max = rtm::vector_max(max, sample);
			}

			return TrackStreamRange::from_min_max(min, max);
		}

		inline void extract_bone_ranges_impl(SegmentContext& segment, BoneRanges* bone_ranges)
		{
			const bool has_scale = segment_context_has_scale(segment);

			for (uint32_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
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

		inline void extract_clip_bone_ranges(iallocator& allocator, clip_context& context)
		{
			context.ranges = allocate_type_array<BoneRanges>(allocator, context.num_bones);

			ACL_ASSERT(context.num_segments == 1, "clip_context must contain a single segment!");
			SegmentContext& segment = context.segments[0];

			acl_impl::extract_bone_ranges_impl(segment, context.ranges);
		}

		inline void extract_segment_bone_ranges(iallocator& allocator, clip_context& context)
		{
			const rtm::vector4f one = rtm::vector_set(1.0F);
			const rtm::vector4f zero = rtm::vector_zero();
			const float max_range_value_flt = float((1 << k_segment_range_reduction_num_bits_per_component) - 1);
			const rtm::vector4f max_range_value = rtm::vector_set(max_range_value_flt);
			const rtm::vector4f inv_max_range_value = rtm::vector_set(1.0F / max_range_value_flt);

			// Segment ranges are always normalized and live between [0.0 ... 1.0]

			auto fixup_range = [&](const TrackStreamRange& range)
			{
				// In our compressed format, we store the minimum value of the track range quantized on 8 bits.
				// To get the best accuracy, we pick the value closest to the true minimum that is slightly lower.
				// This is to ensure that we encompass the lowest value even after quantization.
				const rtm::vector4f range_min = range.get_min();
				const rtm::vector4f scaled_min = rtm::vector_mul(range_min, max_range_value);
				const rtm::vector4f quantized_min0 = rtm::vector_clamp(rtm::vector_floor(scaled_min), zero, max_range_value);
				const rtm::vector4f quantized_min1 = rtm::vector_max(rtm::vector_sub(quantized_min0, one), zero);

				const rtm::vector4f padded_range_min0 = rtm::vector_mul(quantized_min0, inv_max_range_value);
				const rtm::vector4f padded_range_min1 = rtm::vector_mul(quantized_min1, inv_max_range_value);

				// Check if min0 is below or equal to our original range minimum value, if it is, it is good
				// enough to use otherwise min1 is guaranteed to be lower.
				const rtm::mask4f is_min0_lower_mask = rtm::vector_less_equal(padded_range_min0, range_min);
				const rtm::vector4f padded_range_min = rtm::vector_select(is_min0_lower_mask, padded_range_min0, padded_range_min1);

				// The story is different for the extent. We do not store the max, instead we use the extent
				// for performance reasons: a single mul/add is required to reconstruct the original value.
				// Now that our minimum value changed, our extent also changed.
				// We want to pick the extent value that brings us closest to our original max value while
				// being slightly larger to encompass it.
				const rtm::vector4f range_max = range.get_max();
				const rtm::vector4f range_extent = rtm::vector_sub(range_max, padded_range_min);
				const rtm::vector4f scaled_extent = rtm::vector_mul(range_extent, max_range_value);
				const rtm::vector4f quantized_extent0 = rtm::vector_clamp(rtm::vector_ceil(scaled_extent), zero, max_range_value);
				const rtm::vector4f quantized_extent1 = rtm::vector_min(rtm::vector_add(quantized_extent0, one), max_range_value);

				const rtm::vector4f padded_range_extent0 = rtm::vector_mul(quantized_extent0, inv_max_range_value);
				const rtm::vector4f padded_range_extent1 = rtm::vector_mul(quantized_extent1, inv_max_range_value);

				// Check if extent0 is above or equal to our original range maximum value, if it is, it is good
				// enough to use otherwise extent1 is guaranteed to be higher.
				const rtm::mask4f is_extent0_higher_mask = rtm::vector_greater_equal(padded_range_extent0, range_max);
				const rtm::vector4f padded_range_extent = rtm::vector_select(is_extent0_higher_mask, padded_range_extent0, padded_range_extent1);

				return TrackStreamRange::from_min_extent(padded_range_min, padded_range_extent);
			};

			for (SegmentContext& segment : context.segment_iterator())
			{
				segment.ranges = allocate_type_array<BoneRanges>(allocator, segment.num_bones);

				acl_impl::extract_bone_ranges_impl(segment, segment.ranges);

				for (uint32_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
				{
					const BoneStreams& bone_stream = segment.bone_streams[bone_index];
					BoneRanges& bone_range = segment.ranges[bone_index];

					if (!bone_stream.is_rotation_constant && context.are_rotations_normalized)
						bone_range.rotation = fixup_range(bone_range.rotation);

					if (!bone_stream.is_translation_constant && context.are_translations_normalized)
						bone_range.translation = fixup_range(bone_range.translation);

					if (!bone_stream.is_scale_constant && context.are_scales_normalized)
						bone_range.scale = fixup_range(bone_range.scale);
				}
			}
		}

		inline rtm::vector4f RTM_SIMD_CALL normalize_sample(rtm::vector4f_arg0 sample, const TrackStreamRange& range)
		{
			const rtm::vector4f range_min = range.get_min();
			const rtm::vector4f range_extent = range.get_extent();
			const rtm::mask4f is_range_zero_mask = rtm::vector_less_than(range_extent, rtm::vector_set(0.000000001F));

			rtm::vector4f normalized_sample = rtm::vector_div(rtm::vector_sub(sample, range_min), range_extent);
			// Clamp because the division might be imprecise
			normalized_sample = rtm::vector_min(normalized_sample, rtm::vector_set(1.0F));
			return rtm::vector_select(is_range_zero_mask, rtm::vector_zero(), normalized_sample);
		}

		inline void normalize_rotation_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint32_t num_bones)
		{
			const rtm::vector4f one = rtm::vector_set(1.0F);
			const rtm::vector4f zero = rtm::vector_zero();

			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				BoneStreams& bone_stream = bone_streams[bone_index];
				const BoneRanges& bone_range = bone_ranges[bone_index];

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", bone_stream.rotations.get_sample_size(), sizeof(rtm::vector4f));

				// Constant or default tracks are not normalized
				if (bone_stream.is_rotation_constant)
					continue;

				const uint32_t num_samples = bone_stream.rotations.get_num_samples();

				const rtm::vector4f range_min = bone_range.rotation.get_min();
				const rtm::vector4f range_extent = bone_range.rotation.get_extent();
				const rtm::mask4f is_range_zero_mask = rtm::vector_less_than(range_extent, rtm::vector_set(0.000000001F));

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					// normalized value is between [0.0 .. 1.0]
					// value = (normalized value * range extent) + range min
					// normalized value = (value - range min) / range extent
					const rtm::vector4f rotation = bone_stream.rotations.get_raw_sample<rtm::vector4f>(sample_index);
					rtm::vector4f normalized_rotation = rtm::vector_div(rtm::vector_sub(rotation, range_min), range_extent);
					// Clamp because the division might be imprecise
					normalized_rotation = rtm::vector_min(normalized_rotation, one);
					normalized_rotation = rtm::vector_select(is_range_zero_mask, zero, normalized_rotation);

#if defined(ACL_HAS_ASSERT_CHECKS)
					switch (bone_stream.rotations.get_rotation_format())
					{
					case rotation_format8::quatf_full:
						ACL_ASSERT(rtm::vector_all_greater_equal(normalized_rotation, zero) && rtm::vector_all_less_equal(normalized_rotation, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", (float)rtm::vector_get_x(normalized_rotation), (float)rtm::vector_get_y(normalized_rotation), (float)rtm::vector_get_z(normalized_rotation), (float)rtm::vector_get_w(normalized_rotation));
						break;
					case rotation_format8::quatf_drop_w_full:
					case rotation_format8::quatf_drop_w_variable:
						ACL_ASSERT(rtm::vector_all_greater_equal3(normalized_rotation, zero) && rtm::vector_all_less_equal3(normalized_rotation, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f] <= 1.0", (float)rtm::vector_get_x(normalized_rotation), (float)rtm::vector_get_y(normalized_rotation), (float)rtm::vector_get_z(normalized_rotation));
						break;
					}
#endif

					bone_stream.rotations.set_raw_sample(sample_index, normalized_rotation);
				}
			}
		}

		inline void normalize_translation_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint32_t num_bones)
		{
			const rtm::vector4f one = rtm::vector_set(1.0F);
			const rtm::vector4f zero = rtm::vector_zero();

			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				BoneStreams& bone_stream = bone_streams[bone_index];
				const BoneRanges& bone_range = bone_ranges[bone_index];

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", bone_stream.translations.get_sample_size(), sizeof(rtm::vector4f));

				// Constant or default tracks are not normalized
				if (bone_stream.is_translation_constant)
					continue;

				const uint32_t num_samples = bone_stream.translations.get_num_samples();

				const rtm::vector4f range_min = bone_range.translation.get_min();
				const rtm::vector4f range_extent = bone_range.translation.get_extent();
				const rtm::mask4f is_range_zero_mask = rtm::vector_less_than(range_extent, rtm::vector_set(0.000000001F));

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					// normalized value is between [0.0 .. 1.0]
					// value = (normalized value * range extent) + range min
					// normalized value = (value - range min) / range extent
					const rtm::vector4f translation = bone_stream.translations.get_raw_sample<rtm::vector4f>(sample_index);
					rtm::vector4f normalized_translation = rtm::vector_div(rtm::vector_sub(translation, range_min), range_extent);
					// Clamp because the division might be imprecise
					normalized_translation = rtm::vector_min(normalized_translation, one);
					normalized_translation = rtm::vector_select(is_range_zero_mask, zero, normalized_translation);

					ACL_ASSERT(rtm::vector_all_greater_equal3(normalized_translation, zero) && rtm::vector_all_less_equal3(normalized_translation, one), "Invalid normalized translation. 0.0 <= [%f, %f, %f] <= 1.0", (float)rtm::vector_get_x(normalized_translation), (float)rtm::vector_get_y(normalized_translation), (float)rtm::vector_get_z(normalized_translation));

					bone_stream.translations.set_raw_sample(sample_index, normalized_translation);
				}
			}
		}

		inline void normalize_scale_streams(BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint32_t num_bones)
		{
			const rtm::vector4f one = rtm::vector_set(1.0F);
			const rtm::vector4f zero = rtm::vector_zero();

			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				BoneStreams& bone_stream = bone_streams[bone_index];
				const BoneRanges& bone_range = bone_ranges[bone_index];

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", bone_stream.scales.get_sample_size(), sizeof(rtm::vector4f));

				// Constant or default tracks are not normalized
				if (bone_stream.is_scale_constant)
					continue;

				const uint32_t num_samples = bone_stream.scales.get_num_samples();

				const rtm::vector4f range_min = bone_range.scale.get_min();
				const rtm::vector4f range_extent = bone_range.scale.get_extent();
				const rtm::mask4f is_range_zero_mask = rtm::vector_less_than(range_extent, rtm::vector_set(0.000000001F));

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					// normalized value is between [0.0 .. 1.0]
					// value = (normalized value * range extent) + range min
					// normalized value = (value - range min) / range extent
					const rtm::vector4f scale = bone_stream.scales.get_raw_sample<rtm::vector4f>(sample_index);
					rtm::vector4f normalized_scale = rtm::vector_div(rtm::vector_sub(scale, range_min), range_extent);
					// Clamp because the division might be imprecise
					normalized_scale = rtm::vector_min(normalized_scale, one);
					normalized_scale = rtm::vector_select(is_range_zero_mask, zero, normalized_scale);

					ACL_ASSERT(rtm::vector_all_greater_equal3(normalized_scale, zero) && rtm::vector_all_less_equal3(normalized_scale, one), "Invalid normalized scale. 0.0 <= [%f, %f, %f] <= 1.0", (float)rtm::vector_get_x(normalized_scale), (float)rtm::vector_get_y(normalized_scale), (float)rtm::vector_get_z(normalized_scale));

					bone_stream.scales.set_raw_sample(sample_index, normalized_scale);
				}
			}
		}

		inline void normalize_clip_streams(clip_context& context, range_reduction_flags8 range_reduction)
		{
			ACL_ASSERT(context.num_segments == 1, "clip_context must contain a single segment!");
			SegmentContext& segment = context.segments[0];

			const bool has_scale = segment_context_has_scale(segment);

			if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::rotations))
			{
				normalize_rotation_streams(segment.bone_streams, context.ranges, segment.num_bones);
				context.are_rotations_normalized = true;
			}

			if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::translations))
			{
				normalize_translation_streams(segment.bone_streams, context.ranges, segment.num_bones);
				context.are_translations_normalized = true;
			}

			if (has_scale && are_any_enum_flags_set(range_reduction, range_reduction_flags8::scales))
			{
				normalize_scale_streams(segment.bone_streams, context.ranges, segment.num_bones);
				context.are_scales_normalized = true;
			}
		}

		inline void normalize_segment_streams(clip_context& context, range_reduction_flags8 range_reduction)
		{
			for (SegmentContext& segment : context.segment_iterator())
			{
				if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::rotations))
				{
					normalize_rotation_streams(segment.bone_streams, segment.ranges, segment.num_bones);
					segment.are_rotations_normalized = true;
				}

				if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::translations))
				{
					normalize_translation_streams(segment.bone_streams, segment.ranges, segment.num_bones);
					segment.are_translations_normalized = true;
				}

				const bool has_scale = segment_context_has_scale(segment);
				if (has_scale && are_any_enum_flags_set(range_reduction, range_reduction_flags8::scales))
				{
					normalize_scale_streams(segment.bone_streams, segment.ranges, segment.num_bones);
					segment.are_scales_normalized = true;
				}

				uint32_t range_data_size = 0;

				for (uint32_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
				{
					const BoneStreams& bone_stream = segment.bone_streams[bone_index];

					if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::rotations) && !bone_stream.is_rotation_constant)
					{
						if (bone_stream.rotations.get_rotation_format() == rotation_format8::quatf_full)
							range_data_size += k_segment_range_reduction_num_bytes_per_component * 8;
						else
							range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;
					}

					if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::translations) && !bone_stream.is_translation_constant)
						range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;

					if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::scales) && !bone_stream.is_scale_constant)
						range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;
				}

				segment.range_data_size = range_data_size;
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
