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

#include <rtm/matrix3x4f.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// Temporary put here until they are included in RTM
	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Multiplies a 3x4 affine matrix and four 3D points stored transposed.
		// Multiplication order is as follow: world_position = matrix_mul_point3_x4(local_position, local_to_world)
		// Each lane uses the same operations in the same order as rtm::matrix_mul_point3.
		// Thus each lane gives the same result, unless the compiler merges a multiply and an add (FMA) in only one of the two.
		//////////////////////////////////////////////////////////////////////////
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL matrix_mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			rtm::matrix3x4f_argn mtx,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) RTM_NO_EXCEPT
		{
			// AoS equivalent
			// tmp0 = x * x_axis + y * y_axis
			// tmp1 = z * z_axis + w_axis
			// return tmp0 + tmp1

			rtm::vector4f tmp0_xxxx = rtm::vector_mul(point_xxxx, rtm::vector_dup_x(mtx.x_axis));
			rtm::vector4f tmp0_yyyy = rtm::vector_mul(point_xxxx, rtm::vector_dup_y(mtx.x_axis));
			rtm::vector4f tmp0_zzzz = rtm::vector_mul(point_xxxx, rtm::vector_dup_z(mtx.x_axis));

			tmp0_xxxx = rtm::vector_mul_add(point_yyyy, rtm::vector_dup_x(mtx.y_axis), tmp0_xxxx);
			tmp0_yyyy = rtm::vector_mul_add(point_yyyy, rtm::vector_dup_y(mtx.y_axis), tmp0_yyyy);
			tmp0_zzzz = rtm::vector_mul_add(point_yyyy, rtm::vector_dup_z(mtx.y_axis), tmp0_zzzz);

			const rtm::vector4f tmp1_xxxx = rtm::vector_mul_add(point_zzzz, rtm::vector_dup_x(mtx.z_axis), rtm::vector_dup_x(mtx.w_axis));
			const rtm::vector4f tmp1_yyyy = rtm::vector_mul_add(point_zzzz, rtm::vector_dup_y(mtx.z_axis), rtm::vector_dup_y(mtx.w_axis));
			const rtm::vector4f tmp1_zzzz = rtm::vector_mul_add(point_zzzz, rtm::vector_dup_z(mtx.z_axis), rtm::vector_dup_z(mtx.w_axis));

			out_result_xxxx = rtm::vector_add(tmp0_xxxx, tmp1_xxxx);
			out_result_yyyy = rtm::vector_add(tmp0_yyyy, tmp1_yyyy);
			out_result_zzzz = rtm::vector_add(tmp0_zzzz, tmp1_zzzz);
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
