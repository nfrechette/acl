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

#include "acl/core/compressed_clip.h"
#include "acl/core/ialgorithm.h"
#include "acl/core/memory.h"
#include "acl/math/affine_matrix_32.h"
#include "acl/math/transform_32.h"
#include "acl/math/scalar_32.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/decompression_functions.h"

namespace acl
{
	// TODO: Add a context object to avoid malloc/free of the buffers with every call of the function
	//       or manage the pose buffers externally?
	inline float calculate_skeleton_error(Allocator& allocator, const RigidSkeleton& skeleton, const Transform_32* raw_local_pose, const Transform_32* lossy_local_pose, float* out_error_per_bone = nullptr)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);

		Transform_32* raw_object_pose = allocate_type_array<Transform_32>(allocator, num_bones);
		Transform_32* lossy_object_pose = allocate_type_array<Transform_32>(allocator, num_bones);

		local_to_object_space(skeleton, raw_local_pose, raw_object_pose);
		local_to_object_space(skeleton, lossy_local_pose, lossy_object_pose);

		Vector4_32 x_axis = vector_set(1.0f, 0.0f, 0.0f);
		Vector4_32 y_axis = vector_set(0.0f, 1.0f, 0.0f);

		float error = -1.0f;
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const RigidBone& bone = skeleton.get_bone(bone_index);

			// We use a virtual vertex to simulate skinning
			// We use 2 virtual vertices, to ensure we have at least one that isn't co-linear with the rotation axis
			float vtx_distance = float(bone.vertex_distance);
			Vector4_32 vtx0 = vector_mul(x_axis, vtx_distance);
			Vector4_32 vtx1 = vector_mul(y_axis, vtx_distance);

			Vector4_32 raw_vtx0 = transform_position(raw_object_pose[bone_index], vtx0);
			Vector4_32 lossy_vtx0 = transform_position(lossy_object_pose[bone_index], vtx0);
			float vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);

			Vector4_32 raw_vtx1 = transform_position(raw_object_pose[bone_index], vtx1);
			Vector4_32 lossy_vtx1 = transform_position(lossy_object_pose[bone_index], vtx1);
			float vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

			float bone_error = max(vtx0_error, vtx1_error);
			if (out_error_per_bone != nullptr)
				out_error_per_bone[bone_index] = bone_error;

			error = max(error, bone_error);
		}

		ACL_ENSURE(error >= 0.0f, "Invalid error: %f", error);

		deallocate_type_array(allocator, raw_object_pose, num_bones);
		deallocate_type_array(allocator, lossy_object_pose, num_bones);

		return error;
	}

	inline float calculate_local_bone_error_no_scale(const RigidSkeleton& skeleton, const Transform_32* raw_local_pose, const Transform_32* lossy_local_pose, uint16_t bone_index)
	{
		ACL_ENSURE(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

		const RigidBone& bone = skeleton.get_bone(bone_index);
		float vtx_distance = float(bone.vertex_distance);

		Vector4_32 vtx0 = vector_set(vtx_distance, 0.0f, 0.0f);
		Vector4_32 vtx1 = vector_set(0.0f, vtx_distance, 0.0f);

		Vector4_32 raw_vtx0 = transform_position_no_scale(raw_local_pose[bone_index], vtx0);
		Vector4_32 lossy_vtx0 = transform_position_no_scale(lossy_local_pose[bone_index], vtx0);
		float vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);

		Vector4_32 raw_vtx1 = transform_position_no_scale(raw_local_pose[bone_index], vtx1);
		Vector4_32 lossy_vtx1 = transform_position_no_scale(lossy_local_pose[bone_index], vtx1);
		float vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

		return max(vtx0_error, vtx1_error);
	}

	inline float calculate_local_bone_error(const RigidSkeleton& skeleton, const Transform_32* raw_local_pose, const Transform_32* lossy_local_pose, uint16_t bone_index)
	{
		ACL_ENSURE(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

		const RigidBone& bone = skeleton.get_bone(bone_index);
		float vtx_distance = float(bone.vertex_distance);

		Vector4_32 vtx0 = vector_set(vtx_distance, 0.0f, 0.0f);
		Vector4_32 vtx1 = vector_set(0.0f, vtx_distance, 0.0f);

		Vector4_32 raw_vtx0 = transform_position(raw_local_pose[bone_index], vtx0);
		Vector4_32 lossy_vtx0 = transform_position(lossy_local_pose[bone_index], vtx0);
		float vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);

		Vector4_32 raw_vtx1 = transform_position(raw_local_pose[bone_index], vtx1);
		Vector4_32 lossy_vtx1 = transform_position(lossy_local_pose[bone_index], vtx1);
		float vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

		return max(vtx0_error, vtx1_error);
	}

	inline float calculate_object_bone_error_no_scale(const RigidSkeleton& skeleton, const Transform_32* raw_local_pose, const Transform_32* lossy_local_pose, uint16_t bone_index)
	{
		ACL_ENSURE(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

		Transform_32 raw_obj_transform = raw_local_pose[0];
		Transform_32 lossy_obj_transform = lossy_local_pose[0];

		const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
		for (uint16_t chain_bone_index : bone_chain)
		{
			if (chain_bone_index != 0)
			{
				raw_obj_transform = transform_mul_no_scale(raw_local_pose[chain_bone_index], raw_obj_transform);
				lossy_obj_transform = transform_mul_no_scale(lossy_local_pose[chain_bone_index], lossy_obj_transform);
			}
		}

		const RigidBone& target_bone = skeleton.get_bone(bone_index);
		const float vtx_distance = float(target_bone.vertex_distance);

		const Vector4_32 vtx0 = vector_set(vtx_distance, 0.0f, 0.0f);
		const Vector4_32 vtx1 = vector_set(0.0f, vtx_distance, 0.0f);

		const Vector4_32 raw_vtx0 = transform_position_no_scale(raw_obj_transform, vtx0);
		const Vector4_32 raw_vtx1 = transform_position_no_scale(raw_obj_transform, vtx1);
		const Vector4_32 lossy_vtx0 = transform_position_no_scale(lossy_obj_transform, vtx0);
		const Vector4_32 lossy_vtx1 = transform_position_no_scale(lossy_obj_transform, vtx1);

		const float vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);
		const float vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

		return max(vtx0_error, vtx1_error);
	}

	inline float calculate_object_bone_error(const RigidSkeleton& skeleton, const Transform_32* raw_local_pose, const Transform_32* lossy_local_pose, uint16_t bone_index)
	{
		ACL_ENSURE(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

		AffineMatrix_32 raw_obj_mtx = matrix_from_transform(raw_local_pose[0]);
		AffineMatrix_32 lossy_obj_mtx = matrix_from_transform(lossy_local_pose[0]);

		const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
		for (uint16_t chain_bone_index : bone_chain)
		{
			if (chain_bone_index != 0)
			{
				raw_obj_mtx = matrix_mul(matrix_from_transform(raw_local_pose[chain_bone_index]), raw_obj_mtx);
				lossy_obj_mtx = matrix_mul(matrix_from_transform(lossy_local_pose[chain_bone_index]), lossy_obj_mtx);
			}
		}

		const RigidBone& target_bone = skeleton.get_bone(bone_index);
		const float vtx_distance = float(target_bone.vertex_distance);

		const Vector4_32 vtx0 = vector_set(vtx_distance, 0.0f, 0.0f);
		const Vector4_32 vtx1 = vector_set(0.0f, vtx_distance, 0.0f);

		const Vector4_32 raw_vtx0 = matrix_mul_position(raw_obj_mtx, vtx0);
		const Vector4_32 raw_vtx1 = matrix_mul_position(raw_obj_mtx, vtx1);
		const Vector4_32 lossy_vtx0 = matrix_mul_position(lossy_obj_mtx, vtx0);
		const Vector4_32 lossy_vtx1 = matrix_mul_position(lossy_obj_mtx, vtx1);

		const float vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);
		const float vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

		return max(vtx0_error, vtx1_error);
	}

	struct BoneError
	{
		uint16_t index;
		double error;
		double sample_time;
	};

	inline BoneError calculate_compressed_clip_error(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, bool has_scale,
		AllocateDecompressionContext allocate_context, DecompressPose decompress_pose, DeallocateDecompressionContext deallocate_context)
	{
		uint16_t num_bones = clip.get_num_bones();
		float clip_duration = clip.get_duration();
		float sample_rate = float(clip.get_sample_rate());
		uint32_t num_samples = calculate_num_samples(clip_duration, clip.get_sample_rate());

		void* context = allocate_context(allocator);

		Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
		Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);

		BoneError bone_error = { INVALID_BONE_INDEX, 0.0f, 0.0f };

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			float sample_time = min(float(sample_index) / sample_rate, clip_duration);

			clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
			decompress_pose(context, sample_time, lossy_pose_transforms, num_bones);

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				float error;
				if (has_scale)
					error = calculate_object_bone_error(skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);
				else
					error = calculate_object_bone_error_no_scale(skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);

				if (error > bone_error.error)
				{
					bone_error.error = error;
					bone_error.index = bone_index;
					bone_error.sample_time = sample_time;
				}
			}
		}

		deallocate_type_array(allocator, raw_pose_transforms, num_bones);
		deallocate_type_array(allocator, lossy_pose_transforms, num_bones);
		deallocate_context(allocator, context);

		return bone_error;
	}
}
