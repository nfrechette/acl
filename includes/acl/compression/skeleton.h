#pragma once

#include "acl/memory.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_64.h"

#include <stdint.h>

namespace acl
{
	struct RigidBone
	{
		const char*	name;

		// TODO: Introduce a type for bone indices
		uint16_t	parent_index;		// 0xFFFF == Invalid index

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
}
