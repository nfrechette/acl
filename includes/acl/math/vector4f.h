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

#include <rtm/vector4f.h>

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// Temporary put here until they are included in RTM
	namespace acl_impl
	{
		// TODO: Add to RTM
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE bool RTM_SIMD_CALL vector_all_equal(rtm::vector4f_arg0 lhs, rtm::vector4f_arg1 rhs) RTM_NO_EXCEPT
		{
#if defined(RTM_SSE2_INTRINSICS)
			return _mm_movemask_ps(_mm_cmpeq_ps(lhs, rhs)) == 0xF;
#elif defined(RTM_NEON_INTRINSICS)
			uint8x16_t mask = vreinterpretq_u8_u32(vceqq_f32(lhs, rhs));
			uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
			uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(vreinterpret_u16_u8(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0]), vreinterpret_u16_u8(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]));
			return vget_lane_u32(vreinterpret_u32_u16(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0]), 0) == 0xFFFFFFFFU;
#else
			return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
#endif
		}

		// TODO: Add to RTM
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE bool RTM_SIMD_CALL vector_all_equal3(rtm::vector4f_arg0 lhs, rtm::vector4f_arg1 rhs) RTM_NO_EXCEPT
		{
#if defined(RTM_SSE2_INTRINSICS)
			return (_mm_movemask_ps(_mm_cmpeq_ps(lhs, rhs)) & 0x7) == 0x7;
#elif defined(RTM_NEON_INTRINSICS)
			uint8x16_t mask = vreinterpretq_u8_u32(vceqq_f32(lhs, rhs));
			uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
			uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(vreinterpret_u16_u8(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0]), vreinterpret_u16_u8(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]));
			return (vget_lane_u32(vreinterpret_u32_u16(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0]), 0) & 0x00FFFFFFU) == 0x00FFFFFFU;
#else
			return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
#endif
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
