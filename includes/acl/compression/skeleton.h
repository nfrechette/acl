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
