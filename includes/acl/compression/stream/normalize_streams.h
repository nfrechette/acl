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
#include "acl/core/enum_utils.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/compression/stream/clip_context.h"
#include "acl/compression/impl/track_database.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

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

				if (!bone_stream.is_rotation_constant && clip_context.are_rotations_normalized)
					bone_range.rotation = fixup_range(bone_range.rotation);

				if (!bone_stream.is_translation_constant && clip_context.are_translations_normalized)
					bone_range.translation = fixup_range(bone_range.translation);

				if (!bone_stream.is_scale_constant && clip_context.are_scales_normalized)
					bone_range.scale = fixup_range(bone_range.scale);
			}
		}
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL normalize_sample(Vector4_32Arg0 sample, Vector4_32Arg1 range_min, Vector4_32Arg2 range_extent)
		{
			const Vector4_32 is_range_zero_mask = vector_less_than(range_extent, vector_set(0.000000001f));

			Vector4_32 normalized_sample = vector_div(vector_sub(sample, range_min), range_extent);
			// Clamp because the division might be imprecise
			normalized_sample = vector_min(normalized_sample, vector_set(1.0f));
			return vector_blend(is_range_zero_mask, vector_zero_32(), normalized_sample);
		}
	}

	inline Vector4_32 ACL_SIMD_CALL normalize_sample(Vector4_32Arg0 sample, const TrackStreamRange& range)
	{
		return acl_impl::normalize_sample(sample, range.get_min(), range.get_extent());
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
			if (bone_stream.is_rotation_constant)
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
			if (bone_stream.is_translation_constant)
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
			if (bone_stream.is_scale_constant)
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

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) && !bone_stream.is_rotation_constant)
				{
					if (bone_stream.rotations.get_rotation_format() == RotationFormat8::Quat_128)
						range_data_size += k_segment_range_reduction_num_bytes_per_component * 8;
					else
						range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && !bone_stream.is_translation_constant)
					range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && !bone_stream.is_scale_constant)
					range_data_size += k_segment_range_reduction_num_bytes_per_component * 6;
			}

			segment.range_data_size = range_data_size;
		}
	}

	namespace acl_impl
	{
		inline float ACL_SIMD_CALL get_min_component(Vector4_32Arg0 input)
		{
#if defined(ACL_SSE2_INTRINSICS)
			__m128 zwzw = _mm_movehl_ps(input, input);
			__m128 xz_yw_zz_ww = _mm_min_ps(input, zwzw);
			__m128 yw_yw_yw_yw = _mm_shuffle_ps(xz_yw_zz_ww, xz_yw_zz_ww, _MM_SHUFFLE(1, 1, 1, 1));
			return _mm_cvtss_f32(_mm_min_ps(xz_yw_zz_ww, yw_yw_yw_yw));
#elif defined(ACL_NEON_INTRINSICS)
			float32x2_t xy_zw = vpmin_f32(vget_low_f32(input), vget_high_f32(input));
			return vget_lane_f32(vpmin_f32(xy_zw, xy_zw), 0);
#else
			return scalar_min(scalar_min(input.x, input.y), scalar_min(input.z, input.w));
#endif
		}

		inline float ACL_SIMD_CALL get_max_component(Vector4_32Arg0 input)
		{
#if defined(ACL_SSE2_INTRINSICS)
			__m128 zwzw = _mm_movehl_ps(input, input);
			__m128 xz_yw_zz_ww = _mm_max_ps(input, zwzw);
			__m128 yw_yw_yw_yw = _mm_shuffle_ps(xz_yw_zz_ww, xz_yw_zz_ww, _MM_SHUFFLE(1, 1, 1, 1));
			return _mm_cvtss_f32(_mm_max_ps(xz_yw_zz_ww, yw_yw_yw_yw));
#elif defined(ACL_NEON_INTRINSICS)
			float32x2_t xy_zw = vpmax_f32(vget_low_f32(input), vget_high_f32(input));
			return vget_lane_f32(vpmax_f32(xy_zw, xy_zw), 0);
#else
			return scalar_max(scalar_max(input.x, input.y), scalar_max(input.z, input.w));
#endif
		}

		inline Vector4_32 ACL_SIMD_CALL vector_broadcast(const float* input)
		{
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_load_ps1(input);
#elif defined(ACL_NEON_INTRINSICS)
			return vld1q_dup_f32(input);
#else
			return vector_set(*input);
#endif
		}

		inline void extract_vector4f_range(Vector4_32* inputs_x, Vector4_32* inputs_y, Vector4_32* inputs_z, Vector4_32* inputs_w, uint32_t num_soa_entries, Vector4_32& out_range_min, Vector4_32& out_range_max)
		{
			const Vector4_32 range_min_value = vector_set(1e10f);
			const Vector4_32 range_max_value = vector_set(-1e10f);

			Vector4_32 input_min_x = range_min_value;
			Vector4_32 input_min_y = range_min_value;
			Vector4_32 input_min_z = range_min_value;
			Vector4_32 input_min_w = range_min_value;

			Vector4_32 input_max_x = range_max_value;
			Vector4_32 input_max_y = range_max_value;
			Vector4_32 input_max_z = range_max_value;
			Vector4_32 input_max_w = range_max_value;

			// TODO: Trivial AVX or ISPC conversion
			for (uint32_t entry_index = 0; entry_index < num_soa_entries; ++entry_index)
			{
				input_min_x = vector_min(input_min_x, inputs_x[entry_index]);
				input_max_x = vector_max(input_max_x, inputs_x[entry_index]);

				input_min_y = vector_min(input_min_y, inputs_y[entry_index]);
				input_max_y = vector_max(input_max_y, inputs_y[entry_index]);

				input_min_z = vector_min(input_min_z, inputs_z[entry_index]);
				input_max_z = vector_max(input_max_z, inputs_z[entry_index]);

				input_min_w = vector_min(input_min_w, inputs_w[entry_index]);
				input_max_w = vector_max(input_max_w, inputs_w[entry_index]);
			}

			// TODO: Transposing the input and performing an single write yields fewer instructions
			// but it seems to mess up the inlining with VS2017 and the caller inlines a lot less
			// which yields a massive net slowdown
			out_range_min = vector_set(get_min_component(input_min_x), get_min_component(input_min_y), get_min_component(input_min_z), get_min_component(input_min_w));
			out_range_max = vector_set(get_max_component(input_max_x), get_max_component(input_max_y), get_max_component(input_max_z), get_max_component(input_max_w));
		}

		inline void extract_vector3f_range(Vector4_32* inputs_x, Vector4_32* inputs_y, Vector4_32* inputs_z, uint32_t num_soa_entries, Vector4_32& out_range_min, Vector4_32& out_range_max)
		{
			const Vector4_32 range_min_value = vector_set(1e10f);
			const Vector4_32 range_max_value = vector_set(-1e10f);

			Vector4_32 input_min_x = range_min_value;
			Vector4_32 input_min_y = range_min_value;
			Vector4_32 input_min_z = range_min_value;

			Vector4_32 input_max_x = range_max_value;
			Vector4_32 input_max_y = range_max_value;
			Vector4_32 input_max_z = range_max_value;

			// TODO: Trivial AVX or ISPC conversion
			for (uint32_t entry_index = 0; entry_index < num_soa_entries; ++entry_index)
			{
				input_min_x = vector_min(input_min_x, inputs_x[entry_index]);
				input_max_x = vector_max(input_max_x, inputs_x[entry_index]);

				input_min_y = vector_min(input_min_y, inputs_y[entry_index]);
				input_max_y = vector_max(input_max_y, inputs_y[entry_index]);

				input_min_z = vector_min(input_min_z, inputs_z[entry_index]);
				input_max_z = vector_max(input_max_z, inputs_z[entry_index]);
			}

			out_range_min = vector_set(get_min_component(input_min_x), get_min_component(input_min_y), get_min_component(input_min_z), 0.0f);
			out_range_max = vector_set(get_max_component(input_max_x), get_max_component(input_max_y), get_max_component(input_max_z), 0.0f);
		}

		inline void extract_database_transform_ranges_per_segment(track_database& database, segment_context& segment)
		{
			const Vector4_32 zero = vector_zero_32();

			const bool has_scale = database.has_scale();
			const RotationFormat8 rotation_format = database.get_rotation_format();
			const uint32_t num_transforms = database.get_num_transforms();
			const uint32_t num_soa_entries = segment.num_soa_entries;
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				qvvf_ranges& range = segment.ranges[transform_index];

				if (rotation_format == RotationFormat8::Quat_128)
				{
					Vector4_32* rotations_x;
					Vector4_32* rotations_y;
					Vector4_32* rotations_z;
					Vector4_32* rotations_w;
					database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

					extract_vector4f_range(rotations_x, rotations_y, rotations_z, rotations_w, num_soa_entries, range.rotation_min, range.rotation_max);
				}
				else
				{
					Vector4_32* rotations_x;
					Vector4_32* rotations_y;
					Vector4_32* rotations_z;
					database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z);

					extract_vector3f_range(rotations_x, rotations_y, rotations_z, num_soa_entries, range.rotation_min, range.rotation_max);
				}

				range.rotation_extent = vector_sub(range.rotation_min, range.rotation_max);

				Vector4_32* translations_x;
				Vector4_32* translations_y;
				Vector4_32* translations_z;
				database.get_translations(segment, transform_index, translations_x, translations_y, translations_z);

				extract_vector3f_range(translations_x, translations_y, translations_z, num_soa_entries, range.translation_min, range.translation_max);
				range.translation_extent = vector_sub(range.translation_min, range.translation_max);

				if (has_scale)
				{
					Vector4_32* scales_x;
					Vector4_32* scales_y;
					Vector4_32* scales_z;
					database.get_scales(segment, transform_index, scales_x, scales_y, scales_z);

					extract_vector3f_range(scales_x, scales_y, scales_z, num_soa_entries, range.scale_min, range.scale_max);
					range.scale_extent = vector_sub(range.scale_min, range.scale_max);
				}
				else
				{
					range.scale_min = zero;
					range.scale_max = zero;
					range.scale_extent = zero;
				}
			}
		}

		inline void extract_segment_ranges(track_database& database, segment_context& segment)
		{
			const Vector4_32 one = vector_set(1.0f);
			const Vector4_32 zero = vector_zero_32();
			const float max_range_value_flt = float((1 << k_segment_range_reduction_num_bits_per_component) - 1);
			const Vector4_32 max_range_value = vector_set(max_range_value_flt);
			const Vector4_32 inv_max_range_value = vector_set(1.0f / max_range_value_flt);

			// Segment ranges are always normalized and live between [0.0 ... 1.0]

			auto fixup_range = [&](Vector4_32& range_min, Vector4_32& range_max, Vector4_32& out_range_extent)
			{
				// In our compressed format, we store the minimum value of the track range quantized on 8 bits.
				// To get the best accuracy, we pick the value closest to the true minimum that is slightly lower.
				// This is to ensure that we encompass the lowest value even after quantization.
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
				const Vector4_32 range_extent_with_new_min = vector_sub(range_max, padded_range_min);
				const Vector4_32 scaled_extent = vector_mul(range_extent_with_new_min, max_range_value);
				const Vector4_32 quantized_extent0 = vector_clamp(vector_ceil(scaled_extent), zero, max_range_value);
				const Vector4_32 quantized_extent1 = vector_min(vector_add(quantized_extent0, one), max_range_value);

				const Vector4_32 padded_range_extent0 = vector_mul(quantized_extent0, inv_max_range_value);
				const Vector4_32 padded_range_extent1 = vector_mul(quantized_extent1, inv_max_range_value);

				// Check if extent0 is above or equal to our original range maximum value, if it is, it is good
				// enough to use otherwise extent1 is guaranteed to be higher.
				const Vector4_32 is_extent0_higher_mask = vector_greater_equal(padded_range_extent0, range_max);
				const Vector4_32 padded_range_extent = vector_blend(is_extent0_higher_mask, padded_range_extent0, padded_range_extent1);

				range_min = padded_range_min;
				range_max = vector_add(padded_range_min, padded_range_extent);
				out_range_extent = padded_range_extent;
			};

			const bool has_scale = database.has_scale();
			const RotationFormat8 rotation_format = database.get_rotation_format();
			const uint32_t num_transforms = database.get_num_transforms();
			const uint32_t num_soa_entries = segment.num_soa_entries;
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				qvvf_ranges& segment_range = segment.ranges[transform_index];

				{
					if (rotation_format == RotationFormat8::Quat_128)
					{
						Vector4_32* rotations_x;
						Vector4_32* rotations_y;
						Vector4_32* rotations_z;
						Vector4_32* rotations_w;
						database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

						extract_vector4f_range(rotations_x, rotations_y, rotations_z, rotations_w, num_soa_entries, segment_range.rotation_min, segment_range.rotation_max);
					}
					else
					{
						Vector4_32* rotations_x;
						Vector4_32* rotations_y;
						Vector4_32* rotations_z;
						database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z);

						extract_vector3f_range(rotations_x, rotations_y, rotations_z, num_soa_entries, segment_range.rotation_min, segment_range.rotation_max);
					}

					Vector4_32 range_min = segment_range.rotation_min;
					Vector4_32 range_max = segment_range.rotation_max;
					Vector4_32 range_extent;

					fixup_range(range_min, range_max, range_extent);

					segment_range.rotation_min = range_min;
					segment_range.rotation_max = range_max;
					segment_range.rotation_extent = range_extent;
				}

				{
					Vector4_32* translations_x;
					Vector4_32* translations_y;
					Vector4_32* translations_z;
					database.get_translations(segment, transform_index, translations_x, translations_y, translations_z);

					extract_vector3f_range(translations_x, translations_y, translations_z, num_soa_entries, segment_range.translation_min, segment_range.translation_max);

					Vector4_32 range_min = segment_range.translation_min;
					Vector4_32 range_max = segment_range.translation_max;
					Vector4_32 range_extent;

					fixup_range(range_min, range_max, range_extent);

					segment_range.translation_min = range_min;
					segment_range.translation_max = range_max;
					segment_range.translation_extent = range_extent;
				}

				if (has_scale)
				{
					Vector4_32* scales_x;
					Vector4_32* scales_y;
					Vector4_32* scales_z;
					database.get_scales(segment, transform_index, scales_x, scales_y, scales_z);

					extract_vector3f_range(scales_x, scales_y, scales_z, num_soa_entries, segment_range.scale_min, segment_range.scale_max);

					Vector4_32 range_min = segment_range.scale_min;
					Vector4_32 range_max = segment_range.scale_max;
					Vector4_32 range_extent;

					fixup_range(range_min, range_max, range_extent);

					segment_range.scale_min = range_min;
					segment_range.scale_max = range_max;
					segment_range.scale_extent = range_extent;
				}
				else
				{
					segment_range.scale_min = zero;
					segment_range.scale_max = zero;
					segment_range.scale_extent = zero;
				}
			}
		}

		inline void merge_database_transform_ranges_from_segments(track_database& database, segment_context* segments, uint32_t num_segments)
		{
			const Vector4_32 range_min_value = vector_set(1e10f);
			const Vector4_32 range_max_value = vector_set(-1e10f);

			const uint32_t num_transforms = database.get_num_transforms();
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				Vector4_32 rotation_range_min = range_min_value;
				Vector4_32 rotation_range_max = range_max_value;
				Vector4_32 translation_range_min = range_min_value;
				Vector4_32 translation_range_max = range_max_value;
				Vector4_32 scale_range_min = range_min_value;
				Vector4_32 scale_range_max = range_max_value;

				for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
				{
					const segment_context& segment = segments[segment_index];
					const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

					rotation_range_min = vector_min(rotation_range_min, segment_transform_range.rotation_min);
					rotation_range_max = vector_max(rotation_range_max, segment_transform_range.rotation_max);
					translation_range_min = vector_min(translation_range_min, segment_transform_range.translation_min);
					translation_range_max = vector_max(translation_range_max, segment_transform_range.translation_max);
					scale_range_min = vector_min(scale_range_min, segment_transform_range.scale_min);
					scale_range_max = vector_max(scale_range_max, segment_transform_range.scale_max);
				}

				qvvf_ranges& clip_transform_range = database.get_range(transform_index);

				const Vector4_32 rotation_range_extent = vector_sub(rotation_range_max, rotation_range_min);
				clip_transform_range.rotation_min = rotation_range_min;
				clip_transform_range.rotation_max = rotation_range_max;
				clip_transform_range.rotation_extent = rotation_range_extent;

				const Vector4_32 translation_range_extent = vector_sub(translation_range_max, translation_range_min);
				clip_transform_range.translation_min = translation_range_min;
				clip_transform_range.translation_max = translation_range_max;
				clip_transform_range.translation_extent = translation_range_extent;

				const Vector4_32 scale_range_extent = vector_sub(scale_range_max, scale_range_min);
				clip_transform_range.scale_min = scale_range_min;
				clip_transform_range.scale_max = scale_range_max;
				clip_transform_range.scale_extent = scale_range_extent;
			}
		}

		inline void normalize_vector4f_track(Vector4_32* inputs_x, Vector4_32* inputs_y, Vector4_32* inputs_z, Vector4_32* inputs_w, uint32_t num_soa_entries, const Vector4_32& range_min, const Vector4_32& range_extent)
		{
			const Vector4_32 one = vector_set(1.0f);
			const Vector4_32 zero = vector_zero_32();
			const Vector4_32 range_extent_epsilon = vector_set(0.000000001f);

			const float* range_min_ptr = vector_as_float_ptr(range_min);
			const Vector4_32 range_min_x = vector_broadcast(range_min_ptr + 0);
			const Vector4_32 range_min_y = vector_broadcast(range_min_ptr + 1);
			const Vector4_32 range_min_z = vector_broadcast(range_min_ptr + 2);
			const Vector4_32 range_min_w = vector_broadcast(range_min_ptr + 3);

			const float* range_extent_ptr = vector_as_float_ptr(range_extent);
			const Vector4_32 range_extent_x = vector_broadcast(range_extent_ptr + 0);
			const Vector4_32 range_extent_y = vector_broadcast(range_extent_ptr + 1);
			const Vector4_32 range_extent_z = vector_broadcast(range_extent_ptr + 2);
			const Vector4_32 range_extent_w = vector_broadcast(range_extent_ptr + 3);

			const Vector4_32 is_range_zero_mask_x = vector_less_than(range_extent_x, range_extent_epsilon);
			const Vector4_32 is_range_zero_mask_y = vector_less_than(range_extent_y, range_extent_epsilon);
			const Vector4_32 is_range_zero_mask_z = vector_less_than(range_extent_z, range_extent_epsilon);
			const Vector4_32 is_range_zero_mask_w = vector_less_than(range_extent_w, range_extent_epsilon);

			// TODO: Trivial AVX or ISPC conversion
			for (uint32_t entry_index = 0; entry_index < num_soa_entries; ++entry_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				Vector4_32 normalized_input_x = vector_div(vector_sub(inputs_x[entry_index], range_min_x), range_extent_x);
				Vector4_32 normalized_input_y = vector_div(vector_sub(inputs_y[entry_index], range_min_y), range_extent_y);
				Vector4_32 normalized_input_z = vector_div(vector_sub(inputs_z[entry_index], range_min_z), range_extent_z);
				Vector4_32 normalized_input_w = vector_div(vector_sub(inputs_w[entry_index], range_min_w), range_extent_w);

				normalized_input_x = vector_min(normalized_input_x, one);
				normalized_input_y = vector_min(normalized_input_y, one);
				normalized_input_z = vector_min(normalized_input_z, one);
				normalized_input_w = vector_min(normalized_input_w, one);

				normalized_input_x = vector_blend(is_range_zero_mask_x, zero, normalized_input_x);
				normalized_input_y = vector_blend(is_range_zero_mask_y, zero, normalized_input_y);
				normalized_input_z = vector_blend(is_range_zero_mask_z, zero, normalized_input_z);
				normalized_input_w = vector_blend(is_range_zero_mask_w, zero, normalized_input_w);

				ACL_ASSERT(vector_all_greater_equal(normalized_input_x, zero) && vector_all_less_equal(normalized_input_x, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_input_x), vector_get_y(normalized_input_x), vector_get_z(normalized_input_x), vector_get_w(normalized_input_x));
				ACL_ASSERT(vector_all_greater_equal(normalized_input_y, zero) && vector_all_less_equal(normalized_input_y, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_input_y), vector_get_y(normalized_input_y), vector_get_z(normalized_input_y), vector_get_w(normalized_input_y));
				ACL_ASSERT(vector_all_greater_equal(normalized_input_z, zero) && vector_all_less_equal(normalized_input_z, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_input_z), vector_get_y(normalized_input_z), vector_get_z(normalized_input_z), vector_get_w(normalized_input_z));
				ACL_ASSERT(vector_all_greater_equal(normalized_input_w, zero) && vector_all_less_equal(normalized_input_w, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_input_w), vector_get_y(normalized_input_w), vector_get_z(normalized_input_w), vector_get_w(normalized_input_w));

				inputs_x[entry_index] = normalized_input_x;
				inputs_y[entry_index] = normalized_input_y;
				inputs_z[entry_index] = normalized_input_z;
				inputs_w[entry_index] = normalized_input_w;
			}
		}

		inline void normalize_vector3f_track(Vector4_32* inputs_x, Vector4_32* inputs_y, Vector4_32* inputs_z, uint32_t num_soa_entries, const Vector4_32& range_min, const Vector4_32& range_extent)
		{
			const Vector4_32 one = vector_set(1.0f);
			const Vector4_32 zero = vector_zero_32();
			const Vector4_32 range_extent_epsilon = vector_set(0.000000001f);

			const float* range_min_ptr = vector_as_float_ptr(range_min);
			const Vector4_32 range_min_x = vector_broadcast(range_min_ptr + 0);
			const Vector4_32 range_min_y = vector_broadcast(range_min_ptr + 1);
			const Vector4_32 range_min_z = vector_broadcast(range_min_ptr + 2);

			const float* range_extent_ptr = vector_as_float_ptr(range_extent);
			const Vector4_32 range_extent_x = vector_broadcast(range_extent_ptr + 0);
			const Vector4_32 range_extent_y = vector_broadcast(range_extent_ptr + 1);
			const Vector4_32 range_extent_z = vector_broadcast(range_extent_ptr + 2);

			const Vector4_32 is_range_zero_mask_x = vector_less_than(range_extent_x, range_extent_epsilon);
			const Vector4_32 is_range_zero_mask_y = vector_less_than(range_extent_y, range_extent_epsilon);
			const Vector4_32 is_range_zero_mask_z = vector_less_than(range_extent_z, range_extent_epsilon);

			// TODO: Trivial AVX or ISPC conversion
			for (uint32_t entry_index = 0; entry_index < num_soa_entries; ++entry_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				Vector4_32 normalized_input_x = vector_div(vector_sub(inputs_x[entry_index], range_min_x), range_extent_x);
				Vector4_32 normalized_input_y = vector_div(vector_sub(inputs_y[entry_index], range_min_y), range_extent_y);
				Vector4_32 normalized_input_z = vector_div(vector_sub(inputs_z[entry_index], range_min_z), range_extent_z);

				normalized_input_x = vector_min(normalized_input_x, one);
				normalized_input_y = vector_min(normalized_input_y, one);
				normalized_input_z = vector_min(normalized_input_z, one);

				normalized_input_x = vector_blend(is_range_zero_mask_x, zero, normalized_input_x);
				normalized_input_y = vector_blend(is_range_zero_mask_y, zero, normalized_input_y);
				normalized_input_z = vector_blend(is_range_zero_mask_z, zero, normalized_input_z);

				ACL_ASSERT(vector_all_greater_equal(normalized_input_x, zero) && vector_all_less_equal(normalized_input_x, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_input_x), vector_get_y(normalized_input_x), vector_get_z(normalized_input_x), vector_get_w(normalized_input_x));
				ACL_ASSERT(vector_all_greater_equal(normalized_input_y, zero) && vector_all_less_equal(normalized_input_y, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_input_y), vector_get_y(normalized_input_y), vector_get_z(normalized_input_y), vector_get_w(normalized_input_y));
				ACL_ASSERT(vector_all_greater_equal(normalized_input_z, zero) && vector_all_less_equal(normalized_input_z, one), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_input_z), vector_get_y(normalized_input_z), vector_get_z(normalized_input_z), vector_get_w(normalized_input_z));

				inputs_x[entry_index] = normalized_input_x;
				inputs_y[entry_index] = normalized_input_y;
				inputs_z[entry_index] = normalized_input_z;
			}
		}

		inline void normalize_with_database_ranges(track_database& database, segment_context& segment, RangeReductionFlags8 range_reduction)
		{
			const bool has_scale = database.has_scale();
			const RotationFormat8 rotation_format = database.get_rotation_format();
			const uint32_t num_transforms = database.get_num_transforms();
			const uint32_t num_soa_entries = segment.num_soa_entries;
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				qvvf_ranges& transform_range = database.get_range(transform_index);

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) && !transform_range.is_rotation_constant)
				{
					if (rotation_format == RotationFormat8::Quat_128)
					{
						Vector4_32* rotations_x;
						Vector4_32* rotations_y;
						Vector4_32* rotations_z;
						Vector4_32* rotations_w;
						database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

						normalize_vector4f_track(rotations_x, rotations_y, rotations_z, rotations_w, num_soa_entries, transform_range.rotation_min, transform_range.rotation_extent);
					}
					else
					{
						Vector4_32* rotations_x;
						Vector4_32* rotations_y;
						Vector4_32* rotations_z;
						database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z);

						normalize_vector3f_track(rotations_x, rotations_y, rotations_z, num_soa_entries, transform_range.rotation_min, transform_range.rotation_extent);
					}

					transform_range.are_rotations_normalized = true;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && !transform_range.is_translation_constant)
				{
					Vector4_32* translations_x;
					Vector4_32* translations_y;
					Vector4_32* translations_z;
					database.get_translations(segment, transform_index, translations_x, translations_y, translations_z);

					normalize_vector3f_track(translations_x, translations_y, translations_z, num_soa_entries, transform_range.translation_min, transform_range.translation_extent);

					transform_range.are_translations_normalized = true;
				}

				if (has_scale && are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && !transform_range.is_scale_constant)
				{
					Vector4_32* scales_x;
					Vector4_32* scales_y;
					Vector4_32* scales_z;
					database.get_scales(segment, transform_index, scales_x, scales_y, scales_z);

					normalize_vector3f_track(scales_x, scales_y, scales_z, num_soa_entries, transform_range.scale_min, transform_range.scale_extent);

					transform_range.are_scales_normalized = true;
				}
			}
		}

		inline void normalize_with_segment_ranges(track_database& database, segment_context& segment, RangeReductionFlags8 range_reduction)
		{
			const RotationFormat8 rotation_format = database.get_rotation_format();
			const uint32_t num_transforms = database.get_num_transforms();
			const uint32_t num_soa_entries = segment.num_soa_entries;
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				qvvf_ranges& segment_range = segment.ranges[transform_index];

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) && !segment_range.is_rotation_constant)
				{
					if (rotation_format == RotationFormat8::Quat_128)
					{
						Vector4_32* rotations_x;
						Vector4_32* rotations_y;
						Vector4_32* rotations_z;
						Vector4_32* rotations_w;
						database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

						normalize_vector4f_track(rotations_x, rotations_y, rotations_z, rotations_w, num_soa_entries, segment_range.rotation_min, segment_range.rotation_extent);
					}
					else
					{
						Vector4_32* rotations_x;
						Vector4_32* rotations_y;
						Vector4_32* rotations_z;
						database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z);

						normalize_vector3f_track(rotations_x, rotations_y, rotations_z, num_soa_entries, segment_range.rotation_min, segment_range.rotation_extent);
					}

					segment_range.are_rotations_normalized = true;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && !segment_range.is_translation_constant)
				{
					Vector4_32* translations_x;
					Vector4_32* translations_y;
					Vector4_32* translations_z;
					database.get_translations(segment, transform_index, translations_x, translations_y, translations_z);

					normalize_vector3f_track(translations_x, translations_y, translations_z, num_soa_entries, segment_range.translation_min, segment_range.translation_extent);

					segment_range.are_translations_normalized = true;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && !segment_range.is_scale_constant)
				{
					Vector4_32* scales_x;
					Vector4_32* scales_y;
					Vector4_32* scales_z;
					database.get_scales(segment, transform_index, scales_x, scales_y, scales_z);

					normalize_vector3f_track(scales_x, scales_y, scales_z, num_soa_entries, segment_range.scale_min, segment_range.scale_extent);

					segment_range.are_scales_normalized = true;
				}
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
