#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/compiler_utils.h"
#include "acl/math/vector4_32.h"

#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Casts an ACL Vector4_32 into a RTM vector4f.
	// When SIMD intrinsics are enabled, this is a no-op.
	inline rtm::vector4f RTM_SIMD_CALL vector_acl2rtm(Vector4_32Arg0 value)
	{
#if defined(RTM_SSE2_INTRINSICS) || defined(RTM_NEON_INTRINSICS)
		return value;
#else
		return rtm::vector4f{value.x, value.y, value.z, value.w};
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts a RTM vector4f into an ACL Vector4_32.
	// When SIMD intrinsics are enabled, this is a no-op.
	inline Vector4_32 RTM_SIMD_CALL vector_rtm2acl(rtm::vector4f_arg0 value)
	{
#if defined(RTM_SSE2_INTRINSICS) || defined(RTM_NEON_INTRINSICS)
		return value;
#else
		return Vector4_32{value.x, value.y, value.z, value.w};
#endif
	}
}

ACL_IMPL_FILE_PRAGMA_POP
