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

namespace acl
{
	// TODO: Add separate types for local/object space poses, avoid any possible usage error
	// TODO: Add a context object to avoid malloc/free of the buffers with every call of the function
	//       or manage the pose buffers externally?
	inline double calculate_skeleton_error(Allocator& allocator, const RigidSkeleton& skeleton, const Transform_64* raw_local_pose, const Transform_64* lossy_local_pose)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
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
			// We use a virtual vertex to simulate skinning
			// We use 2 virtual vertices, to ensure we have at least one that isn't co-linear with the rotation axis
			double vtx_distance = bones[bone_index].vertex_distance;
			Vector4_64 vtx0 = vector_mul(x_axis, vtx_distance);
			Vector4_64 vtx1 = vector_mul(y_axis, vtx_distance);

			Vector4_64 raw_vtx0 = transform_position(raw_object_pose[bone_index], vtx0);
			Vector4_64 lossy_vtx0 = transform_position(lossy_object_pose[bone_index], vtx0);
			double vtx0_error = vector_distance3(raw_vtx0, lossy_vtx0);
			error = max(error, vtx0_error);

			Vector4_64 raw_vtx1 = transform_position(raw_object_pose[bone_index], vtx1);
			Vector4_64 lossy_vtx1 = transform_position(lossy_object_pose[bone_index], vtx1);
			double vtx1_error = vector_distance3(raw_vtx1, lossy_vtx1);
			error = max(error, vtx1_error);
		}

		ACL_ENSURE(error >= 0.0, "Invalid error: %f", error);

		allocator.deallocate(raw_object_pose);
		allocator.deallocate(lossy_object_pose);

		return error;
	}
}
