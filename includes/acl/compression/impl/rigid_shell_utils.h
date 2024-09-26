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

			const bool has_additive_base = raw_clip.has_additive_base();
			const additive_clip_format8 additive_format = raw_clip.get_additive_format();
			const clip_topology_t* topology = raw_clip.get_topology();

			// We retain only one dominant sub-transform per transform but in reality, it could change from keyframe to keyframe
			// To keep things simple, we use the first keyframe to compute dominance
			const uint32_t sample_index = 0;
			const uint32_t base_sample_index = 0;

			rtm::qvvf* object_transforms = allocate_type_array<rtm::qvvf>(allocator, num_transforms);

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

			// Now that we computed the object space transforms for this sample,
			// we identity which transforms are dominant
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

					// Compute our transform length in object space
					const rtm::qvvf& object_transform = object_transforms[transform_index];
					rtm::vector4f object_parent_position = rtm::vector_zero();
					if (parent_index != k_invalid_track_index)
						object_parent_position = object_transforms[parent_index].translation;

					const rtm::scalarf local_shell_distance = rtm::scalar_set(transform_shell.local_shell_distance);

					const rtm::vector4f abs_scale = rtm::vector_abs(object_transform.scale);
					const rtm::scalarf largest_scale = rtm::scalar_max(rtm::scalar_max(rtm::vector_get_x_as_scalar(abs_scale), rtm::vector_get_y_as_scalar(abs_scale)), rtm::vector_get_z_as_scalar(abs_scale));
					const rtm::scalarf furthest_shell_point = rtm::scalar_mul(largest_scale, local_shell_distance);

					const rtm::scalarf shell_distance = rtm::scalar_add(furthest_shell_point, rtm::vector_distance3_as_scalar(object_transform.translation, object_parent_position));
					const float shell_distance_f = rtm::scalar_cast(shell_distance);

					rigid_shell_metadata_t& parent_shell = shell_metadata[parent_index];

					if (shell_distance_f > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = shell_distance_f;
						parent_shell.precision = transform_shell.precision;
						parent_shell.dominant_transform_index = transform_shell.dominant_transform_index;
					}
				}
			}

			deallocate_type_array(allocator, object_transforms, num_transforms);

			return shell_metadata;
		}

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
			const bool has_additive_base = owner_clip_context.has_additive_base;
			const additive_clip_format8 additive_format = owner_clip_context.additive_format;
			const clip_topology_t* topology = owner_clip_context.topology;
			const bool has_scale = owner_clip_context.has_scale;

			// We retain only one dominant sub-transform per transform but in reality, it could change from keyframe to keyframe
			// To keep things simple, we use the first keyframe to compute dominance
			const uint32_t segment_sample_index = 0;
			uint32_t base_sample_index = 0;

			if (has_additive_base)
			{
				const float sample_rate = owner_clip_context.sample_rate;
				const float duration = owner_clip_context.duration;

				const uint32_t base_num_samples = additive_base_clip_context.num_samples;
				const float base_duration = additive_base_clip_context.duration;

				const segment_context& base_segment = additive_base_clip_context.segments[0];
				const uint32_t clip_sample_index = segment.clip_sample_offset + segment_sample_index;

				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(float(clip_sample_index) / sample_rate, duration);

				const float normalized_sample_time = base_num_samples > 1 ? (sample_time / duration) : 0.0F;
				const float additive_sample_time = base_num_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

				// With uniform sample distributions, we do not interpolate.
				base_sample_index = get_uniform_sample_key(base_segment, additive_sample_time);
			}

			rtm::qvvf* object_transforms = allocate_type_array<rtm::qvvf>(allocator, num_transforms);

			// Initialize our output shell metadata
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const transform_metadata& metadata = owner_clip_context.metadata[transform_index];
				rigid_shell_metadata_t& shell_metadata = out_shell_metadata[transform_index];

				shell_metadata.local_shell_distance = metadata.shell_distance;
				shell_metadata.precision = metadata.precision;
				shell_metadata.dominant_transform_index = transform_index;
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

			// Now that we computed the object space transforms for this sample,
			// we identity which transforms are dominant
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

					// Compute our transform length in object space
					const rtm::qvvf& object_transform = object_transforms[transform_index];
					rtm::vector4f object_parent_position = rtm::vector_zero();
					if (parent_index != k_invalid_track_index)
						object_parent_position = object_transforms[parent_index].translation;

					const rtm::scalarf local_shell_distance = rtm::scalar_set(transform_shell.local_shell_distance);

					const rtm::vector4f abs_scale = rtm::vector_abs(object_transform.scale);
					const rtm::scalarf largest_scale = rtm::scalar_max(rtm::scalar_max(rtm::vector_get_x_as_scalar(abs_scale), rtm::vector_get_y_as_scalar(abs_scale)), rtm::vector_get_z_as_scalar(abs_scale));
					const rtm::scalarf furthest_shell_point = rtm::scalar_mul(largest_scale, local_shell_distance);

					const rtm::scalarf shell_distance = rtm::scalar_add(furthest_shell_point, rtm::vector_distance3_as_scalar(object_transform.translation, object_parent_position));
					const float shell_distance_f = rtm::scalar_cast(shell_distance);

					rigid_shell_metadata_t& parent_shell = out_shell_metadata[parent_index];

					if (shell_distance_f > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = shell_distance_f;
						parent_shell.precision = transform_shell.precision;
						parent_shell.dominant_transform_index = transform_shell.dominant_transform_index;
					}
				}
			}

			deallocate_type_array(allocator, object_transforms, num_transforms);
		}

		// We use the provided object space transforms to compute the rigid shell
		// For each transform, its rigid shell is formed by the dominant joint (itself or a child)
		// We compute the largest value over the whole segment per transform
		inline void compute_segment_shell_distances(const segment_context& segment, const rtm::qvvf* object_transforms, rigid_shell_metadata_t* out_shell_metadata)
		{
			const uint32_t num_transforms = segment.num_bones;
			if (num_transforms == 0)
				return;	// No transforms present, no shell distances

			if (segment.num_samples == 0)
				return;	// No samples present, no shell distances

			const clip_context& owner_clip_context = *segment.clip;
			const clip_topology_t* topology = owner_clip_context.topology;

			// Initialize our output shell metadata
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const transform_metadata& metadata = owner_clip_context.metadata[transform_index];
				rigid_shell_metadata_t& shell_metadata = out_shell_metadata[transform_index];

				shell_metadata.local_shell_distance = metadata.shell_distance;
				shell_metadata.precision = metadata.precision;
				shell_metadata.dominant_transform_index = transform_index;
			}

			// Now that we computed the object space transforms for this sample,
			// we identity which transforms are dominant
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

					// Compute our transform length in object space
					const rtm::qvvf& object_transform = object_transforms[transform_index];
					rtm::vector4f object_parent_position = rtm::vector_zero();
					if (parent_index != k_invalid_track_index)
						object_parent_position = object_transforms[parent_index].translation;

					const rtm::scalarf local_shell_distance = rtm::scalar_set(transform_shell.local_shell_distance);

					const rtm::vector4f abs_scale = rtm::vector_abs(object_transform.scale);
					const rtm::scalarf largest_scale = rtm::scalar_max(rtm::scalar_max(rtm::vector_get_x_as_scalar(abs_scale), rtm::vector_get_y_as_scalar(abs_scale)), rtm::vector_get_z_as_scalar(abs_scale));
					const rtm::scalarf furthest_shell_point = rtm::scalar_mul(largest_scale, local_shell_distance);

					const rtm::scalarf shell_distance = rtm::scalar_add(furthest_shell_point, rtm::vector_distance3_as_scalar(object_transform.translation, object_parent_position));
					const float shell_distance_f = rtm::scalar_cast(shell_distance);

					rigid_shell_metadata_t& parent_shell = out_shell_metadata[parent_index];

					if (shell_distance_f > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = shell_distance_f;
						parent_shell.precision = transform_shell.precision;
						parent_shell.dominant_transform_index = transform_shell.dominant_transform_index;
					}
				}
			}
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
