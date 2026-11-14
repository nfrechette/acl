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
#include "acl/core/additive_utils.h"
#include "acl/core/error.h"
#include "acl/core/error_result.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track_array.h"

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// The space of the samples of transform tracks: how a sample uses the transform hierarchy.
	// Compression uses the space to put the samples in object space, where it measures the error.
	// The space does not change the compressed data: decompression returns the samples in the same space.
	enum class transform_space_t : uint8_t
	{
		//////////////////////////////////////////////////////////////////////////
		// Each transform is relative to its parent transform (local space, forward kinematics).
		// A root transform is in object space.
		// object_transform = mul(local_transform, parent_object_transform)
		local				= 0,
	};

	//////////////////////////////////////////////////////////////////////////
	// Returns the name of a transform space.
	constexpr const char* get_transform_space_name(transform_space_t space)
	{
		return space == transform_space_t::local ? "local" : "<Invalid>";
	}

	//////////////////////////////////////////////////////////////////////////
	// Finds the transform space that has the provided name.
	// Returns true on success, false otherwise.
	inline bool get_transform_space(const char* name, transform_space_t& out_space)
	{
		ACL_ASSERT(name != nullptr, "Transform space name cannot be null");
		if (name == nullptr)
			return false;

		const char* local_name = "local";
		if (std::strncmp(name, local_name, std::strlen(local_name)) == 0)
		{
			out_space = transform_space_t::local;
			return true;
		}

		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Describes the space of transform tracks and their base.
	// It holds two independent properties:
	//    - the space: how a sample uses the transform hierarchy, see 'transform_space_t'
	//    - the base format: how a sample combines with the base, see 'additive_clip_format8'
	// Compression combines each sample with the base first. Then it puts the result in object space.
	// The default value describes local space transforms that have no base.
	//////////////////////////////////////////////////////////////////////////
	struct transform_space_desc_t
	{
		//////////////////////////////////////////////////////////////////////////
		// The space of the samples.
		// Defaults to 'local'.
		transform_space_t space = transform_space_t::local;

		//////////////////////////////////////////////////////////////////////////
		// How a sample combines with the base.
		// With 'none', the clip has no base and 'base' must be null.
		// Defaults to 'none'.
		additive_clip_format8 base_format = additive_clip_format8::none;

		//////////////////////////////////////////////////////////////////////////
		// The base transform tracks (vqvf, vqf, or vqsf), in local space.
		// The base can have a different transform type and a different number of samples than the clip.
		// A clip sample uses the base sample at the same normalized time, rounded to the nearest sample.
		// Required when 'base_format' is not 'none'. The caller owns the base.
		// Defaults to null.
		const track_array* base = nullptr;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not the clip has a base.
		bool has_base() const { return base_format != additive_clip_format8::none; }

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a transform space description is valid or not.
		// It is valid if:
		//    - The space and the base format are valid values
		//    - With a base format, the base is a valid and non-empty list of transform tracks
		//    - Without a base format, the base is null
		error_result is_valid() const
		{
			if (space != transform_space_t::local)
				return error_result("Invalid transform space");

			if (base_format > additive_clip_format8::additive1)
				return error_result("Invalid base format");

			if (base_format == additive_clip_format8::none)
			{
				if (base != nullptr)
					return error_result("A base requires a base format");

				return error_result();
			}

			if (base == nullptr)
				return error_result("The base format requires a base");

			const error_result result = base->is_valid();
			if (result.any())
				return result;

			if (base->is_empty())
				return error_result("The base cannot be empty");

			if (!acl_impl::is_transform_track_type(base->get_track_type()))
				return error_result("The base must contain transform tracks");

			return error_result();
		}
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
