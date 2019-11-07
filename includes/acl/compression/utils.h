#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/compiler_utils.h"
#include "acl/core/compressed_clip.h"
#include "acl/core/iallocator.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/decompression/default_output_writer.h"

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	struct BoneError
	{
		BoneError() : index(k_invalid_bone_index), error(0.0F), sample_time(0.0F) {}

		uint16_t index;
		float error;
		float sample_time;
	};

	template<class DecompressionContextType>
	inline BoneError calculate_compressed_clip_error(IAllocator& allocator,
		const AnimationClip& clip, const ISkeletalErrorMetric& error_metric, DecompressionContextType& context)
	{
		const uint16_t num_bones = clip.get_num_bones();
		const float clip_duration = clip.get_duration();
		const float sample_rate = clip.get_sample_rate();
		const uint32_t num_samples = clip.get_num_samples();
		const RigidSkeleton& skeleton = clip.get_skeleton();

		uint16_t num_output_bones = 0;
		uint16_t* output_bone_mapping = create_output_bone_mapping(allocator, clip, num_output_bones);

		const AnimationClip* additive_base_clip = clip.get_additive_base();
		const uint32_t additive_num_samples = additive_base_clip != nullptr ? additive_base_clip->get_num_samples() : 0;
		const float additive_duration = additive_base_clip != nullptr ? additive_base_clip->get_duration() : 0.0F;

		Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
		Transform_32* base_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
		Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_output_bones);
		Transform_32* lossy_remapped_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);

		BoneError bone_error;
		DefaultOutputWriter pose_writer(lossy_pose_transforms, num_output_bones);

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const float sample_time = min(float(sample_index) / sample_rate, clip_duration);

			// We use the nearest sample to accurately measure the loss that happened, if any
			clip.sample_pose(sample_time, SampleRoundingPolicy::Nearest, raw_pose_transforms, num_bones);

			context.seek(sample_time, SampleRoundingPolicy::Nearest);
			context.decompress_pose(pose_writer);

			if (additive_base_clip != nullptr)
			{
				const float normalized_sample_time = additive_num_samples > 1 ? (sample_time / clip_duration) : 0.0F;
				const float additive_sample_time = normalized_sample_time * additive_duration;
				additive_base_clip->sample_pose(additive_sample_time, SampleRoundingPolicy::Nearest, base_pose_transforms, num_bones);
			}

			// Perform remapping by copying the raw pose first and we overwrite with the decompressed pose if
			// the data is available
			std::memcpy(lossy_remapped_pose_transforms, raw_pose_transforms, sizeof(Transform_32) * num_bones);
			for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				const uint16_t bone_index = output_bone_mapping[output_index];
				lossy_remapped_pose_transforms[bone_index] = lossy_pose_transforms[output_index];
			}

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				// Always calculate the error with scale, slower but binary exact
				const float error = error_metric.calculate_object_bone_error(skeleton, raw_pose_transforms, base_pose_transforms, lossy_remapped_pose_transforms, bone_index);

				if (error > bone_error.error)
				{
					bone_error.error = error;
					bone_error.index = bone_index;
					bone_error.sample_time = sample_time;
				}
			}
		}

		deallocate_type_array(allocator, output_bone_mapping, num_output_bones);
		deallocate_type_array(allocator, raw_pose_transforms, num_bones);
		deallocate_type_array(allocator, base_pose_transforms, num_bones);
		deallocate_type_array(allocator, lossy_pose_transforms, num_output_bones);
		deallocate_type_array(allocator, lossy_remapped_pose_transforms, num_bones);

		return bone_error;
	}

	template<class DecompressionContextType>
	ACL_DEPRECATED("Use a calculate_compressed_clip_error(..) with an explicit error metric instead, to be removed in v2.0")
	inline BoneError calculate_compressed_clip_error(IAllocator& allocator,
		const AnimationClip& clip, const CompressionSettings& settings, DecompressionContextType& context)
	{
		ACL_ASSERT(settings.error_metric != nullptr, "Error metric cannot be null");
		return calculate_compressed_clip_error(allocator, clip, *settings.error_metric, context);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
