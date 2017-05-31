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

#include "acl/core/memory.h"
#include "acl/math/quat_64.h"		// todo remove
#include "acl/math/vector4_64.h"	// todo remove
#include "acl/math/transform_64.h"

#include <stdint.h>

namespace acl
{
	struct RigidBone
	{
		const char*	name;

		// TODO: Introduce a type for bone indices
		uint16_t	parent_index;		// 0xFFFF == Invalid index

		// TODO: convert to transform
		Quat_64		bind_rotation;
		Vector4_64	bind_translation;
		// TODO: bind_scale

		double		vertex_distance;	// Virtual vertex distance used by hierarchical error function
	};

	class RigidSkeleton
	{
	public:
		RigidSkeleton(Allocator& allocator, uint16_t num_bones)
			: m_allocator(allocator)
			, m_bones(allocate_type_array<RigidBone>(allocator, num_bones))
			, m_num_bones(num_bones)
		{}

		~RigidSkeleton()
		{
			m_allocator.deallocate(m_bones);
		}

		RigidSkeleton(const RigidSkeleton&) = delete;
		RigidSkeleton& operator=(const RigidSkeleton&) = delete;

		RigidBone* get_bones() { return m_bones; }
		const RigidBone* get_bones() const { return m_bones; }
		uint16_t get_num_bones() const { return m_num_bones; }

	private:
		Allocator&	m_allocator;
		RigidBone*	m_bones;

		uint16_t	m_num_bones;
	};

	// Note: It is safe for both pose buffers to alias since the data is sorted parent first
	inline void local_to_object_space(const RigidSkeleton& skeleton, const Transform_64* local_pose, Transform_64* out_object_pose)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
		ensure(num_bones != 0);

		out_object_pose[0] = local_pose[0];

		for (uint16_t bone_index = 1; bone_index < num_bones; ++bone_index)
		{
			uint16_t parent_bone_index = bones[bone_index].parent_index;
			out_object_pose[bone_index] = transform_mul(out_object_pose[parent_bone_index], local_pose[bone_index]);
		}
	}

	// Note: It is safe for both pose buffers to alias since the data is sorted parent first
	inline void object_to_local_space(const RigidSkeleton& skeleton, const Transform_64* object_pose, Transform_64* out_local_pose)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
		ensure(num_bones != 0);

		out_local_pose[0] = object_pose[0];

		for (uint16_t bone_index = 1; bone_index < num_bones; ++bone_index)
		{
			uint16_t parent_bone_index = bones[bone_index].parent_index;
			Transform_64 inv_parent_transform = transform_inverse(object_pose[parent_bone_index]);
			out_local_pose[bone_index] = transform_mul(inv_parent_transform, object_pose[bone_index]);
		}
	}
}
