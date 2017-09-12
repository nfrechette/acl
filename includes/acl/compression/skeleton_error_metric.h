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

#include <algorithm>
#include <functional>

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
		uint16_t num_bones = skeleton.get_num_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);
		ACL_ENSURE(bone_index < num_bones, "Invalid bone index: %u", bone_index);

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
		uint16_t num_bones = skeleton.get_num_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);
		ACL_ENSURE(bone_index < num_bones, "Invalid bone index: %u", bone_index);

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
		uint16_t num_bones = skeleton.get_num_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);
		ACL_ENSURE(bone_index < num_bones, "Invalid bone index: %u", bone_index);

		const RigidBone& target_bone = skeleton.get_bone(bone_index);
		float vtx_distance = float(target_bone.vertex_distance);

		std::function<Transform_32(const Transform_32*, uint16_t)> apply_fun;
		apply_fun = [&](const Transform_32* local_pose, uint16_t bone_index) -> Transform_32
		{
			if (bone_index == 0)
				return local_pose[0];
			const RigidBone& bone = skeleton.get_bone(bone_index);
			Transform_32 parent_transform = apply_fun(local_pose, bone.parent_index);
			return transform_mul_no_scale(local_pose[bone_index], parent_transform);
		};

		Transform_32 raw_obj_transform = apply_fun(raw_local_pose, bone_index);
		Transform_32 lossy_obj_transform = apply_fun(lossy_local_pose, bone_index);

		Vector4_32 vtx0 = vector_set(vtx_distance, 0.0f, 0.0f);
		Vector4_32 vtx1 = vector_set(0.0f, vtx_distance, 0.0f);

		Vector4_32 raw_vtx0 = transform_position_no_scale(raw_obj_transform, vtx0);
		Vector4_32 raw_vtx1 = transform_position_no_scale(raw_obj_transform, vtx1);
		Vector4_32 lossy_vtx0 = transform_position_no_scale(lossy_obj_transform, vtx0);
		Vector4_32 lossy_vtx1 = transform_position_no_scale(lossy_obj_transform, vtx1);

		float vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);
		float vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

		return max(vtx0_error, vtx1_error);
	}

	inline float calculate_object_bone_error(const RigidSkeleton& skeleton, const Transform_32* raw_local_pose, const Transform_32* lossy_local_pose, uint16_t bone_index)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);
		ACL_ENSURE(bone_index < num_bones, "Invalid bone index: %u", bone_index);

		const RigidBone& target_bone = skeleton.get_bone(bone_index);
		float vtx_distance = float(target_bone.vertex_distance);

		std::function<AffineMatrix_32(const Transform_32*, uint16_t)> apply_fun;
		apply_fun = [&](const Transform_32* local_pose, uint16_t bone_index) -> AffineMatrix_32
		{
			if (bone_index == 0)
				return matrix_from_transform(local_pose[0]);
			const RigidBone& bone = skeleton.get_bone(bone_index);
			AffineMatrix_32 parent_mtx = apply_fun(local_pose, bone.parent_index);
			AffineMatrix_32 local_mtx = matrix_from_transform(local_pose[bone_index]);
			return matrix_mul(local_mtx, parent_mtx);
		};

		AffineMatrix_32 raw_obj_mtx = apply_fun(raw_local_pose, bone_index);
		AffineMatrix_32 lossy_obj_mtx = apply_fun(lossy_local_pose, bone_index);

		Vector4_32 vtx0 = vector_set(vtx_distance, 0.0f, 0.0f);
		Vector4_32 vtx1 = vector_set(0.0f, vtx_distance, 0.0f);

		Vector4_32 raw_vtx0 = matrix_mul_position(raw_obj_mtx, vtx0);
		Vector4_32 raw_vtx1 = matrix_mul_position(raw_obj_mtx, vtx1);
		Vector4_32 lossy_vtx0 = matrix_mul_position(lossy_obj_mtx, vtx0);
		Vector4_32 lossy_vtx1 = matrix_mul_position(lossy_obj_mtx, vtx1);

		float vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);
		float vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

		return max(vtx0_error, vtx1_error);
	}

	struct BoneError
	{
		uint16_t index;
		double error;
		double sample_time;
	};

	inline BoneError calculate_compressed_clip_error(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, bool has_scale,
		std::function<void*(Allocator& allocator)> alloc_ctx_fun,
		std::function<void(Allocator& allocator, void* context)> free_ctx_fun,
		std::function<void(void* context, float sample_time, Transform_32* out_transforms, uint16_t num_transforms)> compressed_clip_sample_fun)
	{
		uint16_t num_bones = clip.get_num_bones();
		float clip_duration = clip.get_duration();
		float sample_rate = float(clip.get_sample_rate());
		uint32_t num_samples = calculate_num_samples(clip_duration, clip.get_sample_rate());

		void* context = alloc_ctx_fun(allocator);

		Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
		Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);

		BoneError bone_error = { INVALID_BONE_INDEX, 0.0f, 0.0f };

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			float sample_time = min(float(sample_index) / sample_rate, clip_duration);

			clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
			compressed_clip_sample_fun(context, sample_time, lossy_pose_transforms, num_bones);

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
		free_ctx_fun(allocator, context);

		return bone_error;
	}

	struct BoneTrackError
	{
		float rotation;
		float translation;
	};

	inline void calculate_skeleton_error_contribution(const RigidSkeleton& skeleton, const Transform_32* raw_local_pose, const Transform_32* lossy_local_pose, uint16_t target_bone_index, BoneTrackError* error_per_track)
	{
		// Our object space metric is calculated like this, where 0 is the root bone:
		// bone 2 object transform = bone 2 local transform * bone 1 local transform * bone 0 local transform
		// virtual object vertex = virtual local vertex * bone 2 object transform
		// virtual object vertex = virtual local vertex * (bone 2 local transform * bone 1 local transform * bone 0 local transform)
		//
		// The distance between the virtual object vertex from the raw and lossy poses gives us our error value.
		//
		// Due to the properties of matrix associativity, we can rewrite the above operation as follow:
		// virtual object vertex = (((virtual local vertex * bone 2 local transform) * bone 1 local transform) * bone 0 local transform)
		//
		// As such, for the error measured in object space for a specific bone, we can split the equation and calculate
		// the contribution of every bone in the chain.
		// We can also extend this idea to the contribution of the individual transform components: rotation, translation, scale
		// transform = rotation * translation * scale
		// transformed vtx = vtx * transform
		// transformed vtx = vtx * (rotation * translation * scale)
		// transformed vtx = (((vtx * rotation) * translation) * scale)

		// TODO: Implement, for a specific bone index, return the error contribution per parent bone track/stream
		uint16_t num_bones = skeleton.get_num_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);
		ACL_ENSURE(target_bone_index < num_bones, "Invalid target bone index: %u >= %u", target_bone_index, num_bones);

		// Clear everything
		std::fill(&error_per_track[0], &error_per_track[num_bones], BoneTrackError{-1.0f, -1.0f});

		Vector4_32 x_axis = vector_set(1.0f, 0.0f, 0.0f);
		Vector4_32 y_axis = vector_set(0.0f, 1.0f, 0.0f);

		const RigidBone& target_bone = skeleton.get_bone(target_bone_index);
		float target_vtx_distance = float(target_bone.vertex_distance);
		Vector4_32 vtx0 = vector_mul(x_axis, target_vtx_distance);
		Vector4_32 vtx1 = vector_mul(y_axis, target_vtx_distance);

		Vector4_32 raw_vtx0 = vtx0;
		Vector4_32 raw_vtx1 = vtx1;

		// Initial child bone index is our target bone index, everything is initialized to 0.0,
		// we'll end up subtracting 0.0 from our current error, no invalid ptr access, no branching
		uint16_t current_bone_index = target_bone_index;
		while (current_bone_index != INVALID_BONE_INDEX)
		{
			// transformed vtx = (((vtx * rotation) * translation) * scale)
			// Reset our lossy vtx to match the raw vtx in order to measure only the error contribution of this track
			Vector4_32 lossy_vtx0 = raw_vtx0;
			Vector4_32 lossy_vtx1 = raw_vtx1;

			raw_vtx0 = quat_rotate(raw_local_pose[current_bone_index].rotation, raw_vtx0);
			lossy_vtx0 = quat_rotate(lossy_local_pose[current_bone_index].rotation, lossy_vtx0);

			raw_vtx1 = quat_rotate(raw_local_pose[current_bone_index].rotation, raw_vtx1);
			lossy_vtx1 = quat_rotate(lossy_local_pose[current_bone_index].rotation, lossy_vtx1);

			float vtx0_rotation_error = vector_distance3(raw_vtx0, lossy_vtx0);
			float vtx1_rotation_error = vector_distance3(raw_vtx1, lossy_vtx1);
			float rotation_error = max(vtx0_rotation_error, vtx1_rotation_error);

			// Reset our lossy vtx to match the raw vtx in order to measure only the error contribution of this track
			lossy_vtx0 = raw_vtx0;
			lossy_vtx1 = raw_vtx1;

			raw_vtx0 = vector_add(raw_local_pose[current_bone_index].translation, raw_vtx0);
			lossy_vtx0 = vector_add(lossy_local_pose[current_bone_index].translation, lossy_vtx0);

			raw_vtx1 = vector_add(raw_local_pose[current_bone_index].translation, raw_vtx1);
			lossy_vtx1 = vector_add(lossy_local_pose[current_bone_index].translation, lossy_vtx1);

			float vtx0_translation_error = vector_distance3(raw_vtx0, lossy_vtx0);
			float vtx1_translation_error = vector_distance3(raw_vtx1, lossy_vtx1);
			float translation_error = max(vtx0_translation_error, vtx1_translation_error);

			error_per_track[current_bone_index] = BoneTrackError{rotation_error, translation_error};

			// virtual object vertex = (((virtual local vertex * bone 2 local transform) * bone 1 local transform) * bone 0 local transform)
			const RigidBone& bone = skeleton.get_bone(current_bone_index);
			current_bone_index = bone.parent_index;
		}
	}
}
