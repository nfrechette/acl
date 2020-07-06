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

#include <rtm/quatf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	RTM_DISABLE_SECURITY_COOKIE_CHECK inline rtm::quatf RTM_SIMD_CALL quat_lerp_no_normalization(rtm::quatf_arg0 start, rtm::quatf_arg1 end, float alpha) RTM_NO_EXCEPT
	{
		using namespace rtm;

#if defined(RTM_SSE2_INTRINSICS)
		// Calculate the vector4 dot product: dot(start, end)
		__m128 dot;
#if defined(RTM_SSE4_INTRINSICS)
		// The dpps instruction isn't as accurate but we don't care here, we only need the sign of the
		// dot product. If both rotations are on opposite ends of the hypersphere, the result will be
		// very negative. If we are on the edge, the rotations are nearly opposite but not quite which
		// means that the linear interpolation here will have terrible accuracy to begin with. It is designed
		// for interpolating rotations that are reasonably close together. The bias check is mainly necessary
		// because the W component is often kept positive which flips the sign.
		// Using the dpps instruction reduces the number of registers that we need and helps the function get
		// inlined.
		dot = _mm_dp_ps(start, end, 0xFF);
#else
		{
			__m128 x2_y2_z2_w2 = _mm_mul_ps(start, end);
			__m128 z2_w2_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 3, 2));
			__m128 x2z2_y2w2_0_0 = _mm_add_ps(x2_y2_z2_w2, z2_w2_0_0);
			__m128 y2w2_0_0_0 = _mm_shuffle_ps(x2z2_y2w2_0_0, x2z2_y2w2_0_0, _MM_SHUFFLE(0, 0, 0, 1));
			__m128 x2y2z2w2_0_0_0 = _mm_add_ps(x2z2_y2w2_0_0, y2w2_0_0_0);
			// Shuffle the dot product to all SIMD lanes, there is no _mm_and_ss and loading
			// the constant from memory with the 'and' instruction is faster, it uses fewer registers
			// and fewer instructions
			dot = _mm_shuffle_ps(x2y2z2w2_0_0_0, x2y2z2w2_0_0_0, _MM_SHUFFLE(0, 0, 0, 0));
		}
#endif

		// Calculate the bias, if the dot product is positive or zero, there is no bias
		// but if it is negative, we want to flip the 'end' rotation XYZW components
		__m128 bias = _mm_and_ps(dot, _mm_set_ps1(-0.0F));

		// Lerp the rotation after applying the bias
		// ((1.0 - alpha) * start) + (alpha * (end ^ bias)) == (start - alpha * start) + (alpha * (end ^ bias))
		__m128 alpha_ = _mm_set_ps1(alpha);
		__m128 interpolated_rotation = _mm_add_ps(_mm_sub_ps(start, _mm_mul_ps(alpha_, start)), _mm_mul_ps(alpha_, _mm_xor_ps(end, bias)));

		// Due to the interpolation, the result might not be anywhere near normalized!
		// Make sure to normalize afterwards before using
		return interpolated_rotation;
#elif defined (RTM_NEON64_INTRINSICS)
		// On ARM64 with NEON, we load 1.0 once and use it twice which is faster than
		// using a AND/XOR with the bias (same number of instructions)
		float dot = vector_dot(start, end);
		float bias = dot >= 0.0F ? 1.0F : -1.0F;

		// ((1.0 - alpha) * start) + (alpha * (end * bias)) == (start - alpha * start) + (alpha * (end * bias))
		vector4f interpolated_rotation = vector_mul_add(vector_mul(end, bias), alpha, vector_neg_mul_sub(start, alpha, start));

		// Due to the interpolation, the result might not be anywhere near normalized!
		// Make sure to normalize afterwards before using
		return interpolated_rotation;
#elif defined(RTM_NEON_INTRINSICS)
		// Calculate the vector4 dot product: dot(start, end)
		float32x4_t x2_y2_z2_w2 = vmulq_f32(start, end);
		float32x2_t x2_y2 = vget_low_f32(x2_y2_z2_w2);
		float32x2_t z2_w2 = vget_high_f32(x2_y2_z2_w2);
		float32x2_t x2z2_y2w2 = vadd_f32(x2_y2, z2_w2);
		float32x2_t x2y2z2w2 = vpadd_f32(x2z2_y2w2, x2z2_y2w2);

		// Calculate the bias, if the dot product is positive or zero, there is no bias
		// but if it is negative, we want to flip the 'end' rotation XYZW components
		// On ARM-v7-A, the AND/XOR trick is faster than the cmp/fsel
		uint32x2_t bias = vand_u32(vreinterpret_u32_f32(x2y2z2w2), vdup_n_u32(0x80000000));

		// Lerp the rotation after applying the bias
		// ((1.0 - alpha) * start) + (alpha * (end ^ bias)) == (start - alpha * start) + (alpha * (end ^ bias))
		float32x4_t end_biased = vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(end), vcombine_u32(bias, bias)));
		float32x4_t interpolated_rotation = vmlaq_n_f32(vmlsq_n_f32(start, start, alpha), end_biased, alpha);

		// Due to the interpolation, the result might not be anywhere near normalized!
		// Make sure to normalize afterwards before using
		return interpolated_rotation;
#else
		// To ensure we take the shortest path, we apply a bias if the dot product is negative
		vector4f start_vector = quat_to_vector(start);
		vector4f end_vector = quat_to_vector(end);
		float dot = vector_dot(start_vector, end_vector);
		float bias = dot >= 0.0F ? 1.0F : -1.0F;
		// ((1.0 - alpha) * start) + (alpha * (end * bias)) == (start - alpha * start) + (alpha * (end * bias))
		vector4f interpolated_rotation = vector_mul_add(vector_mul(end_vector, bias), alpha, vector_neg_mul_sub(start_vector, alpha, start_vector));

		// Due to the interpolation, the result might not be anywhere near normalized!
		// Make sure to normalize afterwards before using
		return vector_to_quat(interpolated_rotation);
#endif
	}
}

ACL_IMPL_FILE_PRAGMA_POP
