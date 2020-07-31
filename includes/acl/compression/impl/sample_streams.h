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
#include "acl/core/track_formats.h"
#include "acl/core/utils.h"
#include "acl/core/variable_bit_rates.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_packing.h"
#include "acl/compression/impl/track_stream.h"
#include "acl/compression/impl/normalize_streams.h"
#include "acl/compression/impl/convert_rotation_streams.h"

#include <rtm/quatf.h>
#include <rtm/qvvf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline rtm::vector4f RTM_SIMD_CALL load_rotation_sample(const uint8_t* ptr, rotation_format8 format, uint8_t bit_rate)
		{
			switch (format)
			{
			case rotation_format8::quatf_full:
				return unpack_vector4_128(ptr);
			case rotation_format8::quatf_drop_w_full:
				return unpack_vector3_96_unsafe(ptr);
			case rotation_format8::quatf_drop_w_variable:
				ACL_ASSERT(bit_rate != k_invalid_bit_rate, "Invalid bit rate!");
				if (is_constant_bit_rate(bit_rate))
				{
					return unpack_vector3_u48_unsafe(ptr);
				}
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96_unsafe(ptr);
				else
				{
					const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					return unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, ptr, 0);
				}
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return rtm::vector_zero();
			}
		}

		inline rtm::vector4f RTM_SIMD_CALL load_vector_sample(const uint8_t* ptr, vector_format8 format, uint8_t bit_rate)
		{
			switch (format)
			{
			case vector_format8::vector3f_full:
				return unpack_vector3_96_unsafe(ptr);
			case vector_format8::vector3f_variable:
				ACL_ASSERT(bit_rate != k_invalid_bit_rate, "Invalid bit rate!");
				if (is_constant_bit_rate(bit_rate))
					return unpack_vector3_u48_unsafe(ptr);
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96_unsafe(ptr);
				else
				{
					const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					return unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, ptr, 0);
				}
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
				return rtm::vector_zero();
			}
		}

		inline rtm::quatf RTM_SIMD_CALL rotation_to_quat_32(rtm::vector4f_arg0 rotation, rotation_format8 format)
		{
			switch (format)
			{
			case rotation_format8::quatf_full:
				return rtm::vector_to_quat(rotation);
			case rotation_format8::quatf_drop_w_full:
			case rotation_format8::quatf_drop_w_variable:
				return rtm::quat_from_positive_w(rotation);
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return rtm::quat_identity();
			}
		}

		// Gets a rotation sample from the format/bit rate stored
		inline rtm::quatf RTM_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;

			const rotation_format8 format = bone_steams.rotations.get_rotation_format();
			const uint8_t bit_rate = bone_steams.rotations.get_bit_rate();

			if (format == rotation_format8::quatf_drop_w_variable && is_constant_bit_rate(bit_rate))
				sample_index = 0;

			const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);

			rtm::vector4f packed_rotation = acl_impl::load_rotation_sample(quantized_ptr, format, bit_rate);

			if (!bone_steams.is_rotation_constant && clip->are_rotations_normalized && !is_raw_bit_rate(bit_rate))
			{
				if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate))
				{
					const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

					const rtm::vector4f segment_range_min = segment_bone_range.rotation.get_min();
					const rtm::vector4f segment_range_extent = segment_bone_range.rotation.get_extent();

					packed_rotation = rtm::vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				const rtm::vector4f clip_range_min = clip_bone_range.rotation.get_min();
				const rtm::vector4f clip_range_extent = clip_bone_range.rotation.get_extent();

				packed_rotation = rtm::vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
			}

			return acl_impl::rotation_to_quat_32(packed_rotation, format);
		}

		// Gets a rotation sample at the specified bit rate
		inline rtm::quatf RTM_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;
			const rotation_format8 format = bone_steams.rotations.get_rotation_format();

			rtm::vector4f rotation;
			if (is_constant_bit_rate(bit_rate))
			{
				const uint8_t* quantized_ptr = raw_bone_steams.rotations.get_raw_sample_ptr(segment->clip_sample_offset);
				rotation = acl_impl::load_rotation_sample(quantized_ptr, rotation_format8::quatf_full, k_invalid_bit_rate);
				rotation = convert_rotation(rotation, rotation_format8::quatf_full, format);
			}
			else if (is_raw_bit_rate(bit_rate))
			{
				const uint8_t* quantized_ptr = raw_bone_steams.rotations.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
				rotation = acl_impl::load_rotation_sample(quantized_ptr, rotation_format8::quatf_full, k_invalid_bit_rate);
				rotation = convert_rotation(rotation, rotation_format8::quatf_full, format);
			}
			else
			{
				const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);
				rotation = acl_impl::load_rotation_sample(quantized_ptr, format, 0);
			}

			// Pack and unpack at our desired bit rate
			const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			rtm::vector4f packed_rotation;

			if (is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
				const rtm::vector4f normalized_rotation = normalize_sample(rotation, clip_bone_range.rotation);

				packed_rotation = decay_vector3_u48(normalized_rotation);
			}
			else if (is_raw_bit_rate(bit_rate))
				packed_rotation = rotation;
			else
				packed_rotation = decay_vector3_uXX(rotation, num_bits_at_bit_rate);

			if (!is_raw_bit_rate(bit_rate))
			{
				if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate))
				{
					const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

					const rtm::vector4f segment_range_min = segment_bone_range.rotation.get_min();
					const rtm::vector4f segment_range_extent = segment_bone_range.rotation.get_extent();

					packed_rotation = rtm::vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				const rtm::vector4f clip_range_min = clip_bone_range.rotation.get_min();
				const rtm::vector4f clip_range_extent = clip_bone_range.rotation.get_extent();

				packed_rotation = rtm::vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
			}

			return acl_impl::rotation_to_quat_32(packed_rotation, format);
		}

		// Gets a rotation sample with the desired format
		inline rtm::quatf RTM_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index, rotation_format8 desired_format)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;

			const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);
			const rotation_format8 format = bone_steams.rotations.get_rotation_format();

			const rtm::vector4f rotation = acl_impl::load_rotation_sample(quantized_ptr, format, 0);

			// Pack and unpack in our desired format
			rtm::vector4f packed_rotation;

			switch (desired_format)
			{
			case rotation_format8::quatf_full:
			case rotation_format8::quatf_drop_w_full:
				packed_rotation = rotation;
				break;
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(desired_format));
				packed_rotation = rtm::vector_zero();
				break;
			}

			const bool are_rotations_normalized = clip->are_rotations_normalized && !bone_steams.is_rotation_constant;
			if (are_rotations_normalized)
			{
				if (segment->are_rotations_normalized)
				{
					const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

					const rtm::vector4f segment_range_min = segment_bone_range.rotation.get_min();
					const rtm::vector4f segment_range_extent = segment_bone_range.rotation.get_extent();

					packed_rotation = rtm::vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				const rtm::vector4f clip_range_min = clip_bone_range.rotation.get_min();
				const rtm::vector4f clip_range_extent = clip_bone_range.rotation.get_extent();

				packed_rotation = rtm::vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
			}

			return acl_impl::rotation_to_quat_32(packed_rotation, format);
		}

		// Gets a translation sample from the format/bit rate stored
		inline rtm::vector4f RTM_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;
			const bool are_translations_normalized = clip->are_translations_normalized;

			const vector_format8 format = bone_steams.translations.get_vector_format();
			const uint8_t bit_rate = bone_steams.translations.get_bit_rate();

			if (format == vector_format8::vector3f_variable && is_constant_bit_rate(bit_rate))
				sample_index = 0;

			const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

			rtm::vector4f packed_translation = acl_impl::load_vector_sample(quantized_ptr, format, bit_rate);

			if (!bone_steams.is_translation_constant && are_translations_normalized && !is_raw_bit_rate(bit_rate))
			{
				if (segment->are_translations_normalized && !is_constant_bit_rate(bit_rate))
				{
					const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

					const rtm::vector4f segment_range_min = segment_bone_range.translation.get_min();
					const rtm::vector4f segment_range_extent = segment_bone_range.translation.get_extent();

					packed_translation = rtm::vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				const rtm::vector4f clip_range_min = clip_bone_range.translation.get_min();
				const rtm::vector4f clip_range_extent = clip_bone_range.translation.get_extent();

				packed_translation = rtm::vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
			}

			return packed_translation;
		}

		// Gets a translation sample at the specified bit rate
		inline rtm::vector4f RTM_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;
			const vector_format8 format = bone_steams.translations.get_vector_format();

			const uint8_t* quantized_ptr;
			if (is_constant_bit_rate(bit_rate))
				quantized_ptr = raw_bone_steams.translations.get_raw_sample_ptr(segment->clip_sample_offset);
			else if (is_raw_bit_rate(bit_rate))
				quantized_ptr = raw_bone_steams.translations.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
			else
				quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

			const rtm::vector4f translation = acl_impl::load_vector_sample(quantized_ptr, format, 0);

			ACL_ASSERT(clip->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

			// Pack and unpack at our desired bit rate
			rtm::vector4f packed_translation;

			if (is_constant_bit_rate(bit_rate))
			{
				ACL_ASSERT(segment->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

				const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
				const rtm::vector4f normalized_translation = normalize_sample(translation, clip_bone_range.translation);

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

					const rtm::vector4f segment_range_min = segment_bone_range.translation.get_min();
					const rtm::vector4f segment_range_extent = segment_bone_range.translation.get_extent();

					packed_translation = rtm::vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				const rtm::vector4f clip_range_min = clip_bone_range.translation.get_min();
				const rtm::vector4f clip_range_extent = clip_bone_range.translation.get_extent();

				packed_translation = rtm::vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
			}

			return packed_translation;
		}

		// Gets a translation sample with the desired format
		inline rtm::vector4f RTM_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index, vector_format8 desired_format)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;
			const bool are_translations_normalized = clip->are_translations_normalized && !bone_steams.is_translation_constant;
			const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);
			const vector_format8 format = bone_steams.translations.get_vector_format();

			const rtm::vector4f translation = acl_impl::load_vector_sample(quantized_ptr, format, 0);

			// Pack and unpack in our desired format
			rtm::vector4f packed_translation;

			switch (desired_format)
			{
			case vector_format8::vector3f_full:
				packed_translation = translation;
				break;
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
				packed_translation = rtm::vector_zero();
				break;
			}

			if (are_translations_normalized)
			{
				if (segment->are_translations_normalized)
				{
					const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

					rtm::vector4f segment_range_min = segment_bone_range.translation.get_min();
					rtm::vector4f segment_range_extent = segment_bone_range.translation.get_extent();

					packed_translation = rtm::vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				rtm::vector4f clip_range_min = clip_bone_range.translation.get_min();
				rtm::vector4f clip_range_extent = clip_bone_range.translation.get_extent();

				packed_translation = rtm::vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
			}

			return packed_translation;
		}

		// Gets a scale sample from the format/bit rate stored
		inline rtm::vector4f RTM_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;

			const vector_format8 format = bone_steams.scales.get_vector_format();
			const uint8_t bit_rate = bone_steams.scales.get_bit_rate();

			if (format == vector_format8::vector3f_variable && is_constant_bit_rate(bit_rate))
				sample_index = 0;

			const uint8_t* quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);

			rtm::vector4f packed_scale = acl_impl::load_vector_sample(quantized_ptr, format, bit_rate);

			if (!bone_steams.is_scale_constant && clip->are_scales_normalized && !is_raw_bit_rate(bit_rate))
			{
				if (segment->are_scales_normalized && !is_constant_bit_rate(bit_rate))
				{
					const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

					const rtm::vector4f segment_range_min = segment_bone_range.scale.get_min();
					const rtm::vector4f segment_range_extent = segment_bone_range.scale.get_extent();

					packed_scale = rtm::vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				const rtm::vector4f clip_range_min = clip_bone_range.scale.get_min();
				const rtm::vector4f clip_range_extent = clip_bone_range.scale.get_extent();

				packed_scale = rtm::vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
			}

			return packed_scale;
		}

		// Gets a scale sample at the specified bit rate
		inline rtm::vector4f RTM_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;
			const vector_format8 format = bone_steams.scales.get_vector_format();

			const uint8_t* quantized_ptr;
			if (is_constant_bit_rate(bit_rate))
				quantized_ptr = raw_bone_steams.scales.get_raw_sample_ptr(segment->clip_sample_offset);
			else if (is_raw_bit_rate(bit_rate))
				quantized_ptr = raw_bone_steams.scales.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
			else
				quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);

			const rtm::vector4f scale = acl_impl::load_vector_sample(quantized_ptr, format, 0);

			ACL_ASSERT(clip->are_scales_normalized, "Scales must be normalized to support variable bit rates.");

			// Pack and unpack at our desired bit rate
			rtm::vector4f packed_scale;

			if (is_constant_bit_rate(bit_rate))
			{
				ACL_ASSERT(segment->are_scales_normalized, "Translations must be normalized to support variable bit rates.");

				const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
				const rtm::vector4f normalized_scale = normalize_sample(scale, clip_bone_range.scale);

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

					const rtm::vector4f segment_range_min = segment_bone_range.scale.get_min();
					const rtm::vector4f segment_range_extent = segment_bone_range.scale.get_extent();

					packed_scale = rtm::vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				const rtm::vector4f clip_range_min = clip_bone_range.scale.get_min();
				const rtm::vector4f clip_range_extent = clip_bone_range.scale.get_extent();

				packed_scale = rtm::vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
			}

			return packed_scale;
		}

		// Gets a scale sample with the desired format
		inline rtm::vector4f RTM_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index, vector_format8 desired_format)
		{
			const SegmentContext* segment = bone_steams.segment;
			const clip_context* clip = segment->clip;
			const bool are_scales_normalized = clip->are_scales_normalized && !bone_steams.is_scale_constant;
			const uint8_t* quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);
			const vector_format8 format = bone_steams.scales.get_vector_format();

			const rtm::vector4f scale = acl_impl::load_vector_sample(quantized_ptr, format, 0);

			// Pack and unpack in our desired format
			rtm::vector4f packed_scale;

			switch (desired_format)
			{
			case vector_format8::vector3f_full:
				packed_scale = scale;
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

					rtm::vector4f segment_range_min = segment_bone_range.scale.get_min();
					rtm::vector4f segment_range_extent = segment_bone_range.scale.get_extent();

					packed_scale = rtm::vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
				}

				const BoneRanges& clip_bone_range = clip->ranges[bone_steams.bone_index];

				rtm::vector4f clip_range_min = clip_bone_range.scale.get_min();
				rtm::vector4f clip_range_extent = clip_bone_range.scale.get_extent();

				packed_scale = rtm::vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
			}

			return packed_scale;
		}

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
			const clip_context* clip = segment.clip;
			find_linear_interpolation_samples_with_sample_rate(clip->num_samples, clip->sample_rate, sample_time, sample_rounding_policy::nearest, key0, key1, interpolation_alpha);

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
		ACL_FORCE_INLINE rtm::quatf RTM_SIMD_CALL sample_rotation(const sample_context& context, const BoneStreams& bone_stream)
		{
			rtm::quatf rotation;
			if (bone_stream.is_rotation_default)
				rotation = rtm::quat_identity();
			else if (bone_stream.is_rotation_constant)
				rotation = rtm::quat_normalize(get_rotation_sample(bone_stream, 0));
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.rotations.get_num_samples();
					const float sample_rate = bone_stream.rotations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, sample_rounding_policy::none, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				const rtm::quatf sample0 = get_rotation_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const rtm::quatf sample1 = get_rotation_sample(bone_stream, key1);
					rotation = rtm::quat_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					rotation = rtm::quat_normalize(sample0);
			}

			return rotation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE rtm::quatf RTM_SIMD_CALL sample_rotation(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_rotation_variable, rotation_format8 rotation_format)
		{
			rtm::quatf rotation;
			if (bone_stream.is_rotation_default)
				rotation = rtm::quat_identity();
			else if (bone_stream.is_rotation_constant)
			{
				if (is_rotation_variable)
					rotation = get_rotation_sample(raw_bone_stream, 0);
				else
					rotation = get_rotation_sample(raw_bone_stream, 0, rotation_format);

				rotation = rtm::quat_normalize(rotation);
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

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, sample_rounding_policy::none, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				rtm::quatf sample0;
				rtm::quatf sample1;
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
					rotation = rtm::quat_lerp(sample0, sample1, interpolation_alpha);
				else
					rotation = rtm::quat_normalize(sample0);
			}

			return rotation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL sample_translation(const sample_context& context, const BoneStreams& bone_stream)
		{
			rtm::vector4f translation;
			if (bone_stream.is_translation_default)
				translation = rtm::vector_zero();
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

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, sample_rounding_policy::none, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				const rtm::vector4f sample0 = get_translation_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const rtm::vector4f sample1 = get_translation_sample(bone_stream, key1);
					translation = rtm::vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					translation = sample0;
			}

			return translation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL sample_translation(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_translation_variable, vector_format8 translation_format)
		{
			rtm::vector4f translation;
			if (bone_stream.is_translation_default)
				translation = rtm::vector_zero();
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(raw_bone_stream, 0, vector_format8::vector3f_full);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.translations.get_num_samples();
					const float sample_rate = bone_stream.translations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, sample_rounding_policy::none, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				rtm::vector4f sample0;
				rtm::vector4f sample1;
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
					translation = rtm::vector_lerp(sample0, sample1, interpolation_alpha);
				else
					translation = sample0;
			}

			return translation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL sample_scale(const sample_context& context, const BoneStreams& bone_stream, rtm::vector4f_arg0 default_scale)
		{
			rtm::vector4f scale;
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

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, sample_rounding_policy::none, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				const rtm::vector4f sample0 = get_scale_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const rtm::vector4f sample1 = get_scale_sample(bone_stream, key1);
					scale = rtm::vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					scale = sample0;
			}

			return scale;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL sample_scale(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_scale_variable, vector_format8 scale_format, rtm::vector4f_arg0 default_scale)
		{
			rtm::vector4f scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(raw_bone_stream, 0, vector_format8::vector3f_full);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.scales.get_num_samples();
					const float sample_rate = bone_stream.scales.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, sample_rounding_policy::none, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0F;
				}

				rtm::vector4f sample0;
				rtm::vector4f sample1;
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
					scale = rtm::vector_lerp(sample0, sample1, interpolation_alpha);
				else
					scale = sample0;
			}

			return scale;
		}

		inline void sample_streams(const BoneStreams* bone_streams, uint32_t num_bones, float sample_time, rtm::qvvf* out_local_pose)
		{
			const SegmentContext* segment_context = bone_streams->segment;
			const rtm::vector4f default_scale = get_default_scale(segment_context->clip->additive_format);
			const bool has_scale = segment_context->clip->has_scale;

			// With uniform sample distributions, we do not interpolate.
			uint32_t sample_key;
			if (segment_context->distribution == SampleDistribution8::Uniform)
				sample_key = get_uniform_sample_key(*segment_context, sample_time);
			else
				sample_key = 0;	// Not used

			acl_impl::sample_context context;
			context.sample_key = sample_key;
			context.sample_time = sample_time;

			if (segment_context->distribution == SampleDistribution8::Uniform)
			{
				for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					context.track_index = bone_index;

					const BoneStreams& bone_stream = bone_streams[bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;

					out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
				}
			}
			else
			{
				for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					context.track_index = bone_index;

					const BoneStreams& bone_stream = bone_streams[bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;

					out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
				}
			}
		}

		inline void sample_stream(const BoneStreams* bone_streams, uint32_t num_bones, float sample_time, uint32_t bone_index, rtm::qvvf* out_local_pose)
		{
			(void)num_bones;

			const SegmentContext* segment_context = bone_streams->segment;
			const rtm::vector4f default_scale = get_default_scale(segment_context->clip->additive_format);
			const bool has_scale = segment_context->clip->has_scale;

			// With uniform sample distributions, we do not interpolate.
			uint32_t sample_key;
			if (segment_context->distribution == SampleDistribution8::Uniform)
				sample_key = get_uniform_sample_key(*segment_context, sample_time);
			else
				sample_key = 0;	// Not used

			acl_impl::sample_context context;
			context.track_index = bone_index;
			context.sample_key = sample_key;
			context.sample_time = sample_time;

			const BoneStreams& bone_stream = bone_streams[bone_index];

			rtm::quatf rotation;
			rtm::vector4f translation;
			rtm::vector4f scale;
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

			out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
		}

		inline void sample_streams_hierarchical(const BoneStreams* bone_streams, uint32_t num_bones, float sample_time, uint32_t bone_index, rtm::qvvf* out_local_pose)
		{
			(void)num_bones;

			const SegmentContext* segment_context = bone_streams->segment;
			const rtm::vector4f default_scale = get_default_scale(segment_context->clip->additive_format);
			const bool has_scale = segment_context->clip->has_scale;

			// With uniform sample distributions, we do not interpolate.
			uint32_t sample_key;
			if (segment_context->distribution == SampleDistribution8::Uniform)
				sample_key = get_uniform_sample_key(*segment_context, sample_time);
			else
				sample_key = 0;	// Not used

			acl_impl::sample_context context;
			context.sample_key = sample_key;
			context.sample_time = sample_time;

			if (segment_context->distribution == SampleDistribution8::Uniform)
			{
				uint32_t current_bone_index = bone_index;
				while (current_bone_index != k_invalid_track_index)
				{
					context.track_index = current_bone_index;

					const BoneStreams& bone_stream = bone_streams[current_bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;

					out_local_pose[current_bone_index] = rtm::qvv_set(rotation, translation, scale);
					current_bone_index = bone_stream.parent_bone_index;
				}
			}
			else
			{
				uint32_t current_bone_index = bone_index;
				while (current_bone_index != k_invalid_track_index)
				{
					context.track_index = current_bone_index;

					const BoneStreams& bone_stream = bone_streams[current_bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;

					out_local_pose[current_bone_index] = rtm::qvv_set(rotation, translation, scale);
					current_bone_index = bone_stream.parent_bone_index;
				}
			}
		}

		inline void sample_streams(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint32_t num_bones, float sample_time, const BoneBitRate* bit_rates, rotation_format8 rotation_format, vector_format8 translation_format, vector_format8 scale_format, rtm::qvvf* out_local_pose)
		{
			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);
			const bool is_scale_variable = is_vector_format_variable(scale_format);

			const SegmentContext* segment_context = bone_streams->segment;
			const rtm::vector4f default_scale = get_default_scale(segment_context->clip->additive_format);
			const bool has_scale = segment_context->clip->has_scale;

			// With uniform sample distributions, we do not interpolate.
			uint32_t sample_key;
			if (segment_context->distribution == SampleDistribution8::Uniform)
				sample_key = get_uniform_sample_key(*segment_context, sample_time);
			else
				sample_key = 0;	// Not used

			acl_impl::sample_context context;
			context.sample_key = sample_key;
			context.sample_time = sample_time;

			if (segment_context->distribution == SampleDistribution8::Uniform)
			{
				for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					context.track_index = bone_index;
					context.bit_rates = bit_rates[bone_index];

					const BoneStreams& bone_stream = bone_streams[bone_index];
					const BoneStreams& raw_bone_steam = raw_bone_steams[bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_rotation_variable, rotation_format);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_translation_variable, translation_format);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_scale_variable, scale_format, default_scale) : default_scale;

					out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
				}
			}
			else
			{
				for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					context.track_index = bone_index;
					context.bit_rates = bit_rates[bone_index];

					const BoneStreams& bone_stream = bone_streams[bone_index];
					const BoneStreams& raw_bone_steam = raw_bone_steams[bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_rotation_variable, rotation_format);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_translation_variable, translation_format);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_scale_variable, scale_format, default_scale) : default_scale;

					out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
				}
			}
		}

		inline void sample_stream(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint32_t num_bones, float sample_time, uint32_t bone_index, const BoneBitRate* bit_rates, rotation_format8 rotation_format, vector_format8 translation_format, vector_format8 scale_format, rtm::qvvf* out_local_pose)
		{
			(void)num_bones;

			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);
			const bool is_scale_variable = is_vector_format_variable(scale_format);

			const SegmentContext* segment_context = bone_streams->segment;
			const rtm::vector4f default_scale = get_default_scale(segment_context->clip->additive_format);
			const bool has_scale = segment_context->clip->has_scale;

			// With uniform sample distributions, we do not interpolate.
			uint32_t sample_key;
			if (segment_context->distribution == SampleDistribution8::Uniform)
				sample_key = get_uniform_sample_key(*segment_context, sample_time);
			else
				sample_key = 0;	// Not used

			acl_impl::sample_context context;
			context.track_index = bone_index;
			context.sample_key = sample_key;
			context.sample_time = sample_time;
			context.bit_rates = bit_rates[bone_index];

			const BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneStreams& raw_bone_stream = raw_bone_steams[bone_index];

			rtm::quatf rotation;
			rtm::vector4f translation;
			rtm::vector4f scale;
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

			out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
		}

		inline void sample_streams_hierarchical(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint32_t num_bones, float sample_time, uint32_t bone_index, const BoneBitRate* bit_rates, rotation_format8 rotation_format, vector_format8 translation_format, vector_format8 scale_format, rtm::qvvf* out_local_pose)
		{
			(void)num_bones;

			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);
			const bool is_scale_variable = is_vector_format_variable(scale_format);

			const SegmentContext* segment_context = bone_streams->segment;
			const rtm::vector4f default_scale = get_default_scale(segment_context->clip->additive_format);
			const bool has_scale = segment_context->clip->has_scale;

			// With uniform sample distributions, we do not interpolate.
			uint32_t sample_key;
			if (segment_context->distribution == SampleDistribution8::Uniform)
				sample_key = get_uniform_sample_key(*segment_context, sample_time);
			else
				sample_key = 0;	// Not used

			acl_impl::sample_context context;
			context.sample_key = sample_key;
			context.sample_time = sample_time;

			if (segment_context->distribution == SampleDistribution8::Uniform)
			{
				uint32_t current_bone_index = bone_index;
				while (current_bone_index != k_invalid_track_index)
				{
					context.track_index = current_bone_index;
					context.bit_rates = bit_rates[current_bone_index];

					const BoneStreams& bone_stream = bone_streams[current_bone_index];
					const BoneStreams& raw_bone_stream = raw_bone_steams[current_bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;

					out_local_pose[current_bone_index] = rtm::qvv_set(rotation, translation, scale);
					current_bone_index = bone_stream.parent_bone_index;
				}
			}
			else
			{
				uint32_t current_bone_index = bone_index;
				while (current_bone_index != k_invalid_track_index)
				{
					context.track_index = current_bone_index;
					context.bit_rates = bit_rates[current_bone_index];

					const BoneStreams& bone_stream = bone_streams[current_bone_index];
					const BoneStreams& raw_bone_stream = raw_bone_steams[current_bone_index];

					const rtm::quatf rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
					const rtm::vector4f translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;

					out_local_pose[current_bone_index] = rtm::qvv_set(rotation, translation, scale);
					current_bone_index = bone_stream.parent_bone_index;
				}
			}
		}

		inline void sample_streams(const BoneStreams* bone_streams, uint32_t num_bones, uint32_t sample_index, rtm::qvvf* out_local_pose)
		{
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = bone_streams[bone_index];

				const uint32_t rotation_sample_index = !bone_stream.is_rotation_constant ? sample_index : 0;
				const rtm::quatf rotation = get_rotation_sample(bone_stream, rotation_sample_index);

				const uint32_t translation_sample_index = !bone_stream.is_translation_constant ? sample_index : 0;
				const rtm::vector4f translation = get_translation_sample(bone_stream, translation_sample_index);

				const uint32_t scale_sample_index = !bone_stream.is_scale_constant ? sample_index : 0;
				const rtm::vector4f scale = get_scale_sample(bone_stream, scale_sample_index);

				out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
