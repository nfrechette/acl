#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2025 Nicholas Frechette & Animation Compression Library contributors
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

// Included only once from sample_interpolator.h

#include "acl/version.h"
#include "acl/core/memory_utils.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	inline const char* get_sample_interpolator_name(sample_interpolator_t interpolator)
	{
		switch (interpolator)
		{
		case sample_interpolator_t::constant:	return "constant";
		case sample_interpolator_t::linear:		return "linear";
		default:								return "<Invalid>";
		}
	}

	inline bool get_sample_interpolator(const char* interpolator, sample_interpolator_t& out_interpolator)
	{
		// Entries in the same order as the enum integral value
		static const char* k_sample_interpolator_names[] =
		{
			"constant",
			"linear",
		};

		static_assert(get_array_size(k_sample_interpolator_names) == (size_t)sample_interpolator_t::linear + 1, "Unexpected array size");

		ACL_ASSERT(interpolator != nullptr, "Track interpolator name cannot be null");
		if (interpolator == nullptr)
			return false;

		for (size_t index = 0; index < get_array_size(k_sample_interpolator_names); ++index)
		{
			const char* name = k_sample_interpolator_names[index];
			if (std::strncmp(interpolator, name, std::strlen(name)) == 0)
			{
				out_interpolator = safe_static_cast<sample_interpolator_t>(index);
				return true;
			}
		}

		return false;
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
