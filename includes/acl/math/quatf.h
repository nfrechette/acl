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

#include <rtm/quatf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

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

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// The following functions come in packed and scalar form and are specifically
		// crafted to ensure determinism between compression and decompression.
		// The packed functions have a numbered suffix dictating how many elements
		// are processed while scalar functions have the 'stable' suffix.
		//////////////////////////////////////////////////////////////////////////

		// About 31 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL quat_from_positive_w_x4(rtm::vector4f_arg0 xxxx, rtm::vector4f_arg1 yyyy, rtm::vector4f_arg2 zzzz)
		{
			// 1.0 - (x * x)
			rtm::vector4f result = rtm::vector_neg_mul_sub(xxxx, xxxx, rtm::vector_set(1.0F));
			// result - (y * y)
			result = rtm::vector_neg_mul_sub(yyyy, yyyy, result);
			// result - (z * z)
			const rtm::vector4f wwww_squared = rtm::vector_neg_mul_sub(zzzz, zzzz, result);

			// w_squared can be negative either due to rounding or due to quantization imprecision, we take the absolute value
			// to ensure the resulting quaternion is always normalized with a positive W component
			return rtm::vector_sqrt(rtm::vector_abs(wwww_squared));
		}

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
		// Force inline this function, we only use it to keep the code readable
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE __m256 RTM_SIMD_CALL quat_from_positive_w_avx8(__m256 xxxx0_xxxx1, __m256 yyyy0_yyyy1, __m256 zzzz0_zzzz1)
		{
			const __m256 one_v = _mm256_set1_ps(1.0F);

			const __m256 xxxx0_xxxx1_squared = _mm256_mul_ps(xxxx0_xxxx1, xxxx0_xxxx1);
			const __m256 yyyy0_yyyy1_squared = _mm256_mul_ps(yyyy0_yyyy1, yyyy0_yyyy1);
			const __m256 zzzz0_zzzz1_squared = _mm256_mul_ps(zzzz0_zzzz1, zzzz0_zzzz1);

			const __m256 wwww0_wwww1_squared = _mm256_sub_ps(_mm256_sub_ps(_mm256_sub_ps(one_v, xxxx0_xxxx1_squared), yyyy0_yyyy1_squared), zzzz0_zzzz1_squared);

			const __m256i abs_mask = _mm256_set1_epi32(0x7FFFFFFFULL);
			const __m256 wwww0_wwww1_squared_abs = _mm256_and_ps(wwww0_wwww1_squared, _mm256_castsi256_ps(abs_mask));

			return _mm256_sqrt_ps(wwww0_wwww1_squared_abs);
		}
#endif

		// About 28 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL quat_lerp_no_normalization_x4(
			rtm::vector4f_arg0 xxxx0, rtm::vector4f_arg1 yyyy0, rtm::vector4f_arg2 zzzz0, rtm::vector4f_arg3 wwww0,
			rtm::vector4f_arg4 xxxx1, rtm::vector4f_arg5 yyyy1, rtm::vector4f_arg6 zzzz1, rtm::vector4f_arg7 wwww1,
			rtm::vector4f_argn interpolation_alpha,
			rtm::vector4f& interp_xxxx, rtm::vector4f& interp_yyyy, rtm::vector4f& interp_zzzz, rtm::vector4f& interp_wwww)
		{
			// Calculate the vector4 dot product: dot(start, end)
			const rtm::vector4f dot4 = rtm::vector_mul_add(wwww0, wwww1, rtm::vector_mul_add(zzzz0, zzzz1, rtm::vector_mul_add(yyyy0, yyyy1, rtm::vector_mul(xxxx0, xxxx1))));

			// Calculate the bias, if the dot product is positive or zero, there is no bias
			// but if it is negative, we want to flip the 'end' rotation XYZW components
			const rtm::vector4f neg_zero = rtm::vector_set(-0.0F);
			const rtm::vector4f bias = rtm::vector_and(dot4, neg_zero);

			// Apply our bias to the 'end'
			const rtm::vector4f xxxx1_with_bias = rtm::vector_xor(xxxx1, bias);
			const rtm::vector4f yyyy1_with_bias = rtm::vector_xor(yyyy1, bias);
			const rtm::vector4f zzzz1_with_bias = rtm::vector_xor(zzzz1, bias);
			const rtm::vector4f wwww1_with_bias = rtm::vector_xor(wwww1, bias);

			// Lerp the rotation after applying the bias
			// ((1.0 - alpha) * start) + (alpha * (end ^ bias)) == (start - alpha * start) + (alpha * (end ^ bias))
			interp_xxxx = rtm::vector_mul_add(xxxx1_with_bias, interpolation_alpha, rtm::vector_neg_mul_sub(xxxx0, interpolation_alpha, xxxx0));
			interp_yyyy = rtm::vector_mul_add(yyyy1_with_bias, interpolation_alpha, rtm::vector_neg_mul_sub(yyyy0, interpolation_alpha, yyyy0));
			interp_zzzz = rtm::vector_mul_add(zzzz1_with_bias, interpolation_alpha, rtm::vector_neg_mul_sub(zzzz0, interpolation_alpha, zzzz0));
			interp_wwww = rtm::vector_mul_add(wwww1_with_bias, interpolation_alpha, rtm::vector_neg_mul_sub(wwww0, interpolation_alpha, wwww0));
		}

		// About 9 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL quat_normalize_x4(rtm::vector4f& xxxx, rtm::vector4f& yyyy, rtm::vector4f& zzzz, rtm::vector4f& wwww)
		{
			const rtm::vector4f dot4 = rtm::vector_mul_add(wwww, wwww, rtm::vector_mul_add(zzzz, zzzz, rtm::vector_mul_add(yyyy, yyyy, rtm::vector_mul(xxxx, xxxx))));

			const rtm::vector4f len4 = rtm::vector_sqrt(dot4);
			const rtm::vector4f inv_len4 = rtm::vector_div(rtm::vector_set(1.0F), len4);

			xxxx = rtm::vector_mul(xxxx, inv_len4);
			yyyy = rtm::vector_mul(yyyy, inv_len4);
			zzzz = rtm::vector_mul(zzzz, inv_len4);
			wwww = rtm::vector_mul(wwww, inv_len4);
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL quat_mul_x4(
			rtm::vector4f_arg0 lhs_xxxx, rtm::vector4f_arg1 lhs_yyyy, rtm::vector4f_arg2 lhs_zzzz, rtm::vector4f_arg3 lhs_wwww,
			rtm::vector4f_arg4 rhs_xxxx, rtm::vector4f_arg5 rhs_yyyy, rtm::vector4f_arg6 rhs_zzzz, rtm::vector4f_arg7 rhs_wwww,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz, rtm::vector4f& out_result_wwww) RTM_NO_EXCEPT
		{
			out_result_xxxx = rtm::vector_neg_mul_sub(rhs_zzzz, lhs_yyyy, rtm::vector_mul_add(rhs_yyyy, lhs_zzzz, rtm::vector_mul_add(rhs_xxxx, lhs_wwww, rtm::vector_mul(rhs_wwww, lhs_xxxx))));
			out_result_yyyy = rtm::vector_mul_add(rhs_zzzz, lhs_xxxx, rtm::vector_mul_add(rhs_yyyy, lhs_wwww, rtm::vector_neg_mul_sub(rhs_xxxx, lhs_zzzz, rtm::vector_mul(rhs_wwww, lhs_yyyy))));
			out_result_zzzz = rtm::vector_mul_add(rhs_zzzz, lhs_wwww, rtm::vector_neg_mul_sub(rhs_yyyy, lhs_xxxx, rtm::vector_mul_add(rhs_xxxx, lhs_yyyy, rtm::vector_mul(rhs_wwww, lhs_zzzz))));
			out_result_wwww = rtm::vector_neg_mul_sub(rhs_zzzz, lhs_zzzz, rtm::vector_neg_mul_sub(rhs_yyyy, lhs_yyyy, rtm::vector_neg_mul_sub(rhs_xxxx, lhs_xxxx, rtm::vector_mul(rhs_wwww, lhs_wwww))));
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE void RTM_SIMD_CALL quat_mul_vector3_x4(
			rtm::vector4f_arg0 vector_xxxx, rtm::vector4f_arg1 vector_yyyy, rtm::vector4f_arg2 vector_zzzz,
			rtm::quatf_arg3 rotation,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) RTM_NO_EXCEPT
		{
			// AoS equivalent
			// quatf vector_quat = quat_set_w(vector_to_quat(vector), 0.0f);
			// quatf inv_rotation = quat_conjugate(rotation);
			// return quat_to_vector(quat_mul(quat_mul(inv_rotation, vector_quat), rotation));

			rtm::quatf inv_rotation = rtm::quat_conjugate(rotation);

			rtm::vector4f inv_rotation_v = rtm::quat_to_vector(inv_rotation);
			rtm::vector4f inv_rotation_xxxx = rtm::vector_dup_x(inv_rotation_v);
			rtm::vector4f inv_rotation_yyyy = rtm::vector_dup_y(inv_rotation_v);
			rtm::vector4f inv_rotation_zzzz = rtm::vector_dup_z(inv_rotation_v);
			rtm::vector4f inv_rotation_wwww = rtm::vector_dup_w(inv_rotation_v);

			rtm::vector4f tmp_xxxx;
			rtm::vector4f tmp_yyyy;
			rtm::vector4f tmp_zzzz;
			rtm::vector4f tmp_wwww;

			// We know that vector_wwww is zero and so we can cut down a few operations
			// rtm::vector4f vector_wwww = rtm::vector_zero();
			// quat_mul_x4(
			//	 inv_rotation_xxxx, inv_rotation_yyyy, inv_rotation_zzzz, inv_rotation_wwww,
			//	 vector_xxxx, vector_yyyy, vector_zzzz, vector_wwww,
			//	 tmp_xxxx, tmp_yyyy, tmp_zzzz, tmp_wwww);
			//
			// Inlined and optimized
			// tmp_xxxx = rtm::vector_neg_mul_sub(vector_zzzz, inv_rotation_yyyy, rtm::vector_mul_add(vector_yyyy, inv_rotation_zzzz, rtm::vector_mul(vector_xxxx, inv_rotation_wwww)));
			// tmp_yyyy = rtm::vector_mul_add(vector_zzzz, inv_rotation_xxxx, rtm::vector_mul_add(vector_yyyy, inv_rotation_wwww, rtm::vector_mul(vector_xxxx, inv_rotation_zzzz)));
			// tmp_zzzz = rtm::vector_mul_add(vector_zzzz, inv_rotation_wwww, rtm::vector_neg_mul_sub(vector_yyyy, inv_rotation_xxxx, rtm::vector_mul(vector_xxxx, inv_rotation_yyyy)));
			// tmp_wwww = rtm::vector_neg_mul_sub(vector_zzzz, inv_rotation_zzzz, rtm::vector_neg_mul_sub(vector_yyyy, inv_rotation_yyyy, rtm::vector_mul(vector_xxxx, inv_rotation_xxxx)));
			//
			// Hand optimize because Apple clang struggles to re-order the instructions
			// to minimize dependencies. Ideally, we want independent instructions to dispatch
			// together to facilitate simultaneous execution.
			tmp_xxxx = rtm::vector_mul(vector_xxxx, inv_rotation_wwww);
			tmp_yyyy = rtm::vector_mul(vector_xxxx, inv_rotation_zzzz);
			tmp_zzzz = rtm::vector_mul(vector_xxxx, inv_rotation_yyyy);
			tmp_wwww = rtm::vector_mul(vector_xxxx, inv_rotation_xxxx);

			tmp_xxxx = rtm::vector_mul_add(vector_yyyy, inv_rotation_zzzz, tmp_xxxx);
			tmp_yyyy = rtm::vector_mul_add(vector_yyyy, inv_rotation_wwww, tmp_yyyy);
			tmp_zzzz = rtm::vector_neg_mul_sub(vector_yyyy, inv_rotation_xxxx, tmp_zzzz);
			tmp_wwww = rtm::vector_neg_mul_sub(vector_yyyy, inv_rotation_yyyy, tmp_wwww);

			tmp_xxxx = rtm::vector_neg_mul_sub(vector_zzzz, inv_rotation_yyyy, tmp_xxxx);
			tmp_yyyy = rtm::vector_mul_add(vector_zzzz, inv_rotation_xxxx, tmp_yyyy);
			tmp_zzzz = rtm::vector_mul_add(vector_zzzz, inv_rotation_wwww, tmp_zzzz);
			tmp_wwww = rtm::vector_neg_mul_sub(vector_zzzz, inv_rotation_zzzz, tmp_wwww);

			rtm::vector4f rotation_v = rtm::quat_to_vector(rotation);
			rtm::vector4f rotation_xxxx = rtm::vector_dup_x(rotation_v);
			rtm::vector4f rotation_yyyy = rtm::vector_dup_y(rotation_v);
			rtm::vector4f rotation_zzzz = rtm::vector_dup_z(rotation_v);
			rtm::vector4f rotation_wwww = rtm::vector_dup_w(rotation_v);

			// We know that result_wwww is discarded and so we can cut down a few operations
			// quat_mul_x4(
			//	 tmp_xxxx, tmp_yyyy, tmp_zzzz, tmp_wwww,
			//	 rotation_xxxx, rotation_yyyy, rotation_zzzz, rotation_wwww,
			//	 out_result_xxxx, out_result_yyyy, out_result_zzzz, tmp_wwww);
			//
			// Inlined and optimized
			// out_result_xxxx = rtm::vector_neg_mul_sub(rotation_zzzz, tmp_yyyy, rtm::vector_mul_add(rotation_yyyy, tmp_zzzz, rtm::vector_mul_add(rotation_xxxx, tmp_wwww, rtm::vector_mul(rotation_wwww, tmp_xxxx))));
			// out_result_yyyy = rtm::vector_mul_add(rotation_zzzz, tmp_xxxx, rtm::vector_mul_add(rotation_yyyy, tmp_wwww, rtm::vector_neg_mul_sub(rotation_xxxx, tmp_zzzz, rtm::vector_mul(rotation_wwww, tmp_yyyy))));
			// out_result_zzzz = rtm::vector_mul_add(rotation_zzzz, tmp_wwww, rtm::vector_neg_mul_sub(rotation_yyyy, tmp_xxxx, rtm::vector_mul_add(rotation_xxxx, tmp_yyyy, rtm::vector_mul(rotation_wwww, tmp_zzzz))));
			//
			// Hand optimize because Apple clang struggles to re-order the instructions
			// to minimize dependencies. Ideally, we want independent instructions to dispatch
			// together to facilitate simultaneous execution.
			rtm::vector4f result_xxxx = rtm::vector_mul(rotation_wwww, tmp_xxxx);
			rtm::vector4f result_yyyy = rtm::vector_mul(rotation_wwww, tmp_yyyy);
			rtm::vector4f result_zzzz = rtm::vector_mul(rotation_wwww, tmp_zzzz);

			result_xxxx = rtm::vector_mul_add(rotation_xxxx, tmp_wwww, result_xxxx);
			result_yyyy = rtm::vector_neg_mul_sub(rotation_xxxx, tmp_zzzz, result_yyyy);
			result_zzzz = rtm::vector_mul_add(rotation_xxxx, tmp_yyyy, result_zzzz);

			result_xxxx = rtm::vector_mul_add(rotation_yyyy, tmp_zzzz, result_xxxx);
			result_yyyy = rtm::vector_mul_add(rotation_yyyy, tmp_wwww, result_yyyy);
			result_zzzz = rtm::vector_neg_mul_sub(rotation_yyyy, tmp_xxxx, result_zzzz);

			out_result_xxxx = rtm::vector_neg_mul_sub(rotation_zzzz, tmp_yyyy, result_xxxx);
			out_result_yyyy = rtm::vector_mul_add(rotation_zzzz, tmp_xxxx, result_yyyy);
			out_result_zzzz = rtm::vector_mul_add(rotation_zzzz, tmp_wwww, result_zzzz);
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE rtm::scalarf RTM_SIMD_CALL vector_dot_stable(rtm::vector4f_arg0 input0, rtm::vector4f_arg1 input1) RTM_NO_EXCEPT
		{
			// SIMD NEON uses fused multiply-accumulate, we need to make sure to use it with the scalar version as well
#if defined(RTM_NEON_INTRINSICS)
			const rtm::scalarf x0 = rtm::vector_get_x_as_scalar(input0);
			const rtm::scalarf y0 = rtm::vector_get_y_as_scalar(input0);
			const rtm::scalarf z0 = rtm::vector_get_z_as_scalar(input0);
			const rtm::scalarf w0 = rtm::vector_get_w_as_scalar(input0);

			const rtm::scalarf x1 = rtm::vector_get_x_as_scalar(input1);
			const rtm::scalarf y1 = rtm::vector_get_y_as_scalar(input1);
			const rtm::scalarf z1 = rtm::vector_get_z_as_scalar(input1);
			const rtm::scalarf w1 = rtm::vector_get_w_as_scalar(input1);

			const rtm::scalarf dot = rtm::scalar_mul_add(w0, w1, rtm::scalar_mul_add(z0, z1, rtm::scalar_mul_add(y0, y1, rtm::scalar_mul(x0, x1))));
#else
			const rtm::vector4f input0_mul_input1 = rtm::vector_mul(input0, input1);

			rtm::scalarf dot = rtm::vector_get_x_as_scalar(input0_mul_input1);
			dot = rtm::scalar_add(dot, rtm::vector_get_y_as_scalar(input0_mul_input1));
			dot = rtm::scalar_add(dot, rtm::vector_get_z_as_scalar(input0_mul_input1));
			dot = rtm::scalar_add(dot, rtm::vector_get_w_as_scalar(input0_mul_input1));
#endif

			return dot;
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE rtm::quatf RTM_SIMD_CALL quat_from_positive_w_stable(rtm::vector4f_arg0 input) RTM_NO_EXCEPT
		{
			const rtm::scalarf x = rtm::vector_get_x_as_scalar(input);
			const rtm::scalarf y = rtm::vector_get_y_as_scalar(input);
			const rtm::scalarf z = rtm::vector_get_z_as_scalar(input);

			// 1.0 - (x * x)
			rtm::scalarf result = rtm::scalar_neg_mul_sub(x, x, rtm::scalar_set(1.0F));
			// result - (y * y)
			result = rtm::scalar_neg_mul_sub(y, y, result);
			// result - (z * z)
			const rtm::scalarf w_squared = rtm::scalar_neg_mul_sub(z, z, result);

			// w_squared can be negative either due to rounding or due to quantization imprecision, we take the absolute value
			// to ensure the resulting quaternion is always normalized with a positive W component
			const rtm::scalarf w = rtm::scalar_sqrt(rtm::scalar_abs(w_squared));
			return rtm::quat_set_w(rtm::vector_to_quat(input), w);
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_INLINE rtm::quatf RTM_SIMD_CALL quat_normalize_stable(rtm::quatf_arg0 input) RTM_NO_EXCEPT
		{
			rtm::vector4f input_v = rtm::quat_to_vector(input);

			rtm::scalarf dot = vector_dot_stable(input_v, input_v);

			rtm::scalarf inv_len = rtm::scalar_div(rtm::scalar_set(1.0F),  rtm::scalar_sqrt(dot));
			return rtm::vector_to_quat(rtm::vector_mul(input_v, inv_len));
		}

		RTM_DISABLE_SECURITY_COOKIE_CHECK inline rtm::quatf RTM_SIMD_CALL quat_lerp_stable(rtm::quatf_arg0 start, rtm::quatf_arg1 end, float alpha) RTM_NO_EXCEPT
		{
			rtm::vector4f start_v = rtm::quat_to_vector(start);
			rtm::vector4f end_v = rtm::quat_to_vector(end);

			rtm::scalarf dot = vector_dot_stable(start_v, end_v);

			rtm::scalarf bias = rtm::scalar_set(rtm::scalar_cast(dot) >= 0.0F ? 1.0F : -1.0F);

			// ((1.0 - alpha) * start) + (alpha * (end * bias)) == (start - alpha * start) + (alpha * (end * bias))
			rtm::vector4f interpolated_rotation = rtm::vector_mul_add(rtm::vector_mul(end_v, bias), alpha, rtm::vector_neg_mul_sub(start_v, alpha, start_v));

			return quat_normalize_stable(rtm::vector_to_quat(interpolated_rotation));
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
