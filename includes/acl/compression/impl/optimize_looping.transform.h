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

#include "acl/version.h"
#include "acl/core/iallocator.h"
#include "acl/core/error.h"
#include "acl/core/scope_profiler.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/compression_stats.h"
#include "acl/compression/impl/segment_context.h"
#include "acl/compression/impl/transform_clip_adapters.h"
#include "acl/compression/impl/topology_metadata.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		template<class clip_adapter_t>
		inline bool is_clip_looping(
			iallocator& allocator,
			const clip_adapter_t& raw_clip,
			const clip_adapter_t& additive_base_clip,
			const itransform_error_metric& error_metric)
		{
			static_assert(std::is_base_of<transform_clip_adapter_t, clip_adapter_t>::value, "Clip adapter must derive from transform_clip_adapter_t");

			const uint32_t num_samples = raw_clip.get_num_samples();
			if (num_samples <= 1)
				return false;	// We have 1 or fewer samples, can't wrap

			const uint32_t num_transforms = raw_clip.get_num_transforms();
			if (num_transforms == 0)
				return false;	// No data present

			const bool has_scale = true;	// We assume we have scale for simplicity
			if (error_metric.needs_conversion(has_scale))
				return false;	// We don't support error metrics that require conversion

			const uint32_t first_sample_index = 0;
			const uint32_t last_sample_index = num_samples - 1;

			const bool has_additive_base = raw_clip.has_additive_base();
			const uint32_t base_num_samples = has_additive_base ? additive_base_clip.get_num_samples() : 0;
			const uint32_t base_first_sample_index = 0;
			const uint32_t base_last_sample_index = has_additive_base ? (base_num_samples - 1) : 0;

			uint32_t* dirty_transform_indices = allocate_type_array<uint32_t>(allocator, num_transforms);
			uint32_t* parent_transform_indices = allocate_type_array<uint32_t>(allocator, num_transforms);
			rtm::qvvf* clip_transforms = allocate_type_array<rtm::qvvf>(allocator, num_transforms * 2);
			rtm::qvvf* base_transforms = allocate_type_array<rtm::qvvf>(allocator, num_transforms * 2);

			rtm::qvvf* clip_transforms_first = clip_transforms;
			rtm::qvvf* clip_transforms_last = clip_transforms + num_transforms;

			rtm::qvvf* base_transforms_first = base_transforms;
			rtm::qvvf* base_transforms_last = base_transforms + num_transforms;

			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				dirty_transform_indices[transform_index] = transform_index;
				parent_transform_indices[transform_index] = raw_clip.get_transform_parent_index(transform_index);
			}

			// Sample our transforms in local space
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const rtm::quatf first_rotation = raw_clip.get_transform_rotation(transform_index, first_sample_index);
				const rtm::vector4f first_translation = raw_clip.get_transform_translation(transform_index, first_sample_index);
				const rtm::vector4f first_scale = raw_clip.get_transform_scale(transform_index, first_sample_index);

				const rtm::quatf last_rotation = raw_clip.get_transform_rotation(transform_index, last_sample_index);
				const rtm::vector4f last_translation = raw_clip.get_transform_translation(transform_index, last_sample_index);
				const rtm::vector4f last_scale = raw_clip.get_transform_scale(transform_index, last_sample_index);

				clip_transforms_first[transform_index] = rtm::qvv_set(first_rotation, first_translation, first_scale);
				clip_transforms_last[transform_index] = rtm::qvv_set(last_rotation, last_translation, last_scale);

				if (has_additive_base)
				{
					const rtm::quatf base_first_rotation = additive_base_clip.get_transform_rotation(transform_index, base_first_sample_index);
					const rtm::vector4f base_first_translation = additive_base_clip.get_transform_translation(transform_index, base_first_sample_index);
					const rtm::vector4f base_first_scale = additive_base_clip.get_transform_scale(transform_index, base_first_sample_index);

					const rtm::quatf base_last_rotation = additive_base_clip.get_transform_rotation(transform_index, base_last_sample_index);
					const rtm::vector4f base_last_translation = additive_base_clip.get_transform_translation(transform_index, base_last_sample_index);
					const rtm::vector4f base_last_scale = additive_base_clip.get_transform_scale(transform_index, base_last_sample_index);

					base_transforms_first[transform_index] = rtm::qvv_set(base_first_rotation, base_first_translation, base_first_scale);
					base_transforms_last[transform_index] = rtm::qvv_set(base_last_rotation, base_last_translation, base_last_scale);
				}
			}

			// Apply our base if we have one
			if (has_additive_base)
			{
				itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args;
				apply_additive_to_base_args.dirty_transform_indices = dirty_transform_indices;
				apply_additive_to_base_args.num_dirty_transforms = num_transforms;
				apply_additive_to_base_args.base_transforms = (const void*)base_transforms_first;
				apply_additive_to_base_args.local_transforms = (const void*)clip_transforms_first;
				apply_additive_to_base_args.num_transforms = num_transforms;

				error_metric.apply_additive_to_base(apply_additive_to_base_args, clip_transforms_first);

				apply_additive_to_base_args.base_transforms = (const void*)base_transforms_last;
				apply_additive_to_base_args.local_transforms = (const void*)clip_transforms_last;

				error_metric.apply_additive_to_base(apply_additive_to_base_args, clip_transforms_last);
			}

			// Convert to object space
			{
				itransform_error_metric::local_to_object_space_args local_to_object_space_args;
				local_to_object_space_args.dirty_transform_indices = dirty_transform_indices;
				local_to_object_space_args.num_dirty_transforms = num_transforms;
				local_to_object_space_args.parent_transform_indices = parent_transform_indices;
				local_to_object_space_args.local_transforms = (const void*)clip_transforms_first;
				local_to_object_space_args.num_transforms = num_transforms;

				error_metric.local_to_object_space(local_to_object_space_args, clip_transforms_first);

				local_to_object_space_args.local_transforms = (const void*)clip_transforms_last;

				error_metric.local_to_object_space(local_to_object_space_args, clip_transforms_last);
			}

			// Detect if our last sample matches the first, if it does we are looping and we can
			// remove the last sample and wrap instead of clamping
			bool is_wrapping = true;
			{
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const float shell_distance = raw_clip.get_transform_shell_distance(transform_index);
					const rtm::scalarf precision = rtm::scalar_set(raw_clip.get_transform_precision(transform_index));
					const rtm::scalarf precision_sq = rtm::scalar_mul(precision, precision);

					itransform_error_metric::calculate_error_args calculate_error_args;
					calculate_error_args.construct_sphere_shell(shell_distance);
					calculate_error_args.transform0 = clip_transforms_first + transform_index;
					calculate_error_args.transform1 = clip_transforms_last + transform_index;

					const rtm::scalarf error_sq = error_metric.calculate_error_squared(calculate_error_args);

					// If our error exceeds the desired precision, we are not wrapping
					if (rtm::scalar_greater_than(error_sq, precision_sq))
					{
						is_wrapping = false;
						break;
					}
				}
			}

			deallocate_type_array(allocator, dirty_transform_indices, num_transforms);
			deallocate_type_array(allocator, parent_transform_indices, num_transforms);
			deallocate_type_array(allocator, clip_transforms, num_transforms * 2);
			deallocate_type_array(allocator, base_transforms, num_transforms * 2);

			return is_wrapping;
		}

		inline void optimize_looping(
			clip_context& context,
			const clip_context& additive_base_clip_context,
			const compression_settings& settings,
			compression_stats_t& compression_stats)
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

			(void)compression_stats;

#if defined(ACL_USE_SJSON)
			scope_profiler optimize_loop_time;
#endif

			segment_context& segment = context.segments[0];

			ACL_ASSERT(segment.bone_streams->rotations.get_rotation_format() == rotation_format8::quatf_full, "Expected full precision");
			ACL_ASSERT(segment.bone_streams->translations.get_vector_format() == vector_format8::vector3f_full, "Expected full precision");
			ACL_ASSERT(segment.bone_streams->scales.get_vector_format() == vector_format8::vector3f_full, "Expected full precision");
			ACL_ASSERT(context.num_segments == 1, "Cannot optimize multi-segments");

			// Detect if our last sample matches the first, if it does we are looping and we can
			// remove the last sample and wrap instead of clamping
			const bool is_wrapping = is_clip_looping(
				*context.allocator,
				transform_clip_context_adapter_t(context),
				transform_clip_context_adapter_t(additive_base_clip_context),
				*settings.error_metric);

			if (is_wrapping)
			{
				// Our last sample matches the first, we can wrap
				context.num_samples--;
				context.looping_policy = sample_looping_policy::wrap;

				segment.num_samples--;

				const uint32_t num_transforms = segment.num_bones;
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					segment.bone_streams[transform_index].rotations.strip_last_sample();
					segment.bone_streams[transform_index].translations.strip_last_sample();

					if (context.has_scale)
						segment.bone_streams[transform_index].scales.strip_last_sample();
				}
			}

#if defined(ACL_USE_SJSON)
			compression_stats.optimize_looping_elapsed_seconds = optimize_loop_time.get_elapsed_seconds();
#endif
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
