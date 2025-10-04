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

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// This enum dictates what interpolation function is used when sampling.
	enum class sample_interpolator_t : uint8_t
	{
		//////////////////////////////////////////////////////////////////////////
		// Constant interpolation means that if we sample between value X and X+1,
		// X is returned within that interval.
		constant = 0,

		//////////////////////////////////////////////////////////////////////////
		// Linear interpolation, classic lerp(a, b, alpha) = a + ((b - a) * alpha)
		linear = 1,

		//////////////////////////////////////////////////////////////////////////
		// BELOW VALUES NOT IMPLEMENTED YET
		// See here for details and notes: https://github.com/nfrechette/acl/issues/526
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// Cubic Bezier interpolation
		//cubic_bezier = 2,

		//////////////////////////////////////////////////////////////////////////
		// Cubic Hermite interpolation
		//cubic_hermite = 3,

		//////////////////////////////////////////////////////////////////////////
		// Catmull-Rom interpolation
		//catmull_rom = 4,
	};

	// Returns the string representation for the provided sample interpolator.
	// TODO: constexpr
	const char* get_sample_interpolator_name(sample_interpolator_t interpolator);

	// Returns the sample interpolator from its string representation.
	// Returns true on success, false otherwise.
	bool get_sample_interpolator(const char* interpolator, sample_interpolator_t& out_interpolator);

	ACL_IMPL_VERSION_NAMESPACE_END
}

#include "acl/core/impl/sample_interpolator.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
