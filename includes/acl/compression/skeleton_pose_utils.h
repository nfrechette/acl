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

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/compression/skeleton.h"

#include <rtm/qvvf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	// Note: It is safe for both pose buffers to alias since the data is sorted parent first
	inline void local_to_object_space(const RigidSkeleton& skeleton, const rtm::qvvf* local_pose, rtm::qvvf* out_object_pose)
	{
		const uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
		ACL_ASSERT(num_bones != 0, "Invalid number of bones: %u", num_bones);

		out_object_pose[0] = local_pose[0];

		for (uint16_t bone_index = 1; bone_index < num_bones; ++bone_index)
		{
			const uint16_t parent_bone_index = bones[bone_index].parent_index;
			ACL_ASSERT(parent_bone_index < num_bones, "Invalid parent bone index: %u >= %u", parent_bone_index, num_bones);

			out_object_pose[bone_index] = rtm::qvv_normalize(rtm::qvv_mul(local_pose[bone_index], out_object_pose[parent_bone_index]));
		}
	}

	// Note: It is safe for both pose buffers to alias since the data is sorted parent first
	inline void object_to_local_space(const RigidSkeleton& skeleton, const rtm::qvvf* object_pose, rtm::qvvf* out_local_pose)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
		ACL_ASSERT(num_bones != 0, "Invalid number of bones: %u", num_bones);

		out_local_pose[0] = object_pose[0];

		for (uint16_t bone_index = 1; bone_index < num_bones; ++bone_index)
		{
			const uint16_t parent_bone_index = bones[bone_index].parent_index;
			ACL_ASSERT(parent_bone_index < num_bones, "Invalid parent bone index: %u >= %u", parent_bone_index, num_bones);

			const rtm::qvvf inv_parent_transform = rtm::qvv_inverse(object_pose[parent_bone_index]);
			out_local_pose[bone_index] = rtm::qvv_normalize(rtm::qvv_mul(inv_parent_transform, object_pose[bone_index]));
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
