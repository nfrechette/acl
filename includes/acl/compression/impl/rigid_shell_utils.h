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

#include <rtm/qvvf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// Rigid shell information per transform
		struct rigid_shell_metadata_t
		{
			// Dominant local space shell distance (from transform tip)
			float local_shell_distance;

			// Parent space shell distance (from transform root)
			float parent_shell_distance;

			// Precision required on the surface of the rigid shell
			float precision;
		};

		// We use the raw data to compute the rigid shell since rotations might have been converted already
		// We compute the largest value over the whole clip per transform
		// TODO: We could compute a single value per sample and per transform as well but its unclear if this would provide an advantage
		inline rigid_shell_metadata_t* compute_clip_shell_distances(iallocator& allocator, const clip_context& raw_clip_context, const clip_context& additive_base_clip_context)
		{
			const uint32_t num_transforms = raw_clip_context.num_bones;
			if (num_transforms == 0)
				return nullptr;	// No transforms present, no shell distances

			const uint32_t num_samples = raw_clip_context.num_samples;
			if (num_samples == 0)
				return nullptr;	// No samples present, no shell distances

			const segment_context& raw_segment = raw_clip_context.segments[0];
			const bool has_additive_base = raw_clip_context.has_additive_base;

			rigid_shell_metadata_t* shell_metadata = allocate_type_array<rigid_shell_metadata_t>(allocator, num_transforms);

			// Initialize everything
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const transform_metadata& metadata = raw_clip_context.metadata[transform_index];

				shell_metadata[transform_index].local_shell_distance = metadata.shell_distance;
				shell_metadata[transform_index].precision = metadata.precision;
				shell_metadata[transform_index].parent_shell_distance = 0.0F;
			}

			// Iterate from leaf transforms towards their root, we want to bubble up our shell distance
			for (const uint32_t transform_index : make_reverse_iterator(raw_clip_context.sorted_transforms_parent_first, num_transforms))
			{
				const transform_streams& raw_bone_stream = raw_segment.bone_streams[transform_index];

				rigid_shell_metadata_t& shell = shell_metadata[transform_index];

				// Use the accumulated shell distance so far to see how far it deforms with our local transform
				const rtm::vector4f vtx0 = rtm::vector_set(shell.local_shell_distance, 0.0F, 0.0F);
				const rtm::vector4f vtx1 = rtm::vector_set(0.0F, shell.local_shell_distance, 0.0F);
				const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, shell.local_shell_distance);

				// Calculate the shell distance in parent space
				rtm::scalarf parent_shell_distance = rtm::scalar_set(0.0F);
				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					const rtm::quatf raw_rotation = raw_bone_stream.rotations.get_sample(sample_index);
					const rtm::vector4f raw_translation = raw_bone_stream.translations.get_sample(sample_index);
					const rtm::vector4f raw_scale = raw_bone_stream.scales.get_sample(sample_index);

					rtm::qvvf raw_transform = rtm::qvv_set(raw_rotation, raw_translation, raw_scale);

					if (has_additive_base)
					{
						// If we are additive, we must apply our local transform on the base to figure out
						// the true shell distance
						const segment_context& base_segment = additive_base_clip_context.segments[0];
						const transform_streams& base_bone_stream = base_segment.bone_streams[transform_index];

						// The sample time is calculated from the full clip duration to be consistent with decompression
						const float sample_time = rtm::scalar_min(float(sample_index) / raw_clip_context.sample_rate, raw_clip_context.duration);

						const float normalized_sample_time = base_segment.num_samples > 1 ? (sample_time / raw_clip_context.duration) : 0.0F;
						const float additive_sample_time = base_segment.num_samples > 1 ? (normalized_sample_time * additive_base_clip_context.duration) : 0.0F;

						// With uniform sample distributions, we do not interpolate.
						const uint32_t base_sample_index = get_uniform_sample_key(base_segment, additive_sample_time);

						const rtm::quatf base_rotation = base_bone_stream.rotations.get_sample(base_sample_index);
						const rtm::vector4f base_translation = base_bone_stream.translations.get_sample(base_sample_index);
						const rtm::vector4f base_scale = base_bone_stream.scales.get_sample(base_sample_index);

						const rtm::qvvf base_transform = rtm::qvv_set(base_rotation, base_translation, base_scale);
						raw_transform = apply_additive_to_base(raw_clip_context.additive_format, base_transform, raw_transform);
					}

					const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_transform);
					const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_transform);
					const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_transform);

					const rtm::scalarf vtx0_distance = rtm::vector_length3(raw_vtx0);
					const rtm::scalarf vtx1_distance = rtm::vector_length3(raw_vtx1);
					const rtm::scalarf vtx2_distance = rtm::vector_length3(raw_vtx2);

					const rtm::scalarf transform_length = rtm::scalar_max(rtm::scalar_max(vtx0_distance, vtx1_distance), vtx2_distance);
					parent_shell_distance = rtm::scalar_max(parent_shell_distance, transform_length);
				}

				shell.parent_shell_distance = rtm::scalar_cast(parent_shell_distance);

				const transform_metadata& metadata = raw_clip_context.metadata[transform_index];

				// Add precision since we want to make sure to encompass the maximum amount of error allowed
				// Add it only for non-dominant transforms to account for the error they introduce
				// Dominant transforms will use their own precision
				// If our shell distance has changed, we are non-dominant since a dominant child updated it
				if (shell.local_shell_distance != metadata.shell_distance)
					shell.parent_shell_distance += metadata.precision;

				if (metadata.parent_index != k_invalid_track_index)
				{
					// We have a parent, propagate our shell distance if we are a dominant transform
					// We are a dominant transform if our shell distance in parent space is larger
					// than our parent's shell distance in local space. Otherwise, if we are smaller
					// or equal, it means that the full range of motion of our transform fits within
					// the parent's shell distance.

					rigid_shell_metadata_t& parent_shell = shell_metadata[metadata.parent_index];

					if (shell.parent_shell_distance > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = shell.parent_shell_distance;
						parent_shell.precision = shell.precision;
					}
				}
			}

			return shell_metadata;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
