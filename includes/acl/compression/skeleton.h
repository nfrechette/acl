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
#include "acl/core/string.h"
#include "acl/math/transform_32.h"
#include "acl/math/transform_64.h"

#include <stdint.h>

namespace acl
{
	static constexpr uint16_t INVALID_BONE_INDEX = 0xFFFF;

	struct RigidBone
	{
		RigidBone()
			: name()
			, bind_transform(transform_identity_64())
			, vertex_distance(1.0)
			, parent_index(INVALID_BONE_INDEX)
		{
		}

		RigidBone(RigidBone&& bone)
			: name(std::move(bone.name))
			, bind_transform(bone.bind_transform)
			, vertex_distance(bone.vertex_distance)
			, parent_index(bone.parent_index)
		{
			new(this) RigidBone();
		}

		RigidBone& operator=(RigidBone&& bone)
		{
			std::swap(name, bone.name);
			std::swap(bind_transform, bone.bind_transform);
			std::swap(vertex_distance, bone.vertex_distance);
			std::swap(parent_index, bone.parent_index);

			return *this;
		}

		bool is_root() const { return parent_index == INVALID_BONE_INDEX; }

		String			name;

		Transform_64	bind_transform;		// Bind transform is in parent bone local space
		double			vertex_distance;	// Virtual vertex distance used by hierarchical error function
		uint16_t		parent_index;		// TODO: Introduce a type for bone indices
	};

	class RigidSkeleton
	{
	public:
		RigidSkeleton(Allocator& allocator, RigidBone* bones, uint16_t num_bones)
			: m_allocator(allocator)
			, m_bones(allocate_type_array<RigidBone>(allocator, num_bones))
			, m_num_bones(num_bones)
		{
			// Copy and validate the input data
			bool found_root = false;
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				RigidBone& bone = bones[bone_index];

				bool is_root = bone.parent_index == INVALID_BONE_INDEX;

				ACL_ENSURE(is_root || bone.parent_index < bone_index, "Bones must be sorted parent first");
				ACL_ENSURE((is_root && !found_root) || !is_root, "Multiple root bones found");
				ACL_ENSURE(quat_is_finite(bone.bind_transform.rotation), "Bind rotation is invalid: [%f, %f, %f, %f]", quat_get_x(bone.bind_transform.rotation), quat_get_y(bone.bind_transform.rotation), quat_get_z(bone.bind_transform.rotation), quat_get_w(bone.bind_transform.rotation));
				ACL_ENSURE(quat_is_normalized(bone.bind_transform.rotation), "Bind rotation isn't normalized: [%f, %f, %f, %f]", quat_get_x(bone.bind_transform.rotation), quat_get_y(bone.bind_transform.rotation), quat_get_z(bone.bind_transform.rotation), quat_get_w(bone.bind_transform.rotation));
				ACL_ENSURE(vector_is_finite3(bone.bind_transform.translation), "Bind translation is invalid: [%f, %f, %f]", vector_get_x(bone.bind_transform.translation), vector_get_y(bone.bind_transform.translation), vector_get_z(bone.bind_transform.translation));
				ACL_ENSURE(vector_is_finite3(bone.bind_transform.scale), "Bind scale is invalid: [%f, %f, %f]", vector_get_x(bone.bind_transform.scale), vector_get_y(bone.bind_transform.scale), vector_get_z(bone.bind_transform.scale));
				ACL_ENSURE(!vector_any_near_equal3(bone.bind_transform.scale, vector_zero_64()), "Bind scale is zero: [%f, %f, %f]", vector_get_x(bone.bind_transform.scale), vector_get_y(bone.bind_transform.scale), vector_get_z(bone.bind_transform.scale));

				found_root |= is_root;

				m_bones[bone_index] = std::move(bone);
			}

			ACL_ENSURE(found_root, "No root bone found. The root bone must have a parent index = 0xFFFF");
		}

		~RigidSkeleton()
		{
			deallocate_type_array(m_allocator, m_bones, m_num_bones);
		}

		RigidSkeleton(const RigidSkeleton&) = delete;
		RigidSkeleton& operator=(const RigidSkeleton&) = delete;

		const RigidBone* get_bones() const { return m_bones; }
		const RigidBone& get_bone(uint16_t bone_index) const
		{
			ACL_ENSURE(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			return m_bones[bone_index];
		}
		uint16_t get_num_bones() const { return m_num_bones; }

	private:
		Allocator&	m_allocator;
		RigidBone*	m_bones;

		uint16_t	m_num_bones;
	};

	// Note: It is safe for both pose buffers to alias since the data is sorted parent first
	inline void local_to_object_space(const RigidSkeleton& skeleton, const Transform_32* local_pose, Transform_32* out_object_pose)
	{
		uint16_t num_bones = skeleton.get_num_bones();
		const RigidBone* bones = skeleton.get_bones();
		ACL_ENSURE(num_bones != 0, "Invalid number of bones: %u", num_bones);

		out_object_pose[0] = local_pose[0];

		for (uint16_t bone_index = 1; bone_index < num_bones; ++bone_index)
		{
			uint16_t parent_bone_index = bones[bone_index].parent_index;
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
			uint16_t parent_bone_index = bones[bone_index].parent_index;
			ACL_ENSURE(parent_bone_index < num_bones, "Invalid parent bone index: %u >= %u", parent_bone_index, num_bones);

			Transform_32 inv_parent_transform = transform_inverse(object_pose[parent_bone_index]);
			out_local_pose[bone_index] = transform_mul(inv_parent_transform, object_pose[bone_index]);
			out_local_pose[bone_index].rotation = quat_normalize(out_local_pose[bone_index].rotation);
		}
	}
}
