#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// The space of the samples of transform tracks: how a sample uses the transform hierarchy.
	// Compression uses the space to put the samples in object space, where it measures the error.
	// The space does not change the compressed data: decompression returns the samples in the same space.
	// Each transform track array holds its space, see 'track_array::set_transform_space'.
	enum class transform_space_t : uint8_t
	{
		//////////////////////////////////////////////////////////////////////////
		// Each transform is relative to its parent transform (local space, forward kinematics).
		// A root transform is in object space.
		// object_transform = mul(local_transform, parent_object_transform)
		local				= 0,

		//////////////////////////////////////////////////////////////////////////
		// Each transform is in object space (mesh space): it does not use its parent transform.
		// The error of a transform does not move its children.
		// object_transform = transform
		mesh				= 1,

		//////////////////////////////////////////////////////////////////////////
		// The rotation of each transform is in object space (mesh space). Its translation and
		// its scale are relative to its parent transform, as in local space.
		// The object rotation of a transform is its rotation: the rotation of its parent does not change it.
		// The parent still moves the transform: its object translation uses the parent transform.
		// object_transform = mul(local_transform, parent_object_transform)
		// with local_transform.rotation = mul(transform.rotation, conjugate(parent_object_transform.rotation))
		// It supports the runtime transform types that compose rotations with a quaternion product:
		// 'vqf', 'vqsf', 'vqvf', and 'vqmf'. It does not support the 'relative' base format.
		// With the 'custom' base format, the base adapter receives the base in this space.
		mesh_rotation		= 2,
	};

	//////////////////////////////////////////////////////////////////////////
	// Returns the name of a transform space.
	constexpr const char* get_transform_space_name(transform_space_t space);

	//////////////////////////////////////////////////////////////////////////
	// Finds the transform space that has the provided name.
	// A name matches only when no other name character follows it: 'mesh' does not match 'mesh_space'.
	// Returns true on success, false otherwise.
	bool get_transform_space(const char* name, transform_space_t& out_space);

	ACL_IMPL_VERSION_NAMESPACE_END
}

#include "acl/compression/impl/transform_space.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
