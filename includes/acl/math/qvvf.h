#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2022 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/math/quatf.h"

#include <rtm/qvvf.h>

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// Temporary put here until they are included in RTM
	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Multiplies a QVV transform and a 3D point.
		// Multiplication order is as follow: world_position = qvv_mul_point3(local_position, local_to_world)
		//////////////////////////////////////////////////////////////////////////
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL qvv_mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			rtm::qvvf_argn qvv,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) RTM_NO_EXCEPT
		{
			// AoS equivalent
			// return rtm::vector_add(rtm::quat_mul_vector3(rtm::vector_mul(qvv.scale, point), qvv.rotation), qvv.translation);

			rtm::vector4f result_xxxx = rtm::vector_mul(point_xxxx, rtm::vector_dup_x(qvv.scale));
			rtm::vector4f result_yyyy = rtm::vector_mul(point_yyyy, rtm::vector_dup_y(qvv.scale));
			rtm::vector4f result_zzzz = rtm::vector_mul(point_zzzz, rtm::vector_dup_z(qvv.scale));

			quat_mul_vector3_x4(
				result_xxxx, result_yyyy, result_zzzz,
				qvv.rotation,
				result_xxxx, result_yyyy, result_zzzz);

			out_result_xxxx = rtm::vector_add(result_xxxx, rtm::vector_dup_x(qvv.translation));
			out_result_yyyy = rtm::vector_add(result_yyyy, rtm::vector_dup_y(qvv.translation));
			out_result_zzzz = rtm::vector_add(result_zzzz, rtm::vector_dup_z(qvv.translation));
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL qvv_mul_point3_no_scale_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			rtm::qvvf_argn qvv,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) RTM_NO_EXCEPT
		{
			// AoS equivalent
			// return rtm::vector_add(rtm::quat_mul_vector3(point, qvv.rotation), qvv.translation);

			rtm::vector4f result_xxxx;
			rtm::vector4f result_yyyy;
			rtm::vector4f result_zzzz;
			quat_mul_vector3_x4(
				point_xxxx, point_yyyy, point_zzzz,
				qvv.rotation,
				result_xxxx, result_yyyy, result_zzzz);

			out_result_xxxx = rtm::vector_add(result_xxxx, rtm::vector_dup_x(qvv.translation));
			out_result_yyyy = rtm::vector_add(result_yyyy, rtm::vector_dup_y(qvv.translation));
			out_result_zzzz = rtm::vector_add(result_zzzz, rtm::vector_dup_z(qvv.translation));
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
