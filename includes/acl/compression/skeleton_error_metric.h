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

#include "acl/math/transform_64.h"
#include "acl/math/scalar_64.h"
#include "acl/compression/skeleton.h"

#include <algorithm>

namespace acl
{
	// TODO: Add separate types for local/object space poses, avoid any possible usage error
	// TODO: Add a context object to avoid malloc/free of the buffers with every call of the function
	//       or manage the pose buffers externally?
	inline double calculate_skeleton_error(Allocator& allocator, const RigidSkeleton& skeleton, const Transform_64* raw_local_pose, const Transform_64* lossy_local_pose, double* out_error_per_bone = nullptr)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);

		Transform_64* raw_object_pose = allocate_type_array<Transform_64>(allocator, num_bones);
		Transform_64* lossy_object_pose = allocate_type_array<Transform_64>(allocator, num_bones);

		local_to_object_space(skeleton, raw_local_pose, raw_object_pose);
		local_to_object_space(skeleton, lossy_local_pose, lossy_object_pose);

		Vector4_64 x_axis = vector_set(1.0, 0.0, 0.0);
		Vector4_64 y_axis = vector_set(0.0, 1.0, 0.0);

		double error = -1.0;
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const RigidBone& bone = skeleton.get_bone(bone_index);

			// We use a virtual vertex to simulate skinning
			// We use 2 virtual vertices, to ensure we have at least one that isn't co-linear with the rotation axis
			double vtx_distance = bone.vertex_distance;
			Vector4_64 vtx0 = vector_mul(x_axis, vtx_distance);
			Vector4_64 vtx1 = vector_mul(y_axis, vtx_distance);

			Vector4_64 raw_vtx0 = transform_position(raw_object_pose[bone_index], vtx0);
			Vector4_64 lossy_vtx0 = transform_position(lossy_object_pose[bone_index], vtx0);
			double vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);

			Vector4_64 raw_vtx1 = transform_position(raw_object_pose[bone_index], vtx1);
			Vector4_64 lossy_vtx1 = transform_position(lossy_object_pose[bone_index], vtx1);
			double vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);

			double bone_error = max(vtx0_error, vtx1_error);
			if (out_error_per_bone != nullptr)
				out_error_per_bone[bone_index] = bone_error;

			error = max(error, bone_error);
		}

		ACL_ENSURE(error >= 0.0, "Invalid error: %f", error);

		deallocate_type_array(allocator, raw_object_pose, num_bones);
		deallocate_type_array(allocator, lossy_object_pose, num_bones);

		return error;
	}

	struct BoneTrackError
	{
		double rotation;
		double translation;
	};

	inline void calculate_skeleton_error_contribution(const RigidSkeleton& skeleton, const Transform_64* raw_local_pose, const Transform_64* lossy_local_pose, uint16_t target_bone_index, BoneTrackError* error_per_track)
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
		std::fill(&error_per_track[0], &error_per_track[num_bones], BoneTrackError{0.0, 0.0});

		Vector4_64 x_axis = vector_set(1.0, 0.0, 0.0);
		Vector4_64 y_axis = vector_set(0.0, 1.0, 0.0);

		const RigidBone& target_bone = skeleton.get_bone(target_bone_index);
		double target_vtx_distance = target_bone.vertex_distance;
		Vector4_64 vtx0 = vector_mul(x_axis, target_vtx_distance);
		Vector4_64 vtx1 = vector_mul(y_axis, target_vtx_distance);

		Vector4_64 raw_vtx0 = vtx0;
		Vector4_64 lossy_vtx0 = vtx0;
		Vector4_64 raw_vtx1 = vtx1;
		Vector4_64 lossy_vtx1 = vtx1;

		double child_bone_error_vtx0 = 0.0;
		double child_bone_error_vtx1 = 0.0;

		// Initial child bone index is our target bone index, everything is initialized to 0.0,
		// we'll end up subtracting 0.0 from our current error, no invalid ptr access, no branching
		uint16_t child_bone_index = target_bone_index;
		uint16_t current_bone_index = target_bone_index;
		while (current_bone_index != INVALID_BONE_INDEX)
		{
			// transformed vtx = (((vtx * rotation) * translation) * scale)
			raw_vtx0 = quat_rotate(raw_local_pose[current_bone_index].rotation, raw_vtx0);
			lossy_vtx0 = quat_rotate(lossy_local_pose[current_bone_index].rotation, lossy_vtx0);
			double vtx0_compound_rotation_error = vector_distance3(raw_vtx0, lossy_vtx0);

			raw_vtx1 = quat_rotate(raw_local_pose[current_bone_index].rotation, raw_vtx1);
			lossy_vtx1 = quat_rotate(lossy_local_pose[current_bone_index].rotation, lossy_vtx1);
			double vtx1_compound_rotation_error = vector_distance3(raw_vtx1, lossy_vtx1);

			raw_vtx0 = vector_add(raw_vtx0, raw_local_pose[current_bone_index].translation);
			lossy_vtx0 = vector_add(lossy_vtx0, lossy_local_pose[current_bone_index].translation);
			double vt0_compound_error = vector_distance3(raw_vtx0, lossy_vtx0);

			raw_vtx1 = vector_add(raw_vtx1, raw_local_pose[current_bone_index].translation);
			lossy_vtx1 = vector_add(lossy_vtx1, lossy_local_pose[current_bone_index].translation);
			double vt1_compound_error = vector_distance3(raw_vtx1, lossy_vtx1);

			// Remove any error from the previous child we processed, we only want to track the error
			// introduced by this very bone and its tracks, not the compound error
			double vtx0_rotation_error = abs(vtx0_compound_rotation_error - child_bone_error_vtx0);
			double vtx1_rotation_error = abs(vtx1_compound_rotation_error - child_bone_error_vtx1);
			double vtx0_translation_error = abs(vt0_compound_error - vtx0_rotation_error);
			double vtx1_translation_error = abs(vt1_compound_error - vtx1_rotation_error);

			double rotation_error = max(vtx0_rotation_error, vtx1_rotation_error);
			double translation_error = max(vtx0_translation_error, vtx1_translation_error);

			error_per_track[current_bone_index] = BoneTrackError{rotation_error, translation_error};

			// virtual object vertex = (((virtual local vertex * bone 2 local transform) * bone 1 local transform) * bone 0 local transform)
			const RigidBone& bone = skeleton.get_bone(current_bone_index);
			current_bone_index = bone.parent_index;

			child_bone_error_vtx0 = vt0_compound_error;
			child_bone_error_vtx1 = vt1_compound_error;
		}
	}
}
