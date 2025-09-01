#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

#include <rtm/vector4d.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// Temporary put here until they are included in RTM
	namespace acl_impl
	{
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL vector_dot3_x4(
			rtm::vector4f_arg0 lhs_xxxx, rtm::vector4f_arg1 lhs_yyyy, rtm::vector4f_arg2 lhs_zzzz,
			rtm::vector4f_arg3 rhs_xxxx, rtm::vector4f_arg4 rhs_yyyy, rtm::vector4f_arg5 rhs_zzzz) RTM_NO_EXCEPT
		{
			rtm::vector4f tmp_xxxx = rtm::vector_mul(lhs_xxxx, rhs_xxxx);
			rtm::vector4f tmp_yyyy = rtm::vector_mul(lhs_yyyy, rhs_yyyy);
			rtm::vector4f tmp_zzzz = rtm::vector_mul(lhs_zzzz, rhs_zzzz);

			return rtm::vector_add(rtm::vector_add(tmp_xxxx, tmp_yyyy), tmp_zzzz);
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL vector_distance_squared3_x4(
			rtm::vector4f_arg0 lhs_xxxx, rtm::vector4f_arg1 lhs_yyyy, rtm::vector4f_arg2 lhs_zzzz,
			rtm::vector4f_arg3 rhs_xxxx, rtm::vector4f_arg4 rhs_yyyy, rtm::vector4f_arg5 rhs_zzzz) RTM_NO_EXCEPT
		{
			// AoS equivalent
			// const rtm::vector4f difference = rtm::vector_sub(lhs, rhs);
			// return rtm::vector_length_squared3_as_scalar(difference);

			rtm::vector4f difference_xxxx = rtm::vector_sub(lhs_xxxx, rhs_xxxx);
			rtm::vector4f difference_yyyy = rtm::vector_sub(lhs_yyyy, rhs_yyyy);
			rtm::vector4f difference_zzzz = rtm::vector_sub(lhs_zzzz, rhs_zzzz);

			return vector_dot3_x4(
				difference_xxxx, difference_yyyy, difference_zzzz,
				difference_xxxx, difference_yyyy, difference_zzzz);
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
