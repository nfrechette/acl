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

// Included only once from transform_space.h

#include "acl/version.h"
#include "acl/core/additive_utils.h"
#include "acl/core/error.h"
#include "acl/core/error_result.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_base_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Returns true if 'name' starts with 'candidate' and no other name character follows.
		// The name does not need a null terminator after the candidate.
		inline bool is_transform_space_name(const char* name, const char* candidate)
		{
			const size_t length = std::strlen(candidate);
			if (std::strncmp(name, candidate, length) != 0)
				return false;

			const char next = name[length];
			const bool is_name_character = (next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || (next >= '0' && next <= '9') || next == '_';
			return !is_name_character;
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the parent index that compression uses to measure the error of a transform.
		// In mesh space, a transform does not use its parent: it has no parent for the error.
		constexpr uint32_t get_error_parent_index(transform_space_t space, uint32_t parent_index)
		{
			return space == transform_space_t::mesh ? k_invalid_track_index : parent_index;
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the default scale of the clip samples with a base format: 0.0 or 1.0.
		// With the 'custom' base format, the base adapter gives the default scale.
		inline float get_base_default_scale(additive_clip_format8 base_format, const itransform_base_adapter_t* base_adapter)
		{
			if (base_format == additive_clip_format8::custom)
			{
				ACL_ASSERT(base_adapter != nullptr, "The custom base format requires a base adapter");
				return base_adapter != nullptr ? base_adapter->get_default_scale() : 1.0F;
			}

			return get_additive_default_scale(base_format);
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the default scale of the clip samples with a transform space description: 0.0 or 1.0.
		inline float get_base_default_scale(const transform_space_desc_t& transform_space)
		{
			return get_base_default_scale(transform_space.base_format, transform_space.base_adapter);
		}
	}

	constexpr const char* get_transform_space_name(transform_space_t space)
	{
		return space == transform_space_t::local ? "local"
			: (space == transform_space_t::mesh ? "mesh"
			: (space == transform_space_t::mesh_rotation ? "mesh_rotation" : "<Invalid>"));
	}

	inline bool get_transform_space(const char* name, transform_space_t& out_space)
	{
		ACL_ASSERT(name != nullptr, "Transform space name cannot be null");
		if (name == nullptr)
			return false;

		if (acl_impl::is_transform_space_name(name, "local"))
		{
			out_space = transform_space_t::local;
			return true;
		}

		if (acl_impl::is_transform_space_name(name, "mesh"))
		{
			out_space = transform_space_t::mesh;
			return true;
		}

		if (acl_impl::is_transform_space_name(name, "mesh_rotation"))
		{
			out_space = transform_space_t::mesh_rotation;
			return true;
		}

		return false;
	}

	inline error_result transform_space_desc_t::is_valid() const
	{
		if (space != transform_space_t::local && space != transform_space_t::mesh && space != transform_space_t::mesh_rotation)
			return error_result("Invalid transform space");

		if (base_format > additive_clip_format8::custom)
			return error_result("Invalid base format");

		if (base_format == additive_clip_format8::custom)
		{
			if (base_adapter == nullptr)
				return error_result("The custom base format requires a base adapter");

			const float default_scale = base_adapter->get_default_scale();
			if (default_scale != 0.0F && default_scale != 1.0F)
				return error_result("The default scale of the base adapter must be 0.0 or 1.0");
		}
		else if (base_adapter != nullptr)
			return error_result("A base adapter requires the custom base format");

		// A relative base multiplies the whole transforms: the rotations and the translations are in different spaces
		if (space == transform_space_t::mesh_rotation && base_format == additive_clip_format8::relative)
			return error_result("The mesh rotation space does not support the relative base format");

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

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
