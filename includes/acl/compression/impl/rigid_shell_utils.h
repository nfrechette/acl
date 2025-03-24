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
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/sample_streams.h"
#include "acl/compression/impl/transform_clip_adapters.h"

#include <rtm/qvvf.h>

#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// We use the raw data to compute the rigid shell
		// For each transform, its rigid shell is formed by the dominant joint (itself or a child)
		// We compute the largest value over the whole clip per transform
		template<class clip_adapter_t>
		inline rigid_shell_metadata_t* compute_clip_shell_distances(
			iallocator& allocator,
			const clip_adapter_t& raw_clip,
			const clip_adapter_t& additive_base_clip)
		{
			static_assert(std::is_base_of<transform_clip_adapter_t, clip_adapter_t>::value, "Clip adapter must derive from transform_clip_adapter_t");

			const uint32_t num_transforms = raw_clip.get_num_transforms();
			if (num_transforms == 0)
				return nullptr;	// No transforms present, no shell distances

			const uint32_t num_samples = raw_clip.get_num_samples();
			if (num_samples == 0)
				return nullptr;	// No samples present, no shell distances

			const float sample_rate = raw_clip.get_sample_rate();
			const float duration = raw_clip.get_duration();
			const bool has_additive_base = raw_clip.has_additive_base();
			const additive_clip_format8 additive_format = raw_clip.get_additive_format();
			const uint32_t base_num_samples = has_additive_base ? additive_base_clip.get_num_samples() : 0;
			const float base_duration = has_additive_base ? additive_base_clip.get_duration() : 0.0F;
			const clip_topology_t* topology = raw_clip.get_topology();

			// We retain only one dominant sub-transform per transform but in reality, it could change from keyframe to keyframe
			// To that end, we compute the local shell distance in object space for each transform at every sample and we
			// retain the largest value. We then use this maximum value to compute our dominance.

			rtm::qvvf* object_transforms = allocate_type_array<rtm::qvvf>(allocator, num_transforms);
			float* max_distance_to_parents = allocate_type_array<float>(allocator, num_transforms);

			std::memset(max_distance_to_parents, 0, sizeof(float) * num_transforms);

			// Our output buffer we'll return
			rigid_shell_metadata_t* shell_metadata = allocate_type_array<rigid_shell_metadata_t>(allocator, num_transforms);

			// Initialize our output shell metadata
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				rigid_shell_metadata_t& transform_shell_metadata = shell_metadata[transform_index];

				transform_shell_metadata.local_shell_distance = raw_clip.get_transform_shell_distance(transform_index);
				transform_shell_metadata.precision = raw_clip.get_transform_precision(transform_index);
				transform_shell_metadata.dominant_transform_index = transform_index;
			}

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				uint32_t base_sample_index = 0;
				if (has_additive_base)
				{
					// The sample time is calculated from the full clip duration to be consistent with decompression
					const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

					const float normalized_sample_time = base_num_samples > 1 ? (sample_time / duration) : 0.0F;
					const float additive_sample_time = base_num_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

					// With uniform sample distributions, we do not interpolate.
					base_sample_index = get_uniform_sample_key(additive_base_clip, transform_segment_adapter_t(), additive_sample_time);
				}

				// Retrieve the object space transforms for this sample
				for (const uint32_t transform_index : topology->roots_first_iterator())
				{
					const uint32_t parent_index = topology->transforms[transform_index].parent_index;

					// Sample our local transform
					const rtm::quatf rotation = raw_clip.get_transform_rotation(transform_index, sample_index);
					const rtm::vector4f translation = raw_clip.get_transform_translation(transform_index, sample_index);
					const rtm::vector4f scale = raw_clip.get_transform_scale(transform_index, sample_index);
					rtm::qvvf local_transform = rtm::qvv_set(rotation, translation, scale);

					if (has_additive_base)
					{
						const rtm::quatf base_rotation = additive_base_clip.get_transform_rotation(transform_index, base_sample_index);
						const rtm::vector4f base_translation = additive_base_clip.get_transform_translation(transform_index, base_sample_index);
						const rtm::vector4f base_scale = additive_base_clip.get_transform_scale(transform_index, base_sample_index);
						const rtm::qvvf base_transform = rtm::qvv_set(base_rotation, base_translation, base_scale);

						local_transform = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, base_transform, local_transform));
					}

					// Compute our object space transform
					rtm::qvvf object_transform;
					if (parent_index != k_invalid_track_index)
						object_transform = rtm::qvv_normalize(rtm::qvv_mul(local_transform, object_transforms[parent_index]));
					else
						object_transform = local_transform;

					object_transforms[transform_index] = object_transform;
				}

				// Apply the object space scale the shell distance for each transform
				// This will essentially compute the shell distance in object space
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const rtm::qvvf& object_transform = object_transforms[transform_index];

					// Apply the object space scale to our local space shell distance
					const rtm::vector4f abs_scale = rtm::vector_abs(object_transform.scale);
					const rtm::scalarf largest_scale = rtm::scalar_max(rtm::scalar_max(rtm::vector_get_x_as_scalar(abs_scale), rtm::vector_get_y_as_scalar(abs_scale)), rtm::vector_get_z_as_scalar(abs_scale));
					const rtm::scalarf local_shell_distance = rtm::scalar_set(raw_clip.get_transform_shell_distance(transform_index));
					const float object_shell_distance = rtm::scalar_cast(rtm::scalar_mul(largest_scale, local_shell_distance));

					// Compute our transform length in object space
					rtm::vector4f object_parent_position = rtm::vector_zero();
					const uint32_t parent_index = topology->transforms[transform_index].parent_index;
					if (parent_index != k_invalid_track_index)
						object_parent_position = object_transforms[parent_index].translation;

					const float distance_to_parent = rtm::vector_distance3(object_transform.translation, object_parent_position);

					rigid_shell_metadata_t& transform_shell_metadata = shell_metadata[transform_index];
					transform_shell_metadata.local_shell_distance = rtm::scalar_max(object_shell_distance, transform_shell_metadata.local_shell_distance);

					max_distance_to_parents[transform_index] = rtm::scalar_max(distance_to_parent, max_distance_to_parents[transform_index]);
				}
			}

			// Now that we computed the shell distances, we identity which transforms are dominant
			for (const uint32_t transform_index : topology->leaves_first_iterator())
			{
				const uint32_t parent_index = topology->transforms[transform_index].parent_index;
				if (parent_index != k_invalid_track_index)
				{
					// We have a parent, propagate our shell distance if we are a dominant transform
					// We are a dominant transform if our shell distance in parent space is larger
					// than our parent's shell distance in local space. Otherwise, if we are smaller
					// or equal, it means that the full range of motion of our transform fits within
					// the parent's shell distance.

					const rigid_shell_metadata_t& transform_shell = shell_metadata[transform_index];
					const float shell_distance = transform_shell.local_shell_distance;

					rigid_shell_metadata_t& parent_shell = shell_metadata[parent_index];

					const float distance_to_parent = max_distance_to_parents[transform_index];
					const float new_parent_shell_distance = shell_distance + distance_to_parent;
					if (new_parent_shell_distance > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = new_parent_shell_distance;
						parent_shell.precision = transform_shell.precision;
						parent_shell.dominant_transform_index = transform_shell.dominant_transform_index;
					}
				}
			}

			deallocate_type_array(allocator, object_transforms, num_transforms);
			deallocate_type_array(allocator, max_distance_to_parents, num_transforms);

			return shell_metadata;
		}

#if defined(ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS)
		RTM_DISABLE_SECURITY_COOKIE_CHECK inline rtm::quatf RTM_SIMD_CALL quat_add(rtm::quatf_arg0 lhs, rtm::quatf_arg1 rhs) RTM_NO_EXCEPT
		{
#if defined(RTM_SSE2_INTRINSICS)
			return _mm_add_ps(lhs, rhs);
#elif defined(RTM_NEON_INTRINSICS)
			return vaddq_f32(lhs, rhs);
#else
			return rtm::quat_set(rtm::quat_get_x(lhs) + rtm::quat_get_x(rhs), rtm::quat_get_y(lhs) + rtm::quat_get_y(rhs), rtm::quat_get_z(lhs) + rtm::quat_get_z(rhs), rtm::quat_get_w(lhs) + rtm::quat_get_w(rhs));
#endif
		}

		static constexpr rtm::float4f k_dominance_transform_offsets[46][2] =
		{
			// Rotation offset                  , Scale offset

			// Positive x Positive (16)
			{{  0.000F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.00F, 0.0F }},
			{{  0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, {  0.00F,  0.00F,  0.00F, 0.0F }},

			{{  0.000F,  0.000F,  0.000F, 0.0F }, {  0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.001F,  0.000F,  0.000F, 0.0F }, {  0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, {  0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, {  0.01F,  0.00F,  0.00F, 0.0F }},

			{{  0.000F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.01F,  0.00F, 0.0F }},
			{{  0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.01F,  0.00F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, {  0.00F,  0.01F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, {  0.00F,  0.01F,  0.00F, 0.0F }},

			{{  0.000F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.01F, 0.0F }},
			{{  0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.01F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.01F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, {  0.00F,  0.00F,  0.01F, 0.0F }},

			// Positive x Negative (9)
			{{  0.001F,  0.000F,  0.000F, 0.0F }, { -0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, { -0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, { -0.01F,  0.00F,  0.00F, 0.0F }},

			{{  0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F, -0.01F,  0.00F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, {  0.00F, -0.01F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, {  0.00F, -0.01F,  0.00F, 0.0F }},

			{{  0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F, -0.01F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, {  0.00F,  0.00F, -0.01F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, {  0.00F,  0.00F, -0.01F, 0.0F }},

			// Negative x Positive (12)
			{{ -0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, {  0.00F,  0.00F,  0.00F, 0.0F }},

			{{ -0.001F,  0.000F,  0.000F, 0.0F }, {  0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, {  0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, {  0.01F,  0.00F,  0.00F, 0.0F }},

			{{ -0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.01F,  0.00F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, {  0.00F,  0.01F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, {  0.00F,  0.01F,  0.00F, 0.0F }},

			{{ -0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.01F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, {  0.00F,  0.00F,  0.01F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, {  0.00F,  0.00F,  0.01F, 0.0F }},

			// Negative x Negative (9)
			{{ -0.001F,  0.000F,  0.000F, 0.0F }, { -0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, { -0.01F,  0.00F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, { -0.01F,  0.00F,  0.00F, 0.0F }},

			{{ -0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F, -0.01F,  0.00F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, {  0.00F, -0.01F,  0.00F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, {  0.00F, -0.01F,  0.00F, 0.0F }},

			{{ -0.001F,  0.000F,  0.000F, 0.0F }, {  0.00F,  0.00F, -0.01F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, {  0.00F,  0.00F, -0.01F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, {  0.00F,  0.00F, -0.01F, 0.0F }},
		};

		static constexpr rtm::float4f k_dominance_transform_offsets_no_scale[7][2] =
		{
			// Rotation offset                  , Scale offset

			// Positive (4)
			{{  0.000F,  0.000F,  0.000F, 0.0F }, {  0.0F,  0.0F,  0.0F, 0.0F }},
			{{  0.001F,  0.000F,  0.000F, 0.0F }, {  0.0F,  0.0F,  0.0F, 0.0F }},
			{{  0.000F,  0.001F,  0.000F, 0.0F }, {  0.0F,  0.0F,  0.0F, 0.0F }},
			{{  0.000F,  0.000F,  0.001F, 0.0F }, {  0.0F,  0.0F,  0.0F, 0.0F }},

			// Negative (3)
			{{ -0.001F,  0.000F,  0.000F, 0.0F }, {  0.0F,  0.0F,  0.0F, 0.0F }},
			{{  0.000F, -0.001F,  0.000F, 0.0F }, {  0.0F,  0.0F,  0.0F, 0.0F }},
			{{  0.000F,  0.000F, -0.001F, 0.0F }, {  0.0F,  0.0F,  0.0F, 0.0F }},
		};

		template<class clip_adapter_t>
		inline void compute_dominance_map(
			iallocator& allocator,
			const clip_adapter_t& raw_clip,
			const clip_adapter_t& additive_base_clip,
			const itransform_error_metric& error_metric,
			clip_topology_t& clip_topology)
		{
			static_assert(std::is_base_of<transform_clip_adapter_t, clip_adapter_t>::value, "Clip adapter must derive from transform_clip_adapter_t");

			const uint32_t num_transforms = raw_clip.get_num_transforms();
			if (num_transforms == 0)
				return;	// No transforms present, no shell distances

			const uint32_t num_samples = raw_clip.get_num_samples();
			if (num_samples == 0)
				return;	// No samples present, no shell distances

			const bool has_scale = raw_clip.has_scale();
			if (error_metric.needs_conversion(has_scale))
				return;	// We don't support error metrics that require conversion

			const float sample_rate = raw_clip.get_sample_rate();
			const float duration = raw_clip.get_duration();
			const bool has_additive_base = raw_clip.has_additive_base();
			const additive_clip_format8 additive_format = raw_clip.get_additive_format();
			const uint32_t base_num_samples = has_additive_base ? additive_base_clip.get_num_samples() : 0;
			const float base_duration = has_additive_base ? additive_base_clip.get_duration() : 0.0F;

			const rtm::float4f* const dominance_transform_offsets = has_scale ? &k_dominance_transform_offsets[0][0] : &k_dominance_transform_offsets_no_scale[0][0];
			const size_t num_dominance_transform_offsets = has_scale ? get_array_size(k_dominance_transform_offsets) : get_array_size(k_dominance_transform_offsets_no_scale);

			// In order to build the list of dominant descendants for each transform, we first build the inversed map
			// of which transforms we are dominant over. This allows us to reserve a sensible amount of storage.
			// We'll then allocate the real size needed afterwards and reverse the map to store it in the topology metadata.
			const uint32_t num_map_entries_per_transform = clip_topology.max_leaf_depth + 1;	// Add 1 for the count (first entry)
			uint32_t* dominance_reverse_map = allocate_type_array<uint32_t>(allocator, size_t(num_transforms) * num_map_entries_per_transform);
			uint32_t num_aggregate_dominant_descendant_indices = 0;

			std::fill_n(dominance_reverse_map, size_t(num_transforms) * num_map_entries_per_transform, 0);

			rtm::qvvf* local_transforms = allocate_type_array<rtm::qvvf>(allocator, size_t(num_transforms) * 2);
			rtm::qvvf* object_transforms = local_transforms + num_transforms;

			itransform_error_metric::calculate_error_args calculate_error_args;

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				uint32_t base_sample_index = 0;
				if (has_additive_base)
				{
					// The sample time is calculated from the full clip duration to be consistent with decompression
					const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

					const float normalized_sample_time = base_num_samples > 1 ? (sample_time / duration) : 0.0F;
					const float additive_sample_time = base_num_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

					// With uniform sample distributions, we do not interpolate.
					base_sample_index = get_uniform_sample_key(additive_base_clip, transform_segment_adapter_t(), additive_sample_time);
				}

				// Retrieve the local/object space transforms for this sample
				for (const uint32_t transform_index : clip_topology.roots_first_iterator())
				{
					const uint32_t parent_index = clip_topology.transforms[transform_index].parent_index;

					// Sample our local transform
					const rtm::quatf rotation = raw_clip.get_transform_rotation(transform_index, sample_index);
					const rtm::vector4f translation = raw_clip.get_transform_translation(transform_index, sample_index);
					const rtm::vector4f scale = raw_clip.get_transform_scale(transform_index, sample_index);
					rtm::qvvf local_transform = rtm::qvv_set(rotation, translation, scale);

					if (has_additive_base)
					{
						const rtm::quatf base_rotation = additive_base_clip.get_transform_rotation(transform_index, base_sample_index);
						const rtm::vector4f base_translation = additive_base_clip.get_transform_translation(transform_index, base_sample_index);
						const rtm::vector4f base_scale = additive_base_clip.get_transform_scale(transform_index, base_sample_index);
						const rtm::qvvf base_transform = rtm::qvv_set(base_rotation, base_translation, base_scale);

						local_transform = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, base_transform, local_transform));
					}

					local_transforms[transform_index] = local_transform;

					rtm::qvvf object_transform;
					if (parent_index != k_invalid_track_index)
						object_transform = rtm::qvv_normalize(rtm::qvv_mul(local_transform, object_transforms[parent_index]));
					else
						object_transform = local_transform;

					object_transforms[transform_index] = object_transform;
				}

				// For each transform:
				//   Apply a small delta
				//   Measure the resulting error on each descendant
				//   Retain the descendant with the largest error and compute its distance
				for (const uint32_t transform_index : clip_topology.roots_first_iterator())
				{
					const transform_topology_t& transform_topology = clip_topology.transforms[transform_index];
					if (transform_topology.is_leaf())
					{
						continue;	// Skip leaf transforms as they have no descendants and thus no dominant descendants
					}

					const uint32_t parent_index = transform_topology.parent_index;

					const rtm::float4f* current_dominance_transform_offsets = dominance_transform_offsets;
					for (size_t dominance_transform_offset_index = 0; dominance_transform_offset_index < num_dominance_transform_offsets; ++dominance_transform_offset_index)
					{
						const rtm::quatf rotation_offset = rtm::quat_load(current_dominance_transform_offsets++);
						const rtm::vector4f scale_offset = rtm::vector_load(current_dominance_transform_offsets++);

						rtm::qvvf local_transform = local_transforms[transform_index];

						// Apply our delta offset
						local_transform.rotation = rtm::quat_normalize(quat_add(local_transform.rotation, rotation_offset));
						local_transform.scale = rtm::vector_add(local_transform.scale, scale_offset);

						rtm::scalarf worst_error_sq = rtm::scalar_set(-1.0E10F);
						uint32_t worst_descendant_transform_index = k_invalid_track_index;

						for (const uint32_t descendant_transform_index : transform_topology.descendants_iterator())
						{
							// Compute our descendant transform in object space taking into account our local change
							rtm::qvvf descendant_object_transform = local_transforms[descendant_transform_index];

							// Accumulate transforms up to our current test transform
							uint32_t cursor_index = clip_topology.transforms[descendant_transform_index].parent_index;
							while (cursor_index != transform_index)
							{
								descendant_object_transform = rtm::qvv_normalize(rtm::qvv_mul(descendant_object_transform, local_transforms[cursor_index]));

								cursor_index = clip_topology.transforms[cursor_index].parent_index;
							}

							// Accumulate the test transform
							descendant_object_transform = rtm::qvv_normalize(rtm::qvv_mul(descendant_object_transform, local_transform));

							// Accumulate the rest of the chain up to the root
							if (parent_index != k_invalid_track_index)
								descendant_object_transform = rtm::qvv_normalize(rtm::qvv_mul(descendant_object_transform, object_transforms[parent_index]));

							// The original descendant object transform
							const rtm::qvvf& reference_descendant_object_transform = object_transforms[descendant_transform_index];

							// Measure the error
							const float shell_distance = raw_clip.get_transform_shell_distance(descendant_transform_index);

							calculate_error_args.construct_sphere_shell(shell_distance);
							calculate_error_args.transform0 = &reference_descendant_object_transform;
							calculate_error_args.transform1 = &descendant_object_transform;

							const rtm::scalarf error_sq = error_metric.calculate_error_squared(calculate_error_args);
							if (rtm::scalar_greater_than(error_sq, worst_error_sq))
							{
								worst_error_sq = error_sq;
								worst_descendant_transform_index = descendant_transform_index;
							}
						}

						ACL_ASSERT(worst_descendant_transform_index != k_invalid_track_index, "Failed to find a dominant descendant transform");

						uint32_t* num_descendant_dominant_transforms = dominance_reverse_map + (worst_descendant_transform_index * num_map_entries_per_transform);
						uint32_t* descendant_dominant_transforms = num_descendant_dominant_transforms + 1;

						// We've found the descendant most impacted by the transform delta, as such, it is dominant
						// Dominance is transitive, make sure each parent along the chain also sees us as dominant
						uint32_t cursor_index = transform_index;
						while (cursor_index != k_invalid_track_index)
						{
							// If we've seen this dominant transform before, then every parent also contains it, we can stop iterating
							if (std::any_of(
								descendant_dominant_transforms,
								descendant_dominant_transforms + *num_descendant_dominant_transforms,
								[cursor_index](uint32_t value) { return value == cursor_index; }))
								break;

							// We are unique, append it
							descendant_dominant_transforms[*num_descendant_dominant_transforms] = cursor_index;
							*num_descendant_dominant_transforms += 1;
							num_aggregate_dominant_descendant_indices++;

							cursor_index = clip_topology.transforms[cursor_index].parent_index;
						}
					}
				}
			}

			// Now that we've found the dominant descendants, build our final mapping
			// Allocate the list of dominant descendant indices and partition it among the transforms
			uint32_t* aggregate_dominant_descendant_indices = allocate_type_array<uint32_t>(allocator, num_aggregate_dominant_descendant_indices);
			uint32_t num_assigned_dominant_descendant_indices = 0;

			// We iterate leaves first to propagate our count from the reverse map
			for (const uint32_t transform_index : clip_topology.leaves_first_iterator())
			{
				const uint32_t* num_descendant_dominant_transforms = dominance_reverse_map + (transform_index * num_map_entries_per_transform);
				const uint32_t* descendant_dominant_transforms = num_descendant_dominant_transforms + 1;

				// Propagate our count so we can properly partition the aggregate indices map
				for (uint32_t cursor_index = 0; cursor_index < *num_descendant_dominant_transforms; ++cursor_index)
				{
					const uint32_t parent_dominant_index = descendant_dominant_transforms[cursor_index];

					transform_topology_t& parent_dominant_topology = clip_topology.transforms[parent_dominant_index];
					parent_dominant_topology.num_dominant_descendents++;
				}

				// Assign our indices
				transform_topology_t& transform_topology = clip_topology.transforms[transform_index];
				transform_topology.dominant_descendants = aggregate_dominant_descendant_indices + num_assigned_dominant_descendant_indices;
				num_assigned_dominant_descendant_indices += transform_topology.num_dominant_descendents;
			}

			// Propagate and reverse our dominance map
			for (const uint32_t transform_index : clip_topology.roots_first_iterator())
			{
				transform_topology_t& transform_topology = clip_topology.transforms[transform_index];

				// Reset our count so we can properly append them when visiting our descendants
				transform_topology.num_dominant_descendents = 0;

				const uint32_t* num_descendant_dominant_transforms = dominance_reverse_map + (transform_index * num_map_entries_per_transform);
				const uint32_t* descendant_dominant_transforms = num_descendant_dominant_transforms + 1;

				// Number of indices to propagate
				const uint32_t num_dominant_descendents = *num_descendant_dominant_transforms;

				// Propagate ourself as dominant
				for (uint32_t cursor_index = 0; cursor_index < num_dominant_descendents; ++cursor_index)
				{
					const uint32_t parent_dominant_index = descendant_dominant_transforms[cursor_index];

					transform_topology_t& parent_dominant_topology = clip_topology.transforms[parent_dominant_index];

					const std::ptrdiff_t indices_offset = parent_dominant_topology.dominant_descendants - aggregate_dominant_descendant_indices;
					uint32_t* cursor_dominant_descendants = aggregate_dominant_descendant_indices + indices_offset;

					cursor_dominant_descendants[parent_dominant_topology.num_dominant_descendents] = transform_index;
					parent_dominant_topology.num_dominant_descendents++;
				}
			}

			clip_topology.aggregate_dominant_descendant_indices = aggregate_dominant_descendant_indices;
			clip_topology.num_aggregate_dominant_descendant_indices = num_aggregate_dominant_descendant_indices;

			deallocate_type_array(allocator, local_transforms, size_t(num_transforms) * 2);
			deallocate_type_array(allocator, dominance_reverse_map, size_t(num_transforms) * num_map_entries_per_transform);
		}
#endif

		// We use the raw data to compute the rigid shell
		// For each transform, its rigid shell is formed by the dominant joint (itself or a child)
		// We compute the largest value over the whole segment per transform
		inline void compute_segment_shell_distances(const segment_context& segment, const clip_context& additive_base_clip_context, rigid_shell_metadata_t* out_shell_metadata)
		{
			const uint32_t num_transforms = segment.num_bones;
			if (num_transforms == 0)
				return;	// No transforms present, no shell distances

			const uint32_t num_samples = segment.num_samples;
			if (num_samples == 0)
				return;	// No samples present, no shell distances

			const clip_context& owner_clip_context = *segment.clip;
			iallocator& allocator = *owner_clip_context.allocator;
			const float sample_rate = owner_clip_context.sample_rate;
			const float duration = owner_clip_context.duration;
			const bool has_additive_base = owner_clip_context.has_additive_base;
			const additive_clip_format8 additive_format = owner_clip_context.additive_format;
			const uint32_t base_num_samples = has_additive_base ? additive_base_clip_context.num_samples : 0;
			const float base_duration = has_additive_base ? additive_base_clip_context.duration : 0.0F;
			const clip_topology_t* topology = owner_clip_context.topology;
			const bool has_scale = owner_clip_context.has_scale;

			// We retain only one dominant sub-transform per transform but in reality, it could change from keyframe to keyframe
			// To that end, we compute the local shell distance in object space for each transform at every sample and we
			// retain the largest value. We then use this maximum value to compute our dominance.

			rtm::qvvf* object_transforms = allocate_type_array<rtm::qvvf>(allocator, num_transforms);
			float* max_distance_to_parents = allocate_type_array<float>(allocator, num_transforms);

			std::memset(max_distance_to_parents, 0, sizeof(float) * num_transforms);

			// Initialize our output shell metadata
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const transform_metadata& metadata = owner_clip_context.metadata[transform_index];
				rigid_shell_metadata_t& shell_metadata = out_shell_metadata[transform_index];

				shell_metadata.local_shell_distance = metadata.shell_distance;
				shell_metadata.precision = metadata.precision;
				shell_metadata.dominant_transform_index = transform_index;
			}

			for (uint32_t segment_sample_index = 0; segment_sample_index < num_samples; ++segment_sample_index)
			{
				uint32_t base_sample_index = 0;

				if (has_additive_base)
				{
					const segment_context& base_segment = additive_base_clip_context.segments[0];
					const uint32_t clip_sample_index = segment.clip_sample_offset + segment_sample_index;

					// The sample time is calculated from the full clip duration to be consistent with decompression
					const float sample_time = rtm::scalar_min(float(clip_sample_index) / sample_rate, duration);

					const float normalized_sample_time = base_num_samples > 1 ? (sample_time / duration) : 0.0F;
					const float additive_sample_time = base_num_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

					// With uniform sample distributions, we do not interpolate.
					base_sample_index = get_uniform_sample_key(base_segment, additive_sample_time);
				}

				sample_context context;
				context.sample_key = segment_sample_index;

				// Retrieve the object space transforms for this sample
				for (const uint32_t transform_index : topology->roots_first_iterator())
				{
					const uint32_t parent_index = topology->transforms[transform_index].parent_index;

					// Sample our local transform
					const transform_streams& sampling_bone_stream = segment.bone_streams[transform_index];

					const rtm::quatf rotation = acl_impl::sample_rotation(context, sampling_bone_stream);
					const rtm::vector4f translation = acl_impl::sample_translation(context, sampling_bone_stream);
					const rtm::vector4f scale = has_scale ? acl_impl::sample_scale(context, sampling_bone_stream) : sampling_bone_stream.default_value.scale;
					rtm::qvvf local_transform = rtm::qvv_set(rotation, translation, scale);

					if (has_additive_base)
					{
						const segment_context& base_segment = additive_base_clip_context.segments[0];
						const transform_streams& base_bone_stream = base_segment.bone_streams[transform_index];

						const rtm::quatf base_rotation = base_bone_stream.rotations.get_sample_clamped(base_sample_index);
						const rtm::vector4f base_translation = base_bone_stream.translations.get_sample_clamped(base_sample_index);
						const rtm::vector4f base_scale = base_bone_stream.scales.get_sample_clamped(base_sample_index);
						const rtm::qvvf base_transform = rtm::qvv_set(base_rotation, base_translation, base_scale);

						local_transform = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, base_transform, local_transform));
					}

					// Compute our object space transform
					rtm::qvvf object_transform;
					if (parent_index != k_invalid_track_index)
						object_transform = rtm::qvv_normalize(rtm::qvv_mul(local_transform, object_transforms[parent_index]));
					else
						object_transform = local_transform;

					object_transforms[transform_index] = object_transform;
				}

				// Apply the object space scale the shell distance for each transform
				// This will essentially compute the shell distance in object space
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const rtm::qvvf& object_transform = object_transforms[transform_index];

					// Apply the object space scale to our local space shell distance
					const rtm::vector4f abs_scale = rtm::vector_abs(object_transform.scale);
					const rtm::scalarf largest_scale = rtm::scalar_max(rtm::scalar_max(rtm::vector_get_x_as_scalar(abs_scale), rtm::vector_get_y_as_scalar(abs_scale)), rtm::vector_get_z_as_scalar(abs_scale));
					const rtm::scalarf local_shell_distance = rtm::scalar_set(owner_clip_context.metadata[transform_index].shell_distance);
					const float object_shell_distance = rtm::scalar_cast(rtm::scalar_mul(largest_scale, local_shell_distance));

					// Compute our transform length in object space
					rtm::vector4f object_parent_position = rtm::vector_zero();
					const uint32_t parent_index = topology->transforms[transform_index].parent_index;
					if (parent_index != k_invalid_track_index)
						object_parent_position = object_transforms[parent_index].translation;

					const float distance_to_parent = rtm::vector_distance3(object_transform.translation, object_parent_position);

					rigid_shell_metadata_t& transform_shell_metadata = out_shell_metadata[transform_index];
					transform_shell_metadata.local_shell_distance = rtm::scalar_max(object_shell_distance, transform_shell_metadata.local_shell_distance);

					max_distance_to_parents[transform_index] = rtm::scalar_max(distance_to_parent, max_distance_to_parents[transform_index]);
				}
			}

			// Now that we computed the shell distances, we identity which transforms are dominant
			for (const uint32_t transform_index : topology->leaves_first_iterator())
			{
				const uint32_t parent_index = topology->transforms[transform_index].parent_index;
				if (parent_index != k_invalid_track_index)
				{
					// We have a parent, propagate our shell distance if we are a dominant transform
					// We are a dominant transform if our shell distance in parent space is larger
					// than our parent's shell distance in local space. Otherwise, if we are smaller
					// or equal, it means that the full range of motion of our transform fits within
					// the parent's shell distance.

					const rigid_shell_metadata_t& transform_shell = out_shell_metadata[transform_index];
					const float shell_distance = transform_shell.local_shell_distance;

					rigid_shell_metadata_t& parent_shell = out_shell_metadata[parent_index];

					const float distance_to_parent = max_distance_to_parents[transform_index];
					const float new_parent_shell_distance = shell_distance + distance_to_parent;
					if (new_parent_shell_distance > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = new_parent_shell_distance;
						parent_shell.precision = transform_shell.precision;
						parent_shell.dominant_transform_index = transform_shell.dominant_transform_index;
					}
				}
			}

			deallocate_type_array(allocator, object_transforms, num_transforms);
			deallocate_type_array(allocator, max_distance_to_parents, num_transforms);
		}

		// We use the provided object space transforms to compute the rigid shell
		// For each transform, its rigid shell is formed by the dominant joint (itself or a child)
		// We compute the largest value over the whole segment per transform
		inline void compute_segment_shell_distances(const segment_context& segment, const rtm::qvvf* object_transforms, rigid_shell_metadata_t* out_shell_metadata)
		{
			const uint32_t num_transforms = segment.num_bones;
			if (num_transforms == 0)
				return;	// No transforms present, no shell distances

			const uint32_t num_samples = segment.num_samples;
			if (num_samples == 0)
				return;	// No samples present, no shell distances

			const clip_context& owner_clip_context = *segment.clip;
			const clip_topology_t* topology = owner_clip_context.topology;

			float* max_distance_to_parents = allocate_type_array<float>(*segment.clip->allocator, num_transforms);
			std::memset(max_distance_to_parents, 0, sizeof(float) * num_transforms);

			// Initialize our output shell metadata
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const transform_metadata& metadata = owner_clip_context.metadata[transform_index];
				rigid_shell_metadata_t& shell_metadata = out_shell_metadata[transform_index];

				shell_metadata.local_shell_distance = metadata.shell_distance;
				shell_metadata.precision = metadata.precision;
				shell_metadata.dominant_transform_index = transform_index;
			}

			for (uint32_t segment_sample_index = 0; segment_sample_index < num_samples; ++segment_sample_index)
			{
				const rtm::qvvf* object_pose_transforms = object_transforms + (segment_sample_index * num_transforms);

				// Apply the object space scale the shell distance for each transform
				// This will essentially compute the shell distance in object space
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const rtm::qvvf& object_transform = object_pose_transforms[transform_index];

					// Apply the object space scale to our local space shell distance
					const rtm::vector4f abs_scale = rtm::vector_abs(object_transform.scale);
					const rtm::scalarf largest_scale = rtm::scalar_max(rtm::scalar_max(rtm::vector_get_x_as_scalar(abs_scale), rtm::vector_get_y_as_scalar(abs_scale)), rtm::vector_get_z_as_scalar(abs_scale));
					const rtm::scalarf local_shell_distance = rtm::scalar_set(owner_clip_context.metadata[transform_index].shell_distance);
					const float object_shell_distance = rtm::scalar_cast(rtm::scalar_mul(largest_scale, local_shell_distance));

					// Compute our transform length in object space
					rtm::vector4f object_parent_position = rtm::vector_zero();
					const uint32_t parent_index = topology->transforms[transform_index].parent_index;
					if (parent_index != k_invalid_track_index)
						object_parent_position = object_pose_transforms[parent_index].translation;

					const float distance_to_parent = rtm::vector_distance3(object_transform.translation, object_parent_position);

					rigid_shell_metadata_t& transform_shell_metadata = out_shell_metadata[transform_index];
					transform_shell_metadata.local_shell_distance = rtm::scalar_max(object_shell_distance, transform_shell_metadata.local_shell_distance);

					max_distance_to_parents[transform_index] = rtm::scalar_max(distance_to_parent, max_distance_to_parents[transform_index]);
				}
			}

			// Now that we computed the shell distances, we identity which transforms are dominant
			for (const uint32_t transform_index : topology->leaves_first_iterator())
			{
				const uint32_t parent_index = topology->transforms[transform_index].parent_index;
				if (parent_index != k_invalid_track_index)
				{
					// We have a parent, propagate our shell distance if we are a dominant transform
					// We are a dominant transform if our shell distance in parent space is larger
					// than our parent's shell distance in local space. Otherwise, if we are smaller
					// or equal, it means that the full range of motion of our transform fits within
					// the parent's shell distance.

					const rigid_shell_metadata_t& transform_shell = out_shell_metadata[transform_index];
					const float shell_distance = transform_shell.local_shell_distance;

					rigid_shell_metadata_t& parent_shell = out_shell_metadata[parent_index];

					const float distance_to_parent = max_distance_to_parents[transform_index];
					const float new_parent_shell_distance = shell_distance + distance_to_parent;
					if (new_parent_shell_distance > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = new_parent_shell_distance;
						parent_shell.precision = transform_shell.precision;
						parent_shell.dominant_transform_index = transform_shell.dominant_transform_index;
					}
				}
			}

			deallocate_type_array(*segment.clip->allocator, max_distance_to_parents, num_transforms);
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
