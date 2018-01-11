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

#include "acl/core/error.h"
#include "acl/compression/skeleton.h"
#include "acl/math/transform_32.h"

#include <cstdint>

namespace acl
{
	// Note: It is safe for both pose buffers to alias since the data is sorted parent first
	inline void local_to_object_space(const RigidSkeleton& skeleton, const Transform_32* local_pose, Transform_32* out_object_pose)
	{
		const uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);

		out_object_pose[0] = local_pose[0];

		for (uint16_t bone_index = 1; bone_index < num_bones; ++bone_index)
		{
			const uint16_t parent_bone_index = bones[bone_index].parent_index;
			ACL_ENSURE(parent_bone_index < num_bones, "Invalid parent bone index: %u >= %u", parent_bone_index, num_bones);

			out_object_pose[bone_index] = transform_mul(local_pose[bone_index], out_object_pose[parent_bone_index]);
			out_object_pose[bone_index].rotation = quat_normalize(out_object_pose[bone_index].rotation);
		}
	}

	// Note: It is safe for both pose buffers to alias since the data is sorted parent first
	inline void object_to_local_space(const RigidSkeleton& skeleton, const Transform_32* object_pose, Transform_32* out_local_pose)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);

		out_local_pose[0] = object_pose[0];

		for (uint16_t bone_index = 1; bone_index < num_bones; ++bone_index)
		{
			const uint16_t parent_bone_index = bones[bone_index].parent_index;
			ACL_ENSURE(parent_bone_index < num_bones, "Invalid parent bone index: %u >= %u", parent_bone_index, num_bones);

			const Transform_32 inv_parent_transform = transform_inverse(object_pose[parent_bone_index]);
			out_local_pose[bone_index] = transform_mul(inv_parent_transform, object_pose[bone_index]);
			out_local_pose[bone_index].rotation = quat_normalize(out_local_pose[bone_index].rotation);
		}
	}
}
