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
#include "acl/math/quatf.h"

#include <rtm/matrix3x3f.h>
#include <rtm/vqmf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// Temporary put here until they are included in RTM
	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Multiplies a VQM transform and four 3D points stored transposed.
		// Multiplication order is as follow: world_position = vqm_mul_point3_x4(local_position, local_to_world)
		//////////////////////////////////////////////////////////////////////////
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL vqm_mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			rtm::vqmf_argn vqm,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) RTM_NO_EXCEPT
		{
			// AoS equivalent
			// return rtm::vqm_mul_point3(point, vqm);
			// p' = p * M * R + t

			rtm::vector4f result_xxxx;
			rtm::vector4f result_yyyy;
			rtm::vector4f result_zzzz;

			if (rtm::rtm_impl::vqm_has_identity_scale_shear(vqm))
			{
				// The flag tells that M is exactly the identity, thus p * M = p
				// This is the case for each transform without scale, we skip the matrix product
				result_xxxx = point_xxxx;
				result_yyyy = point_yyyy;
				result_zzzz = point_zzzz;
			}
			else
			{
				// The row vector p multiplies M: p * M = (p.x * M.x_axis) + (p.y * M.y_axis) + (p.z * M.z_axis)
				const rtm::matrix3x3f scale_shear = rtm::rtm_impl::vqm_get_scale_shear_no_flag(vqm);

				result_xxxx = rtm::vector_mul(point_xxxx, rtm::vector_dup_x(scale_shear.x_axis));
				result_yyyy = rtm::vector_mul(point_xxxx, rtm::vector_dup_y(scale_shear.x_axis));
				result_zzzz = rtm::vector_mul(point_xxxx, rtm::vector_dup_z(scale_shear.x_axis));

				result_xxxx = rtm::vector_mul_add(point_yyyy, rtm::vector_dup_x(scale_shear.y_axis), result_xxxx);
				result_yyyy = rtm::vector_mul_add(point_yyyy, rtm::vector_dup_y(scale_shear.y_axis), result_yyyy);
				result_zzzz = rtm::vector_mul_add(point_yyyy, rtm::vector_dup_z(scale_shear.y_axis), result_zzzz);

				result_xxxx = rtm::vector_mul_add(point_zzzz, rtm::vector_dup_x(scale_shear.z_axis), result_xxxx);
				result_yyyy = rtm::vector_mul_add(point_zzzz, rtm::vector_dup_y(scale_shear.z_axis), result_yyyy);
				result_zzzz = rtm::vector_mul_add(point_zzzz, rtm::vector_dup_z(scale_shear.z_axis), result_zzzz);
			}

			quat_mul_vector3_x4(
				result_xxxx, result_yyyy, result_zzzz,
				vqm.rotation,
				result_xxxx, result_yyyy, result_zzzz);

			// The [xyz] components of translation_m22 hold the translation
			const rtm::vector4f translation = rtm::vqm_get_translation(vqm);
			out_result_xxxx = rtm::vector_add(result_xxxx, rtm::vector_dup_x(translation));
			out_result_yyyy = rtm::vector_add(result_yyyy, rtm::vector_dup_y(translation));
			out_result_zzzz = rtm::vector_add(result_zzzz, rtm::vector_dup_z(translation));
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
