#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2022 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/segment_context.h"
#include "acl/compression/impl/track_list_context.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline void optimize_looping(clip_context& context, const track_array_qvvf& track_list, const compression_settings& settings)
		{
			if (!settings.optimize_loops)
				return;	// We don't want to optimize loops, nothing to do

			if (context.looping_policy == sample_looping_policy::wrap)
				return;	// Already optimized, nothing to do

			if (settings.rotation_format == rotation_format8::quatf_full &&
				settings.translation_format == vector_format8::vector3f_full &&
				settings.scale_format == vector_format8::vector3f_full)
				return;	// We requested raw data, don't optimize anything

			if (context.num_samples <= 1)
				return;	// We have 1 or fewer samples, can't wrap

			if (context.num_bones == 0)
				return;	// No data present

			ACL_ASSERT(context.segments[0].bone_streams->rotations.get_rotation_format() == rotation_format8::quatf_full, "Expected full precision");
			ACL_ASSERT(context.segments[0].bone_streams->translations.get_vector_format() == vector_format8::vector3f_full, "Expected full precision");
			ACL_ASSERT(context.segments[0].bone_streams->scales.get_vector_format() == vector_format8::vector3f_full, "Expected full precision");
			ACL_ASSERT(context.num_segments == 1, "Cannot optimize multi-segments");

			// Detect if our last sample matches the first, if it does we are looping and we can
			// remove the last sample and wrap instead of clamping
			bool is_wrapping = true;

			segment_context& segment = context.segments[0];
			const uint32_t last_sample_index = segment.num_samples - 1;

			const uint32_t num_transforms = segment.num_bones;
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const track_desc_transformf& desc = track_list[transform_index].get_description();

				const rtm::quatf first_rotation = segment.bone_streams[transform_index].rotations.get_raw_sample<rtm::quatf>(0);
				const rtm::quatf inv_first_rotation = rtm::quat_conjugate(first_rotation);
				const rtm::quatf last_rotation = segment.bone_streams[transform_index].rotations.get_raw_sample<rtm::quatf>(last_sample_index);
				const rtm::quatf delta_rotation = rtm::quat_normalize(rtm::quat_mul(inv_first_rotation, last_rotation));
				if (!rtm::quat_near_identity(delta_rotation, desc.constant_rotation_threshold_angle))
				{
					is_wrapping = false;
					break;
				}

				const rtm::vector4f first_translation = segment.bone_streams[transform_index].translations.get_raw_sample<rtm::vector4f>(0);
				const rtm::vector4f last_translation = segment.bone_streams[transform_index].translations.get_raw_sample<rtm::vector4f>(last_sample_index);
				if (!rtm::vector_all_near_equal3(first_translation, last_translation, desc.constant_translation_threshold))
				{
					is_wrapping = false;
					break;
				}

				if (context.has_scale)
				{
					const rtm::vector4f first_scale = segment.bone_streams[transform_index].scales.get_raw_sample<rtm::vector4f>(0);
					const rtm::vector4f last_scale = segment.bone_streams[transform_index].scales.get_raw_sample<rtm::vector4f>(last_sample_index);
					if (!rtm::vector_all_near_equal3(first_scale, last_scale, desc.constant_scale_threshold))
					{
						is_wrapping = false;
						break;
					}
				}
			}

			if (is_wrapping)
			{
				// Our last sample matches the first, we can wrap
				context.num_samples--;
				context.looping_policy = sample_looping_policy::wrap;

				segment.num_samples--;

				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					segment.bone_streams[transform_index].rotations.strip_last_sample();
					segment.bone_streams[transform_index].translations.strip_last_sample();

					if (context.has_scale)
						segment.bone_streams[transform_index].scales.strip_last_sample();
				}
			}
		}

		inline void optimize_looping(track_list_context& context, const compression_settings& settings)
		{
			if (!settings.optimize_loops)
				return;	// We don't want to optimize loops, nothing to do

			if (context.looping_policy == sample_looping_policy::wrap)
				return;	// Already optimized, nothing to do

			if (context.num_samples <= 1)
				return;	// We have 1 or fewer samples, can't wrap

			if (context.num_tracks == 0)
				return;	// No data present

			// Detect if our last sample matches the first, if it does we are looping and we can
			// remove the last sample and wrap instead of clamping
			bool is_wrapping = true;

			track_array& track_list = context.track_list;
			const uint32_t last_sample_index = context.num_samples - 1;

			const uint32_t num_tracks = context.num_tracks;
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				const track_vector4f& typed_track = track_cast<const track_vector4f>(track_list[track_index]);
				const track_desc_scalarf& desc = typed_track.get_description();

				const rtm::vector4f first_sample = typed_track[0];
				const rtm::vector4f last_sample = typed_track[last_sample_index];
				if (!rtm::vector_all_near_equal(first_sample, last_sample, desc.precision))
				{
					is_wrapping = false;
					break;
				}
			}

			if (is_wrapping)
			{
				// Our last sample matches the first, we can wrap
				const uint32_t num_samples = context.num_samples - 1;
				const float sample_rate = context.sample_rate;
				iallocator& allocator = *context.allocator;

				context.num_samples = num_samples;
				context.looping_policy = sample_looping_policy::wrap;

				track_array_vector4f wrap_track_list(allocator, num_tracks);

				for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
				{
					const track_vector4f& ref_track = track_cast<const track_vector4f>(track_list[track_index]);
					const track_desc_scalarf& desc = ref_track.get_description();

					track_vector4f& wrap_track = wrap_track_list[track_index];
					wrap_track = track_vector4f::make_copy(desc, allocator, ref_track.get_data(), num_samples, sample_rate);
				}

				track_list = std::move(wrap_track_list);
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
