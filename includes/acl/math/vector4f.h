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

#include "acl/core/impl/compiler_utils.h"

#include <rtm/vector4f.h>

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	// Temporary put here until they are included in RTM
	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Per component logical AND between the inputs: input0 & input1
		//////////////////////////////////////////////////////////////////////////
		inline rtm::vector4f RTM_SIMD_CALL vector_and(rtm::vector4f_arg0 input0, rtm::vector4f_arg1 input1) RTM_NO_EXCEPT
		{
#if defined(RTM_SSE2_INTRINSICS)
			return _mm_and_ps(input0, input1);
#elif defined(RTM_NEON_INTRINSICS)
			return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(input0), vreinterpretq_u32_f32(input1)));
#else
			uint32_t input0_[4];
			uint32_t input1_[4];

			static_assert(sizeof(rtm::vector4f) == sizeof(input0_), "Unexpected size");
			std::memcpy(&input0_[0], &input0, sizeof(rtm::vector4f));
			std::memcpy(&input1_[0], &input1, sizeof(rtm::vector4f));

			uint32_t result_[4];
			result_[0] = input0_[0] & input1_[0];
			result_[1] = input0_[1] & input1_[1];
			result_[2] = input0_[2] & input1_[2];
			result_[3] = input0_[3] & input1_[3];

			rtm::vector4f result;
			std::memcpy(&result, &result_[0], sizeof(rtm::vector4f));

			return result;
#endif
		}

		//////////////////////////////////////////////////////////////////////////
		// Per component logical XOR between the inputs: input0 ^ input1
		//////////////////////////////////////////////////////////////////////////
		inline rtm::vector4f RTM_SIMD_CALL vector_xor(rtm::vector4f_arg0 input0, rtm::vector4f_arg1 input1) RTM_NO_EXCEPT
		{
#if defined(RTM_SSE2_INTRINSICS)
			return _mm_xor_ps(input0, input1);
#elif defined(RTM_NEON_INTRINSICS)
			return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(input0), vreinterpretq_u32_f32(input1)));
#else
			uint32_t input0_[4];
			uint32_t input1_[4];

			static_assert(sizeof(rtm::vector4f) == sizeof(input0_), "Unexpected size");
			std::memcpy(&input0_[0], &input0, sizeof(rtm::vector4f));
			std::memcpy(&input1_[0], &input1, sizeof(rtm::vector4f));

			uint32_t result_[4];
			result_[0] = input0_[0] ^ input1_[0];
			result_[1] = input0_[1] ^ input1_[1];
			result_[2] = input0_[2] ^ input1_[2];
			result_[3] = input0_[3] ^ input1_[3];

			rtm::vector4f result;
			std::memcpy(&result, &result_[0], sizeof(rtm::vector4f));

			return result;
#endif
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
