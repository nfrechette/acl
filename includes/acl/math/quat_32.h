#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/math/math.h"
#include "acl/math/scalar_32.h"
#include "acl/math/vector4_32.h"

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Setters, getters, and casts

	inline Quat_32 quat_set(float x, float y, float z, float w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_set_ps(w, z, y, x);
#elif defined(ACL_NEON_INTRINSICS)
#if 1
		float32x2_t V0 = vcreate_f32(((uint64_t)*(const uint32_t*)&x) | ((uint64_t)(*(const uint32_t*)&y) << 32));
		float32x2_t V1 = vcreate_f32(((uint64_t)*(const uint32_t*)&z) | ((uint64_t)(*(const uint32_t*)&w) << 32));
		return vcombine_f32(V0, V1);
#else
		float __attribute__((aligned(16))) data[4] = { x, y, z, w };
		return vld1q_f32(data);
#endif
#else
		return Quat_32{ x, y, z, w };
#endif
	}

	inline Quat_32 quat_unaligned_load(const float* input)
	{
		ACL_ASSERT(is_aligned(input), "Invalid alignment");
		return quat_set(input[0], input[1], input[2], input[3]);
	}

	inline Quat_32 quat_identity_32()
	{
		return quat_set(0.0f, 0.0f, 0.0f, 1.0f);
	}

	inline Quat_32 vector_to_quat(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS) || defined(ACL_NEON_INTRINSICS)
		return input;
#else
		return Quat_32{ input.x, input.y, input.z, input.w };
#endif
	}

	inline Quat_32 quat_cast(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_shuffle_ps(_mm_cvtpd_ps(input.xy), _mm_cvtpd_ps(input.zw), _MM_SHUFFLE(1, 0, 1, 0));
#else
		return Quat_32{ float(input.x), float(input.y), float(input.z), float(input.w) };
#endif
	}

	inline float quat_get_x(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(input);
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 0);
#else
		return input.x;
#endif
	}

	inline float quat_get_y(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1)));
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 1);
#else
		return input.y;
#endif
	}

	inline float quat_get_z(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2)));
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 2);
#else
		return input.z;
#endif
	}

	inline float quat_get_w(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3)));
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 3);
#else
		return input.w;
#endif
	}

	inline void quat_unaligned_write(const Quat_32& input, float* output)
	{
		ACL_ASSERT(is_aligned(output), "Invalid alignment");
		output[0] = quat_get_x(input);
		output[1] = quat_get_y(input);
		output[2] = quat_get_z(input);
		output[3] = quat_get_w(input);
	}

	//////////////////////////////////////////////////////////////////////////
	// Arithmetic

	inline Quat_32 quat_conjugate(const Quat_32& input)
	{
		return quat_set(-quat_get_x(input), -quat_get_y(input), -quat_get_z(input), quat_get_w(input));
	}

	// Multiplication order is as follow: local_to_world = quat_mul(local_to_object, object_to_world)
	inline Quat_32 quat_mul(const Quat_32& lhs, const Quat_32& rhs)
	{
#if defined(ACL_SSE4_INTRINSICS) && 0
		// TODO: Profile this, the accuracy is the same as with SSE2, should be binary exact
		constexpr __m128 signs_x = { 1.0f,  1.0f,  1.0f, -1.0f };
		constexpr __m128 signs_y = { 1.0f, -1.0f,  1.0f,  1.0f };
		constexpr __m128 signs_z = { 1.0f,  1.0f, -1.0f,  1.0f };
		constexpr __m128 signs_w = { 1.0f, -1.0f, -1.0f, -1.0f };
		// x = dot(rhs.wxyz, lhs.xwzy * signs_x)
		// y = dot(rhs.wxyz, lhs.yzwx * signs_y)
		// z = dot(rhs.wxyz, lhs.zyxw * signs_z)
		// w = dot(rhs.wxyz, lhs.wxyz * signs_w)
		__m128 rhs_wxyz = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(2, 1, 0, 3));
		__m128 lhs_xwzy = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(1, 2, 3, 0));
		__m128 lhs_yzwx = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(0, 3, 2, 1));
		__m128 lhs_zyxw = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(3, 0, 1, 2));
		__m128 lhs_wxyz = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(2, 1, 0, 3));
		__m128 x = _mm_dp_ps(rhs_wxyz, _mm_mul_ps(lhs_xwzy, signs_x), 0xFF);
		__m128 y = _mm_dp_ps(rhs_wxyz, _mm_mul_ps(lhs_yzwx, signs_y), 0xFF);
		__m128 z = _mm_dp_ps(rhs_wxyz, _mm_mul_ps(lhs_zyxw, signs_z), 0xFF);
		__m128 w = _mm_dp_ps(rhs_wxyz, _mm_mul_ps(lhs_wxyz, signs_w), 0xFF);
		__m128 xxyy = _mm_shuffle_ps(x, y, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 zzww = _mm_shuffle_ps(z, w, _MM_SHUFFLE(0, 0, 0, 0));
		return _mm_shuffle_ps(xxyy, zzww, _MM_SHUFFLE(2, 0, 2, 0));
#elif defined(ACL_SSE2_INTRINSICS)
		constexpr __m128 control_wzyx = { 1.0f,-1.0f, 1.0f,-1.0f };
		constexpr __m128 control_zwxy = { 1.0f, 1.0f,-1.0f,-1.0f };
		constexpr __m128 control_yxwz = { -1.0f, 1.0f, 1.0f,-1.0f };

		__m128 r_xxxx = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 r_yyyy = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(1, 1, 1, 1));
		__m128 r_zzzz = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(2, 2, 2, 2));
		__m128 r_wwww = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(3, 3, 3, 3));

		__m128 lxrw_lyrw_lzrw_lwrw = _mm_mul_ps(r_wwww, lhs);
		__m128 l_wzyx = _mm_shuffle_ps(lhs, lhs,_MM_SHUFFLE(0, 1, 2, 3));

		__m128 lwrx_lzrx_lyrx_lxrx = _mm_mul_ps(r_xxxx, l_wzyx);
		__m128 l_zwxy = _mm_shuffle_ps(l_wzyx, l_wzyx,_MM_SHUFFLE(2, 3, 0, 1));

		__m128 lwrx_nlzrx_lyrx_nlxrx = _mm_mul_ps(lwrx_lzrx_lyrx_lxrx, control_wzyx);

		__m128 lzry_lwry_lxry_lyry = _mm_mul_ps(r_yyyy, l_zwxy);
		__m128 l_yxwz = _mm_shuffle_ps(l_zwxy, l_zwxy,_MM_SHUFFLE(0, 1, 2, 3));

		__m128 lzry_lwry_nlxry_nlyry = _mm_mul_ps(lzry_lwry_lxry_lyry, control_zwxy);

		__m128 lyrz_lxrz_lwrz_lzrz = _mm_mul_ps(r_zzzz, l_yxwz);
		__m128 result0 = _mm_add_ps(lxrw_lyrw_lzrw_lwrw, lwrx_nlzrx_lyrx_nlxrx);

		__m128 nlyrz_lxrz_lwrz_wlzrz = _mm_mul_ps(lyrz_lxrz_lwrz_lzrz, control_yxwz);
		__m128 result1 = _mm_add_ps(lzry_lwry_nlxry_nlyry, nlyrz_lxrz_lwrz_wlzrz);
		return _mm_add_ps(result0, result1);
#elif defined(ACL_NEON_INTRINSICS)
		constexpr float32x4_t control_wzyx = { 1.0f,-1.0f, 1.0f,-1.0f };
		constexpr float32x4_t control_zwxy = { 1.0f, 1.0f,-1.0f,-1.0f };
		constexpr float32x4_t control_yxwz = { -1.0f, 1.0f, 1.0f,-1.0f };

		float32x2_t r_xy = vget_low_f32(rhs);
		float32x2_t r_zw = vget_high_f32(rhs);

		float32x4_t r_xxxx = vdupq_lane_f32(r_xy, 0);
		float32x4_t r_yyyy = vdupq_lane_f32(r_xy, 1);
		float32x4_t r_zzzz = vdupq_lane_f32(r_zw, 0);
		float32x4_t lxrw_lyrw_lzrw_lwrw = vmulq_lane_f32(lhs, r_zw, 1);

		float32x4_t l_yxwz = vrev64q_f32(lhs);
		float32x4_t l_wzyx = vcombine_f32(vget_high_f32(l_yxwz), vget_low_f32(l_yxwz));
		float32x4_t lwrx_lzrx_lyrx_lxrx = vmulq_f32(r_xxxx, l_wzyx);
		float32x4_t result0 = vmlaq_f32(lxrw_lyrw_lzrw_lwrw, lwrx_lzrx_lyrx_lxrx, control_wzyx);

		float32x4_t l_zwxy = vrev64q_u32(l_wzyx);
		float32x4_t lzry_lwry_lxry_lyry = vmulq_f32(r_yyyy, l_zwxy);
		float32x4_t result1 = vmlaq_f32(result0, lzry_lwry_lxry_lyry, control_zwxy);

		float32x4_t lyrz_lxrz_lwrz_lzrz = vmulq_f32(r_zzzz, l_yxwz);
		return vmlaq_f32(result1, lyrz_lxrz_lwrz_lzrz, control_yxwz);
#else
		float lhs_x = quat_get_x(lhs);
		float lhs_y = quat_get_y(lhs);
		float lhs_z = quat_get_z(lhs);
		float lhs_w = quat_get_w(lhs);

		float rhs_x = quat_get_x(rhs);
		float rhs_y = quat_get_y(rhs);
		float rhs_z = quat_get_z(rhs);
		float rhs_w = quat_get_w(rhs);

		float x = (rhs_w * lhs_x) + (rhs_x * lhs_w) + (rhs_y * lhs_z) - (rhs_z * lhs_y);
		float y = (rhs_w * lhs_y) - (rhs_x * lhs_z) + (rhs_y * lhs_w) + (rhs_z * lhs_x);
		float z = (rhs_w * lhs_z) + (rhs_x * lhs_y) - (rhs_y * lhs_x) + (rhs_z * lhs_w);
		float w = (rhs_w * lhs_w) - (rhs_x * lhs_x) - (rhs_y * lhs_y) - (rhs_z * lhs_z);

		return quat_set(x, y, z, w);
#endif
	}

	inline Vector4_32 quat_rotate(const Quat_32& rotation, const Vector4_32& vector)
	{
		Quat_32 vector_quat = quat_set(vector_get_x(vector), vector_get_y(vector), vector_get_z(vector), 0.0f);
		Quat_32 inv_rotation = quat_conjugate(rotation);
		return quat_to_vector(quat_mul(quat_mul(inv_rotation, vector_quat), rotation));
	}

	inline float quat_length_squared(const Quat_32& input)
	{
		return vector_length_squared(quat_to_vector(input));
	}

	inline float quat_length(const Quat_32& input)
	{
		return vector_length(quat_to_vector(input));
	}

	inline float quat_length_reciprocal(const Quat_32& input)
	{
		return vector_length_reciprocal(quat_to_vector(input));
	}

	inline Quat_32 quat_normalize(const Quat_32& input)
	{
		// Reciprocal is more accurate to normalize with
		float inv_len = quat_length_reciprocal(input);
		return vector_to_quat(vector_mul(quat_to_vector(input), inv_len));
	}

	inline Quat_32 quat_lerp(const Quat_32& start, const Quat_32& end, float alpha)
	{
#if defined(ACL_SSE2_INTRINSICS)
		// Calculate the vector4 dot product: dot(start, end)
		__m128 x2_y2_z2_w2 = _mm_mul_ps(start, end);
		__m128 z2_w2_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 3, 2));
		__m128 x2z2_y2w2_0_0 = _mm_add_ps(x2_y2_z2_w2, z2_w2_0_0);
		__m128 y2w2_0_0_0 = _mm_shuffle_ps(x2z2_y2w2_0_0, x2z2_y2w2_0_0, _MM_SHUFFLE(0, 0, 0, 1));
		__m128 x2y2z2w2_0_0_0 = _mm_add_ps(x2z2_y2w2_0_0, y2w2_0_0_0);
		// Shuffle the dot product to all SIMD lanes, there is no _mm_and_ss and loading
		// the constant from memory with the 'and' instruction is faster, it uses fewer registers
		// and fewer instructions
		__m128 dot = _mm_shuffle_ps(x2y2z2w2_0_0_0, x2y2z2w2_0_0_0, _MM_SHUFFLE(0, 0, 0, 0));

		// Calculate the bias, if the dot product is positive or zero, there is no bias
		// but if it is negative, we want to flip the 'end' rotation XYZW components
		__m128 bias = _mm_and_ps(dot, _mm_set_ps1(-0.0f));

		// Lerp the rotation after applying the bias
		__m128 interpolated_rotation = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_xor_ps(end, bias), start), _mm_set_ps1(alpha)), start);

		// Now we need to normalize the resulting rotation. We first calculate the
		// dot product to get the length squared: dot(interpolated_rotation, interpolated_rotation)
		x2_y2_z2_w2 = _mm_mul_ps(interpolated_rotation, interpolated_rotation);
		z2_w2_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 3, 2));
		x2z2_y2w2_0_0 = _mm_add_ps(x2_y2_z2_w2, z2_w2_0_0);
		y2w2_0_0_0 = _mm_shuffle_ps(x2z2_y2w2_0_0, x2z2_y2w2_0_0, _MM_SHUFFLE(0, 0, 0, 1));
		x2y2z2w2_0_0_0 = _mm_add_ps(x2z2_y2w2_0_0, y2w2_0_0_0);

		// Keep the dot product result as a scalar within the first lane, it is faster to
		// calculate the reciprocal square root of a single lane VS all 4 lanes
		dot = x2y2z2w2_0_0_0;

		// Calculate the reciprocal square root to get the inverse length of our vector
		// Perform two passes of Newton-Raphson iteration on the hardware estimate
		__m128 half = _mm_set_ss(0.5f);
		__m128 input_half_v = _mm_mul_ss(dot, half);
		__m128 x0 = _mm_rsqrt_ss(dot);

		// First iteration
		__m128 x1 = _mm_mul_ss(x0, x0);
		x1 = _mm_sub_ss(half, _mm_mul_ss(input_half_v, x1));
		x1 = _mm_add_ss(_mm_mul_ss(x0, x1), x0);

		// Second iteration
		__m128 x2 = _mm_mul_ss(x1, x1);
		x2 = _mm_sub_ss(half, _mm_mul_ss(input_half_v, x2));
		x2 = _mm_add_ss(_mm_mul_ss(x1, x2), x1);

		// Broadcast the vector length reciprocal to all 4 lanes in order to multiply it with the vector
		__m128 inv_len = _mm_shuffle_ps(x2, x2, _MM_SHUFFLE(0, 0, 0, 0));

		// Multiply the rotation by it's inverse length in order to normalize it
		return _mm_mul_ps(interpolated_rotation, inv_len);
#elif defined (ACL_NEON64_INTRINSICS)
		// On ARM64 with NEON, we load 1.0 once and use it twice which is faster than
		// using a AND/XOR with the bias (same number of instructions)
		float dot = vector_dot(start, end);
		float bias = dot >= 0.0f ? 1.0f : -1.0f;
		Vector4_32 interpolated_rotation = vector_mul_add(vector_sub(vector_mul(end, bias), start), alpha, start);
		// Use sqrt/div/mul to normalize because the sqrt/div are faster than rsqrt
		float inv_len = 1.0f / sqrt(vector_length_squared(interpolated_rotation));
		return vector_mul(interpolated_rotation, inv_len);
#elif defined(ACL_NEON_INTRINSICS)
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
		float32x4_t end_biased = vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(end), vcombine_u32(bias, bias)));
		float32x4_t interpolated_rotation = vmlaq_n_f32(start, vsubq_f32(end_biased, start), alpha);

		// Now we need to normalize the resulting rotation. We first calculate the
		// dot product to get the length squared: dot(interpolated_rotation, interpolated_rotation)
		x2_y2_z2_w2 = vmulq_f32(interpolated_rotation, interpolated_rotation);
		x2_y2 = vget_low_f32(x2_y2_z2_w2);
		z2_w2 = vget_high_f32(x2_y2_z2_w2);
		x2z2_y2w2 = vadd_f32(x2_y2, z2_w2);
		x2y2z2w2 = vpadd_f32(x2z2_y2w2, x2z2_y2w2);

		float dot = vget_lane_f32(x2y2z2w2, 0);

		// Use sqrt/div/mul to normalize because the sqrt/div are faster than rsqrt
		float inv_len = 1.0f / sqrt(dot);
		return vector_mul(interpolated_rotation, inv_len);
#else
		// To ensure we take the shortest path, we apply a bias if the dot product is negative
		Vector4_32 start_vector = quat_to_vector(start);
		Vector4_32 end_vector = quat_to_vector(end);
		float dot = vector_dot(start_vector, end_vector);
		float bias = dot >= 0.0f ? 1.0f : -1.0f;
		Vector4_32 interpolated_rotation = vector_mul_add(vector_sub(vector_mul(end_vector, bias), start_vector), alpha, start_vector);
		// TODO: Test with this instead: Rotation = (B * Alpha) + (A * (Bias * (1.f - Alpha)));
		//Vector4_32 value = vector_add(vector_mul(end_vector, alpha), vector_mul(start_vector, bias * (1.0f - alpha)));
		return quat_normalize(vector_to_quat(interpolated_rotation));
#endif
	}

	inline Quat_32 quat_neg(const Quat_32& input)
	{
		return vector_to_quat(vector_mul(quat_to_vector(input), -1.0f));
	}

	inline Quat_32 quat_ensure_positive_w(const Quat_32& input)
	{
		return quat_get_w(input) >= 0.f ? input : quat_neg(input);
	}

	inline Quat_32 quat_from_positive_w(const Vector4_32& input)
	{
		// Operation order is important here, due to rounding, ((1.0 - (X*X)) - Y*Y) - Z*Z is more accurate than 1.0 - dot3(xyz, xyz)
		float w_squared = ((1.0f - vector_get_x(input) * vector_get_x(input)) - vector_get_y(input) * vector_get_y(input)) - vector_get_z(input) * vector_get_z(input);
		// w_squared can be negative either due to rounding or due to quantization imprecision, we take the absolute value
		// to ensure the resulting quaternion is always normalized with a positive W component
		float w = sqrt(abs(w_squared));
		return quat_set(vector_get_x(input), vector_get_y(input), vector_get_z(input), w);
	}

	//////////////////////////////////////////////////////////////////////////
	// Conversion to/from axis/angle/euler

	inline void quat_to_axis_angle(const Quat_32& input, Vector4_32& out_axis, float& out_angle)
	{
		constexpr float epsilon = 1.0e-8f;
		constexpr float epsilon_squared = epsilon * epsilon;

		out_angle = acos(quat_get_w(input)) * 2.0f;

		float scale_sq = max(1.0f - quat_get_w(input) * quat_get_w(input), 0.0f);
		out_axis = scale_sq >= epsilon_squared ? vector_div(vector_set(quat_get_x(input), quat_get_y(input), quat_get_z(input)), vector_set(sqrt(scale_sq))) : vector_set(1.0f, 0.0f, 0.0f);
	}

	inline Vector4_32 quat_get_axis(const Quat_32& input)
	{
		constexpr float epsilon = 1.0e-8f;
		constexpr float epsilon_squared = epsilon * epsilon;

		float scale_sq = max(1.0f - quat_get_w(input) * quat_get_w(input), 0.0f);
		return scale_sq >= epsilon_squared ? vector_div(vector_set(quat_get_x(input), quat_get_y(input), quat_get_z(input)), vector_set(sqrt(scale_sq))) : vector_set(1.0f, 0.0f, 0.0f);
	}

	inline float quat_get_angle(const Quat_32& input)
	{
		return acos(quat_get_w(input)) * 2.0f;
	}

	inline Quat_32 quat_from_axis_angle(const Vector4_32& axis, float angle)
	{
		float s, c;
		sincos(0.5f * angle, s, c);

		return quat_set(s * vector_get_x(axis), s * vector_get_y(axis), s * vector_get_z(axis), c);
	}

	// Pitch is around the Y axis (right)
	// Yaw is around the Z axis (up)
	// Roll is around the X axis (forward)
	inline Quat_32 quat_from_euler(float pitch, float yaw, float roll)
	{
		float sp, sy, sr;
		float cp, cy, cr;

		sincos(pitch * 0.5f, sp, cp);
		sincos(yaw * 0.5f, sy, cy);
		sincos(roll * 0.5f, sr, cr);

		return quat_set(cr * sp * sy - sr * cp * cy,
			-cr * sp * cy - sr * cp * sy,
			cr * cp * sy - sr * sp * cy,
			cr * cp * cy + sr * sp * sy);
	}

	//////////////////////////////////////////////////////////////////////////
	// Comparisons and masking

	inline bool quat_is_finite(const Quat_32& input)
	{
		return is_finite(quat_get_x(input)) && is_finite(quat_get_y(input)) && is_finite(quat_get_z(input)) && is_finite(quat_get_w(input));
	}

	inline bool quat_is_normalized(const Quat_32& input, float threshold = 0.00001f)
	{
		float length_squared = quat_length_squared(input);
		return abs(length_squared - 1.0f) < threshold;
	}

	inline bool quat_near_equal(const Quat_32& lhs, const Quat_32& rhs, float threshold = 0.00001f)
	{
		return vector_all_near_equal(quat_to_vector(lhs), quat_to_vector(rhs), threshold);
	}

	inline bool quat_near_identity(const Quat_32& input, float threshold_angle = 0.00284714461f)
	{
		// Because of floating point precision, we cannot represent very small rotations.
		// The closest float to 1.0 that is not 1.0 itself yields:
		// acos(0.99999994f) * 2.0f  = 0.000690533954 rad
		//
		// An error threshold of 1.e-6f is used by default.
		// acos(1.f - 1.e-6f) * 2.0f = 0.00284714461 rad
		// acos(1.f - 1.e-7f) * 2.0f = 0.00097656250 rad
		//
		// We don't really care about the angle value itself, only if it's close to 0.
		// This will happen whenever quat.w is close to 1.0.
		// If the quat.w is close to -1.0, the angle will be near 2*PI which is close to
		// a negative 0 rotation. By forcing quat.w to be positive, we'll end up with
		// the shortest path.
		const float positive_w_angle = acos(abs(quat_get_w(input))) * 2.0f;
		return positive_w_angle < threshold_angle;
	}
}
