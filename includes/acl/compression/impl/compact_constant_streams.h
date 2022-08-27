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

#include "acl/version.h"
#include "acl/core/iallocator.h"
#include "acl/core/track_formats.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/compression/impl/clip_context.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
		inline bool is_rotation_track_constant(const rotation_track_stream& rotations, float threshold_angle, rtm::quatf_arg0 ref_rotation)
#else
		inline bool is_rotation_track_constant(const rotation_track_stream& rotations, float threshold_angle)
#endif
		{

#if !defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
			// Calculating the average rotation and comparing every rotation in the track to it
			// to determine if we are within the threshold seems overkill. We can't use the min/max for the range
			// either because neither of those represents a valid rotation. Instead we grab
			// the first rotation, and compare everything else to it.
#endif

			auto sample_to_quat = [](const rotation_track_stream& track, uint32_t sample_index)
			{
				const rtm::vector4f rotation = track.get_raw_sample<rtm::vector4f>(sample_index);

				switch (track.get_rotation_format())
				{
				case rotation_format8::quatf_full:
					return rtm::vector_to_quat(rotation);
				case rotation_format8::quatf_drop_w_full:
				case rotation_format8::quatf_drop_w_variable:
					return rtm::quat_from_positive_w(rotation);
				default:
					ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(track.get_rotation_format()));
					return rtm::vector_to_quat(rotation);
				}
			};

			const uint32_t num_samples = rotations.get_num_samples();
			if (num_samples <= 1)
				return true;

#if !defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
			const rtm::quatf ref_rotation = sample_to_quat(rotations, 0);
#endif

			const rtm::quatf inv_ref_rotation = rtm::quat_conjugate(ref_rotation);

			// If our error threshold is zero we want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			const bool is_threshold_zero = threshold_angle == 0.0F;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
#else
			for (uint32_t sample_index = 1; sample_index < num_samples; ++sample_index)
#endif

			{
				const rtm::quatf rotation = sample_to_quat(rotations, sample_index);

				if (is_threshold_zero)
				{
					if (!rtm::quat_are_equal(rotation, ref_rotation))
						return false;
				}
				else
				{
					const rtm::quatf delta = rtm::quat_normalize(rtm::quat_mul(inv_ref_rotation, rotation));
					if (!rtm::quat_near_identity(delta, threshold_angle))
						return false;
				}
			}

			return true;
		}

		inline void compact_constant_streams(iallocator& allocator, clip_context& context, const track_array_qvvf& track_list, const compression_settings& settings)
		{
			ACL_ASSERT(context.num_segments == 1, "context must contain a single segment!");
			segment_context& segment = context.segments[0];

			const uint32_t num_bones = context.num_bones;
			const uint32_t num_samples = context.num_samples;

			uint32_t num_default_bone_scales = 0;

			// When a stream is constant, we only keep the first sample
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const track_desc_transformf& desc = track_list[bone_index].get_description();

				transform_streams& bone_stream = segment.bone_streams[bone_index];
				transform_range& bone_range = context.ranges[bone_index];

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", bone_stream.rotations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", bone_stream.translations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", bone_stream.scales.get_sample_size(), sizeof(rtm::vector4f));

				// If we request raw data, use a 0.0 threshold for safety
				const float constant_rotation_threshold_angle = settings.rotation_format != rotation_format8::quatf_full ? desc.constant_rotation_threshold_angle : 0.0F;
				const float constant_translation_threshold = settings.translation_format != vector_format8::vector3f_full ? desc.constant_translation_threshold : 0.0F;
				const float constant_scale_threshold = settings.scale_format != vector_format8::vector3f_full ? desc.constant_scale_threshold : 0.0F;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
				if (is_rotation_track_constant(bone_stream.rotations, constant_rotation_threshold_angle, rtm::vector_to_quat(bone_range.rotation.get_weighted_average())))
#else
				if (is_rotation_track_constant(bone_stream.rotations, constant_rotation_threshold_angle))
#endif
				{
					rotation_track_stream constant_stream(allocator, 1, bone_stream.rotations.get_sample_size(), bone_stream.rotations.get_sample_rate(), bone_stream.rotations.get_rotation_format());

					const rtm::vector4f default_bind_rotation = rtm::quat_to_vector(desc.default_value.rotation);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					rtm::vector4f rotation = num_samples != 0 ? bone_range.rotation.get_weighted_average() : default_bind_rotation;
#else
					rtm::vector4f rotation = num_samples != 0 ? bone_stream.rotations.get_raw_sample<rtm::vector4f>(0) : default_bind_rotation;
#endif

					bone_stream.is_rotation_constant = true;

					// If our error threshold is zero we want to test if we are binary exact
					// This is used by raw clips, we must preserve the original values
					if (constant_rotation_threshold_angle == 0.0F)
						bone_stream.is_rotation_default = rtm::vector_all_equal(rotation, default_bind_rotation);
					else
						bone_stream.is_rotation_default = rtm::quat_near_identity(rtm::quat_normalize(rtm::quat_mul(rtm::vector_to_quat(rotation), rtm::quat_conjugate(rtm::vector_to_quat(default_bind_rotation)))), constant_rotation_threshold_angle);

					if (bone_stream.is_rotation_default)
						rotation = default_bind_rotation;

					constant_stream.set_raw_sample(0, rotation);
					bone_stream.rotations = std::move(constant_stream);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					bone_range.rotation = track_stream_range::from_min_extent(rotation, rtm::vector_zero(), rotation);
#else
					bone_range.rotation = track_stream_range::from_min_extent(rotation, rtm::vector_zero());
#endif
				}

				if (bone_range.translation.is_constant(constant_translation_threshold))
				{
					translation_track_stream constant_stream(allocator, 1, bone_stream.translations.get_sample_size(), bone_stream.translations.get_sample_rate(), bone_stream.translations.get_vector_format());

					const rtm::vector4f default_bind_translation = desc.default_value.translation;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					rtm::vector4f translation = num_samples != 0 ? bone_range.translation.get_weighted_average() : default_bind_translation;
#else
					rtm::vector4f translation = num_samples != 0 ? bone_stream.translations.get_raw_sample<rtm::vector4f>(0) : default_bind_translation;
#endif

					bone_stream.is_translation_constant = true;

					// If our error threshold is zero we want to test if we are binary exact
					// This is used by raw clips, we must preserve the original values
					if (constant_translation_threshold == 0.0F)
						bone_stream.is_translation_default = rtm::vector_all_equal3(translation, default_bind_translation);
					else
						bone_stream.is_translation_default = rtm::vector_all_near_equal3(translation, default_bind_translation, constant_translation_threshold);

					if (bone_stream.is_translation_default)
						translation = default_bind_translation;

					constant_stream.set_raw_sample(0, translation);
					bone_stream.translations = std::move(constant_stream);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					bone_range.translation = track_stream_range::from_min_extent(translation, rtm::vector_zero(), translation);
#else
					bone_range.translation = track_stream_range::from_min_extent(translation, rtm::vector_zero());
#endif
				}

				if (bone_range.scale.is_constant(constant_scale_threshold))
				{
					scale_track_stream constant_stream(allocator, 1, bone_stream.scales.get_sample_size(), bone_stream.scales.get_sample_rate(), bone_stream.scales.get_vector_format());

					const rtm::vector4f default_bind_scale = desc.default_value.scale;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					rtm::vector4f scale = (context.has_scale && (num_samples != 0)) ? bone_range.scale.get_weighted_average() : default_bind_scale;
#else
					rtm::vector4f scale = (context.has_scale && num_samples != 0) ? bone_stream.scales.get_raw_sample<rtm::vector4f>(0) : default_bind_scale;
#endif

					bone_stream.is_scale_constant = true;

					// If our error threshold is zero we want to test if we are binary exact
					// This is used by raw clips, we must preserve the original values
					if (constant_scale_threshold == 0.0F)
						bone_stream.is_scale_default = rtm::vector_all_equal3(scale, default_bind_scale);
					else
						bone_stream.is_scale_default = rtm::vector_all_near_equal3(scale, default_bind_scale, constant_scale_threshold);

					if (bone_stream.is_scale_default)
						scale = default_bind_scale;

					constant_stream.set_raw_sample(0, scale);
					bone_stream.scales = std::move(constant_stream);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					bone_range.scale = track_stream_range::from_min_extent(scale, rtm::vector_zero(), scale);
#else
					bone_range.scale = track_stream_range::from_min_extent(scale, rtm::vector_zero());
#endif

					num_default_bone_scales += bone_stream.is_scale_default ? 1 : 0;
				}
			}

			context.has_scale = num_default_bone_scales != num_bones;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
