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

// Included only once from transform_base.h

#include "acl/version.h"
#include "acl/core/additive_utils.h"
#include "acl/core/error.h"
#include "acl/core/error_result.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_base_adapter.h"
#include "acl/compression/transform_space.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
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
		// Returns the default scale of the clip samples with a base description: 0.0 or 1.0.
		inline float get_base_default_scale(const transform_base_desc_t& transform_base)
		{
			return get_base_default_scale(transform_base.format, transform_base.adapter);
		}
	}

	inline error_result transform_base_desc_t::is_valid(transform_space_t space) const
	{
		if (format > additive_clip_format8::custom)
			return error_result("Invalid base format");

		if (format == additive_clip_format8::custom)
		{
			if (adapter == nullptr)
				return error_result("The custom base format requires a base adapter");

			const float default_scale = adapter->get_default_scale();
			if (default_scale != 0.0F && default_scale != 1.0F)
				return error_result("The default scale of the base adapter must be 0.0 or 1.0");
		}
		else if (adapter != nullptr)
			return error_result("A base adapter requires the custom base format");

		// A relative base multiplies the whole transforms: the rotations and the translations are in different spaces
		if (space == transform_space_t::mesh_rotation && format == additive_clip_format8::relative)
			return error_result("The mesh rotation space does not support the relative base format");

		if (format == additive_clip_format8::none)
		{
			if (tracks != nullptr)
				return error_result("A base requires a base format");

			return error_result();
		}

		if (tracks == nullptr)
			return error_result("The base format requires a base");

		const error_result result = tracks->is_valid();
		if (result.any())
			return result;

		if (tracks->is_empty())
			return error_result("The base cannot be empty");

		if (!acl_impl::is_transform_track_type(tracks->get_track_type()))
			return error_result("The base must contain transform tracks");

		if (tracks->get_transform_space() != transform_space_t::local)
			return error_result("The base must be in local space");

		return error_result();
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
