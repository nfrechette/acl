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

#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/math/math.h"
#include "acl/math/scalar_32.h"

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Setters, getters, and casts

	inline Vector4_32 ACL_SIMD_CALL vector_set(float x, float y, float z, float w)
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
		return Vector4_32{ x, y, z, w };
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_set(float x, float y, float z)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_set_ps(0.0f, z, y, x);
#elif defined(ACL_NEON_INTRINSICS)
#if 1
		float32x2_t V0 = vcreate_f32(((uint64_t)*(const uint32_t*)&x) | ((uint64_t)(*(const uint32_t*)&y) << 32));
		float32x2_t V1 = vcreate_f32((uint64_t)*(const uint32_t*)&z);
		return vcombine_f32(V0, V1);
#else
		float __attribute__((aligned(16))) data[4] = { x, y, z };
		return vld1q_f32(data);
#endif
#else
		return Vector4_32{ x, y, z, 0.0f };
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_set(float xyzw)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_set_ps1(xyzw);
#elif defined(ACL_NEON_INTRINSICS)
		return vdupq_n_f32(xyzw);
#else
		return Vector4_32{ xyzw, xyzw, xyzw, xyzw };
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_unaligned_load(const float* input)
	{
		ACL_ASSERT(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], input[3]);
	}

	inline Vector4_32 ACL_SIMD_CALL vector_unaligned_load3(const float* input)
	{
		ACL_ASSERT(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], 0.0f);
	}

	inline Vector4_32 ACL_SIMD_CALL vector_unaligned_load_32(const uint8_t* input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_loadu_ps((const float*)input);
#elif defined(ACL_NEON_INTRINSICS)
		return vreinterpretq_f32_u8(vld1q_u8(input));
#else
		Vector4_32 result;
		memcpy(&result, input, sizeof(Vector4_32));
		return result;
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_unaligned_load3_32(const uint8_t* input)
	{
		float input_f[3];
		memcpy(&input_f[0], input, sizeof(float) * 3);
		return vector_set(input_f[0], input_f[1], input_f[2], 0.0f);
	}

	inline Vector4_32 ACL_SIMD_CALL vector_zero_32()
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_setzero_ps();
#else
		return vector_set(0.0f);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL quat_to_vector(Quat_32Arg0 input)
	{
#if defined(ACL_SSE2_INTRINSICS) || defined(ACL_NEON_INTRINSICS)
		return input;
#else
		return Vector4_32{ input.x, input.y, input.z, input.w };
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_cast(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_shuffle_ps(_mm_cvtpd_ps(input.xy), _mm_cvtpd_ps(input.zw), _MM_SHUFFLE(1, 0, 1, 0));
#else
		return vector_set(float(input.x), float(input.y), float(input.z), float(input.w));
#endif
	}

	inline float ACL_SIMD_CALL vector_get_x(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(input);
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 0);
#else
		return input.x;
#endif
	}

	inline float ACL_SIMD_CALL vector_get_y(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1)));
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 1);
#else
		return input.y;
#endif
	}

	inline float ACL_SIMD_CALL vector_get_z(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2)));
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 2);
#else
		return input.z;
#endif
	}

	inline float ACL_SIMD_CALL vector_get_w(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3)));
#elif defined(ACL_NEON_INTRINSICS)
		return vgetq_lane_f32(input, 3);
#else
		return input.w;
#endif
	}

	template<VectorMix component_index>
	inline float ACL_SIMD_CALL vector_get_component(Vector4_32Arg0 input)
	{
		switch (component_index)
		{
		case VectorMix::A:
		case VectorMix::X: return vector_get_x(input);
		case VectorMix::B:
		case VectorMix::Y: return vector_get_y(input);
		case VectorMix::C:
		case VectorMix::Z: return vector_get_z(input);
		case VectorMix::D:
		case VectorMix::W: return vector_get_w(input);
		default:
			ACL_ASSERT(false, "Invalid component index");
			return 0.0f;
		}
	}

	inline float ACL_SIMD_CALL vector_get_component(Vector4_32Arg0 input, VectorMix component_index)
	{
		switch (component_index)
		{
		case VectorMix::A:
		case VectorMix::X: return vector_get_x(input);
		case VectorMix::B:
		case VectorMix::Y: return vector_get_y(input);
		case VectorMix::C:
		case VectorMix::Z: return vector_get_z(input);
		case VectorMix::D:
		case VectorMix::W: return vector_get_w(input);
		default:
			ACL_ASSERT(false, "Invalid component index");
			return 0.0f;
		}
	}

	inline const float* ACL_SIMD_CALL vector_as_float_ptr(const Vector4_32& input)
	{
		return reinterpret_cast<const float*>(&input);
	}

	inline void ACL_SIMD_CALL vector_unaligned_write(Vector4_32Arg0 input, float* output)
	{
		ACL_ASSERT(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
		output[3] = vector_get_w(input);
	}

	inline void ACL_SIMD_CALL vector_unaligned_write3(Vector4_32Arg0 input, float* output)
	{
		ACL_ASSERT(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
	}

	inline void ACL_SIMD_CALL vector_unaligned_write(Vector4_32Arg0 input, uint8_t* output)
	{
		memcpy(output, &input, sizeof(Vector4_32));
	}

	inline void ACL_SIMD_CALL vector_unaligned_write3(Vector4_32Arg0 input, uint8_t* output)
	{
		memcpy(output, &input, sizeof(float) * 3);
	}

	//////////////////////////////////////////////////////////////////////////
	// Arithmetic

	inline Vector4_32 ACL_SIMD_CALL vector_add(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_add_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vaddq_f32(lhs, rhs);
#else
		return vector_set(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_sub(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_sub_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vsubq_f32(lhs, rhs);
#else
		return vector_set(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_mul(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_mul_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vmulq_f32(lhs, rhs);
#else
		return vector_set(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_mul(Vector4_32Arg0 lhs, float rhs)
	{
#if defined(ACL_NEON_INTRINSICS)
		return vmulq_n_f32(lhs, rhs);
#else
		return vector_mul(lhs, vector_set(rhs));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_div(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_div_ps(lhs, rhs);
#elif defined (ACL_NEON64_INTRINSICS)
		return vdivq_f32(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		// Perform two passes of Newton-Raphson iteration on the hardware estimate
		float32x4_t x0 = vrecpeq_f32(rhs);

		// First iteration
		float32x4_t x1 = vmulq_f32(x0, vrecpsq_f32(x0, rhs));

		// Second iteration
		float32x4_t x2 = vmulq_f32(x1, vrecpsq_f32(x1, rhs));
		return vmulq_f32(lhs, x2);
#else
		return vector_set(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_max(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_max_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vmaxq_f32(lhs, rhs);
#else
		return vector_set(max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_min(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_min_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vminq_f32(lhs, rhs);
#else
		return vector_set(min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_clamp(Vector4_32Arg0 input, Vector4_32Arg1 min, Vector4_32Arg2 max)
	{
		return vector_min(max, vector_max(min, input));
	}

	inline Vector4_32 ACL_SIMD_CALL vector_abs(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return vector_max(vector_sub(_mm_setzero_ps(), input), input);
#elif defined(ACL_NEON_INTRINSICS)
		return vabsq_f32(input);
#else
		return vector_set(abs(input.x), abs(input.y), abs(input.z), abs(input.w));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_neg(Vector4_32Arg0 input)
	{
#if defined(ACL_NEON_INTRINSICS)
		return vnegq_f32(input);
#else
		return vector_mul(input, -1.0f);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_reciprocal(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		// Perform two passes of Newton-Raphson iteration on the hardware estimate
		__m128 x0 = _mm_rcp_ps(input);

		// First iteration
		__m128 x1 = _mm_sub_ps(_mm_add_ps(x0, x0), _mm_mul_ps(input, _mm_mul_ps(x0, x0)));

		// Second iteration
		__m128 x2 = _mm_sub_ps(_mm_add_ps(x1, x1), _mm_mul_ps(input, _mm_mul_ps(x1, x1)));
		return x2;
#elif defined(ACL_NEON_INTRINSICS)
		// Perform two passes of Newton-Raphson iteration on the hardware estimate
		float32x4_t x0 = vrecpeq_f32(input);

		// First iteration
		float32x4_t x1 = vmulq_f32(x0, vrecpsq_f32(x0, input));

		// Second iteration
		float32x4_t x2 = vmulq_f32(x1, vrecpsq_f32(x1, input));
		return x2;
#else
		return vector_div(vector_set(1.0f), input);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_ceil(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE4_INTRINSICS)
		return _mm_ceil_ps(input);
#else
		return vector_set(ceil(vector_get_x(input)), ceil(vector_get_y(input)), ceil(vector_get_z(input)), ceil(vector_get_w(input)));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_floor(Vector4_32Arg0 input)
	{
#if defined(ACL_SSE4_INTRINSICS)
		return _mm_floor_ps(input);
#else
		return vector_set(floor(vector_get_x(input)), floor(vector_get_y(input)), floor(vector_get_z(input)), floor(vector_get_w(input)));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_cross3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
		return vector_set(vector_get_y(lhs) * vector_get_z(rhs) - vector_get_z(lhs) * vector_get_y(rhs),
						  vector_get_z(lhs) * vector_get_x(rhs) - vector_get_x(lhs) * vector_get_z(rhs),
						  vector_get_x(lhs) * vector_get_y(rhs) - vector_get_y(lhs) * vector_get_x(rhs));
	}

	inline float ACL_SIMD_CALL vector_dot(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE4_INTRINSICS) && 0
		// SSE4 dot product instruction isn't precise enough
		return _mm_cvtss_f32(_mm_dp_ps(lhs, rhs, 0xFF));
#elif defined(ACL_SSE2_INTRINSICS)
		__m128 x2_y2_z2_w2 = _mm_mul_ps(lhs, rhs);
		__m128 z2_w2_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 3, 2));
		__m128 x2z2_y2w2_0_0 = _mm_add_ps(x2_y2_z2_w2, z2_w2_0_0);
		__m128 y2w2_0_0_0 = _mm_shuffle_ps(x2z2_y2w2_0_0, x2z2_y2w2_0_0, _MM_SHUFFLE(0, 0, 0, 1));
		__m128 x2y2z2w2_0_0_0 = _mm_add_ps(x2z2_y2w2_0_0, y2w2_0_0_0);
		return _mm_cvtss_f32(x2y2z2w2_0_0_0);
#elif defined(ACL_NEON_INTRINSICS)
		float32x4_t x2_y2_z2_w2 = vmulq_f32(lhs, rhs);
		float32x2_t x2_y2 = vget_low_f32(x2_y2_z2_w2);
		float32x2_t z2_w2 = vget_high_f32(x2_y2_z2_w2);
		float32x2_t x2z2_y2w2 = vadd_f32(x2_y2, z2_w2);
		float32x2_t x2y2z2w2 = vpadd_f32(x2z2_y2w2, x2z2_y2w2);
		return vget_lane_f32(x2y2z2w2, 0);
#else
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs)) + (vector_get_w(lhs) * vector_get_w(rhs));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_vdot(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE4_INTRINSICS) && 0
		// SSE4 dot product instruction isn't precise enough
		return _mm_dp_ps(lhs, rhs, 0xFF);
#elif defined(ACL_SSE2_INTRINSICS)
		__m128 x2_y2_z2_w2 = _mm_mul_ps(lhs, rhs);
		__m128 z2_w2_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 3, 2));
		__m128 x2z2_y2w2_0_0 = _mm_add_ps(x2_y2_z2_w2, z2_w2_0_0);
		__m128 y2w2_0_0_0 = _mm_shuffle_ps(x2z2_y2w2_0_0, x2z2_y2w2_0_0, _MM_SHUFFLE(0, 0, 0, 1));
		__m128 x2y2z2w2_0_0_0 = _mm_add_ps(x2z2_y2w2_0_0, y2w2_0_0_0);
		return _mm_shuffle_ps(x2y2z2w2_0_0_0, x2y2z2w2_0_0_0, _MM_SHUFFLE(0, 0, 0, 0));
#elif defined(ACL_NEON_INTRINSICS)
		float32x4_t x2_y2_z2_w2 = vmulq_f32(lhs, rhs);
		float32x2_t x2_y2 = vget_low_f32(x2_y2_z2_w2);
		float32x2_t z2_w2 = vget_high_f32(x2_y2_z2_w2);
		float32x2_t x2z2_y2w2 = vadd_f32(x2_y2, z2_w2);
		float32x2_t x2y2z2w2 = vpadd_f32(x2z2_y2w2, x2z2_y2w2);
		return vcombine_f32(x2y2z2w2, x2y2z2w2);
#else
		return vector_set(vector_dot(lhs, rhs));
#endif
	}

	inline float ACL_SIMD_CALL vector_dot3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE4_INTRINSICS) && 0
		// SSE4 dot product instruction isn't precise enough
		return _mm_cvtss_f32(_mm_dp_ps(lhs, rhs, 0x7F));
#elif defined(ACL_SSE2_INTRINSICS)
		__m128 x2_y2_z2_w2 = _mm_mul_ps(lhs, rhs);
		__m128 y2_0_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 0, 1));
		__m128 x2y2_0_0_0 = _mm_add_ss(x2_y2_z2_w2, y2_0_0_0);
		__m128 z2_0_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 0, 2));
		__m128 x2y2z2_0_0_0 = _mm_add_ss(x2y2_0_0_0, z2_0_0_0);
		return _mm_cvtss_f32(x2y2z2_0_0_0);
#elif defined(ACL_NEON_INTRINSICS)
		float32x4_t x2_y2_z2_w2 = vmulq_f32(lhs, rhs);
		float32x2_t x2_y2 = vget_low_f32(x2_y2_z2_w2);
		float32x2_t z2_w2 = vget_high_f32(x2_y2_z2_w2);
		float32x2_t x2y2_x2y2 = vpadd_f32(x2_y2, x2_y2);
		float32x2_t z2_z2 = vdup_lane_f32(z2_w2, 0);
		float32x2_t x2y2z2_x2y2z2 = vadd_f32(x2y2_x2y2, z2_z2);
		return vget_lane_f32(x2y2z2_x2y2z2, 0);
#else
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs));
#endif
	}

	inline float ACL_SIMD_CALL vector_length_squared(Vector4_32Arg0 input)
	{
		return vector_dot(input, input);
	}

	inline float ACL_SIMD_CALL vector_length_squared3(Vector4_32Arg0 input)
	{
		return vector_dot3(input, input);
	}

	inline float ACL_SIMD_CALL vector_length(Vector4_32Arg0 input)
	{
		return sqrt(vector_length_squared(input));
	}

	inline float ACL_SIMD_CALL vector_length3(Vector4_32Arg0 input)
	{
		return sqrt(vector_length_squared3(input));
	}

	inline float ACL_SIMD_CALL vector_length_reciprocal(Vector4_32Arg0 input)
	{
		return sqrt_reciprocal(vector_length_squared(input));
	}

	inline float ACL_SIMD_CALL vector_length_reciprocal3(Vector4_32Arg0 input)
	{
		return sqrt_reciprocal(vector_length_squared3(input));
	}

	inline float ACL_SIMD_CALL vector_distance3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
		return vector_length3(vector_sub(rhs, lhs));
	}

	inline Vector4_32 ACL_SIMD_CALL vector_normalize3(Vector4_32Arg0 input, float threshold = 1.0e-8f)
	{
		// Reciprocal is more accurate to normalize with
		const float len_sq = vector_length_squared3(input);
		if (len_sq >= threshold)
			return vector_mul(input, sqrt_reciprocal(len_sq));
		else
			return input;
	}

	inline Vector4_32 ACL_SIMD_CALL vector_fraction(Vector4_32Arg0 input)
	{
		return vector_set(fraction(vector_get_x(input)), fraction(vector_get_y(input)), fraction(vector_get_z(input)), fraction(vector_get_w(input)));
	}

	// output = (input * scale) + offset
	inline Vector4_32 ACL_SIMD_CALL vector_mul_add(Vector4_32Arg0 input, Vector4_32Arg1 scale, Vector4_32Arg2 offset)
	{
#if defined(ACL_NEON64_INTRINSICS)
		return vfmaq_f32(offset, input, scale);
#elif defined(ACL_NEON_INTRINSICS)
		return vmlaq_f32(offset, input, scale);
#else
		return vector_add(vector_mul(input, scale), offset);
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_mul_add(Vector4_32Arg0 input, float scale, Vector4_32Arg2 offset)
	{
#if defined(ACL_NEON64_INTRINSICS)
		return vfmaq_n_f32(offset, input, scale);
#elif defined(ACL_NEON_INTRINSICS)
		return vmlaq_n_f32(offset, input, scale);
#else
		return vector_add(vector_mul(input, scale), offset);
#endif
	}

	// output = offset - (input * scale)
	inline Vector4_32 ACL_SIMD_CALL vector_neg_mul_sub(Vector4_32Arg0 input, Vector4_32Arg1 scale, Vector4_32Arg2 offset)
	{
#if defined(ACL_NEON64_INTRINSICS)
		return vfmsq_f32(offset, input, scale);
#elif defined(ACL_NEON_INTRINSICS)
		return vmlsq_f32(offset, input, scale);
#else
		return vector_sub(offset, vector_mul(input, scale));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_neg_mul_sub(Vector4_32Arg0 input, float scale, Vector4_32Arg2 offset)
	{
#if defined(ACL_NEON64_INTRINSICS)
		return vfmsq_n_f32(offset, input, scale);
#elif defined(ACL_NEON_INTRINSICS)
		return vmlsq_n_f32(offset, input, scale);
#else
		return vector_sub(offset, vector_mul(input, scale));
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_lerp(Vector4_32Arg0 start, Vector4_32Arg1 end, float alpha)
	{
		return vector_mul_add(vector_sub(end, start), alpha, start);
	}

	//////////////////////////////////////////////////////////////////////////
	// Comparisons and masking

	inline Vector4_32 ACL_SIMD_CALL vector_less_than(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cmplt_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vcltq_f32(lhs, rhs);
#else
		return Vector4_32{ math_impl::get_mask_value(lhs.x < rhs.x), math_impl::get_mask_value(lhs.y < rhs.y), math_impl::get_mask_value(lhs.z < rhs.z), math_impl::get_mask_value(lhs.w < rhs.w) };
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_less_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cmple_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vcleq_f32(lhs, rhs);
#else
		return Vector4_32{ math_impl::get_mask_value(lhs.x <= rhs.x), math_impl::get_mask_value(lhs.y <= rhs.y), math_impl::get_mask_value(lhs.z <= rhs.z), math_impl::get_mask_value(lhs.w <= rhs.w) };
#endif
	}

	inline Vector4_32 ACL_SIMD_CALL vector_greater_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cmpge_ps(lhs, rhs);
#elif defined(ACL_NEON_INTRINSICS)
		return vcgeq_f32(lhs, rhs);
#else
		return Vector4_32{ math_impl::get_mask_value(lhs.x >= rhs.x), math_impl::get_mask_value(lhs.y >= rhs.y), math_impl::get_mask_value(lhs.z >= rhs.z), math_impl::get_mask_value(lhs.w >= rhs.w) };
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_less_than(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) == 0xF;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcltq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) == 0xFFFFFFFFu;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z && lhs.w < rhs.w;
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_less_than3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) & 0x7) == 0x7;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcltq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return (vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) & 0x00FFFFFFu) == 0x00FFFFFFu;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z;
#endif
	}

	inline bool ACL_SIMD_CALL vector_any_less_than(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) != 0;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcltq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) != 0;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z || lhs.w < rhs.w;
#endif
	}

	inline bool ACL_SIMD_CALL vector_any_less_than3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) & 0x7) != 0;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcltq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return (vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) & 0x00FFFFFFu) != 0;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z;
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_less_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) == 0xF;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcleq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) == 0xFFFFFFFFu;
#else
		return lhs.x <= rhs.x && lhs.y <= rhs.y && lhs.z <= rhs.z && lhs.w <= rhs.w;
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_less_equal2(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) & 0x3) == 0x3;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x2_t mask = vcle_f32(vget_low_f32(lhs), vget_low_f32(rhs));
		return vget_lane_u64(mask, 0) == 0xFFFFFFFFFFFFFFFFu;
#else
		return lhs.x <= rhs.x && lhs.y <= rhs.y;
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_less_equal3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) & 0x7) == 0x7;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcleq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return (vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) & 0x00FFFFFFu) == 0x00FFFFFFu;
#else
		return lhs.x <= rhs.x && lhs.y <= rhs.y && lhs.z <= rhs.z;
#endif
	}

	inline bool ACL_SIMD_CALL vector_any_less_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) != 0;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcleq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) != 0;
#else
		return lhs.x <= rhs.x || lhs.y <= rhs.y || lhs.z <= rhs.z || lhs.w <= rhs.w;
#endif
	}

	inline bool ACL_SIMD_CALL vector_any_less_equal3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) & 0x7) != 0;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcleq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return (vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) & 0x00FFFFFFu) != 0;
#else
		return lhs.x <= rhs.x || lhs.y <= rhs.y || lhs.z <= rhs.z;
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_greater_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) == 0xF;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcgeq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) == 0xFFFFFFFFu;
#else
		return lhs.x >= rhs.x && lhs.y >= rhs.y && lhs.z >= rhs.z && lhs.w >= rhs.w;
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_greater_equal3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) & 0x7) == 0x7;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcgeq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return (vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) & 0x00FFFFFFu) == 0x00FFFFFFu;
#else
		return lhs.x >= rhs.x && lhs.y >= rhs.y && lhs.z >= rhs.z;
#endif
	}

	inline bool ACL_SIMD_CALL vector_any_greater_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) != 0;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcgeq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) != 0;
#else
		return lhs.x >= rhs.x || lhs.y >= rhs.y || lhs.z >= rhs.z || lhs.w >= rhs.w;
#endif
	}

	inline bool ACL_SIMD_CALL vector_any_greater_equal3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) & 0x7) != 0;
#elif defined(ACL_NEON_INTRINSICS)
		uint32x4_t mask = vcgeq_f32(lhs, rhs);
		uint8x8x2_t mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15 = vzip_u8(vget_low_u8(mask), vget_high_u8(mask));
		uint16x4x2_t mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15 = vzip_u16(mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[0], mask_0_8_1_9_2_10_3_11_4_12_5_13_6_14_7_15.val[1]);
		return (vget_lane_u32(mask_0_8_4_12_1_9_5_13_2_10_6_14_3_11_7_15.val[0], 0) & 0x00FFFFFFu) != 0;
#else
		return lhs.x >= rhs.x || lhs.y >= rhs.y || lhs.z >= rhs.z;
#endif
	}

	inline bool ACL_SIMD_CALL vector_all_near_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs, float threshold = 0.00001f)
	{
		return vector_all_less_equal(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool ACL_SIMD_CALL vector_all_near_equal2(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs, float threshold = 0.00001f)
	{
		return vector_all_less_equal2(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool ACL_SIMD_CALL vector_all_near_equal3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs, float threshold = 0.00001f)
	{
		return vector_all_less_equal3(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool ACL_SIMD_CALL vector_any_near_equal(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs, float threshold = 0.00001f)
	{
		return vector_any_less_equal(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool ACL_SIMD_CALL vector_any_near_equal3(Vector4_32Arg0 lhs, Vector4_32Arg1 rhs, float threshold = 0.00001f)
	{
		return vector_any_less_equal3(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool ACL_SIMD_CALL vector_is_finite(Vector4_32Arg0 input)
	{
		return is_finite(vector_get_x(input)) && is_finite(vector_get_y(input)) && is_finite(vector_get_z(input)) && is_finite(vector_get_w(input));
	}

	inline bool ACL_SIMD_CALL vector_is_finite3(Vector4_32Arg0 input)
	{
		return is_finite(vector_get_x(input)) && is_finite(vector_get_y(input)) && is_finite(vector_get_z(input));
	}

	//////////////////////////////////////////////////////////////////////////
	// Swizzling, permutations, and mixing

	inline Vector4_32 ACL_SIMD_CALL vector_blend(Vector4_32Arg0 mask, Vector4_32Arg1 if_true, Vector4_32Arg2 if_false)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_or_ps(_mm_andnot_ps(mask, if_false), _mm_and_ps(if_true, mask));
#elif defined(ACL_NEON_INTRINSICS)
		return vbslq_f32(mask, if_true, if_false);
#else
		return Vector4_32{ math_impl::select(mask.x, if_true.x, if_false.x), math_impl::select(mask.y, if_true.y, if_false.y), math_impl::select(mask.z, if_true.z, if_false.z), math_impl::select(mask.w, if_true.w, if_false.w) };
#endif
	}

	template<VectorMix comp0, VectorMix comp1, VectorMix comp2, VectorMix comp3>
	inline Vector4_32 ACL_SIMD_CALL vector_mix(Vector4_32Arg0 input0, Vector4_32Arg1 input1)
	{
#if defined(ACL_SSE2_INTRINSICS)
		// All four components come from input 0
		if (math_impl::is_vector_mix_arg_xyzw(comp0) && math_impl::is_vector_mix_arg_xyzw(comp1) && math_impl::is_vector_mix_arg_xyzw(comp2) && math_impl::is_vector_mix_arg_xyzw(comp3))
			return _mm_shuffle_ps(input0, input0, _MM_SHUFFLE(int(comp3) % 4, int(comp2) % 4, int(comp1) % 4, int(comp0) % 4));

		// All four components come from input 1
		if (math_impl::is_vector_mix_arg_abcd(comp0) && math_impl::is_vector_mix_arg_abcd(comp1) && math_impl::is_vector_mix_arg_abcd(comp2) && math_impl::is_vector_mix_arg_abcd(comp3))
			return _mm_shuffle_ps(input1, input1, _MM_SHUFFLE(int(comp3) % 4, int(comp2) % 4, int(comp1) % 4, int(comp0) % 4));

		// First two components come from input 0, second two come from input 1
		if (math_impl::is_vector_mix_arg_xyzw(comp0) && math_impl::is_vector_mix_arg_xyzw(comp1) && math_impl::is_vector_mix_arg_abcd(comp2) && math_impl::is_vector_mix_arg_abcd(comp3))
			return _mm_shuffle_ps(input0, input1, _MM_SHUFFLE(int(comp3) % 4, int(comp2) % 4, int(comp1) % 4, int(comp0) % 4));

		// First two components come from input 1, second two come from input 0
		if (math_impl::is_vector_mix_arg_abcd(comp0) && math_impl::is_vector_mix_arg_abcd(comp1) && math_impl::is_vector_mix_arg_xyzw(comp2) && math_impl::is_vector_mix_arg_xyzw(comp3))
			return _mm_shuffle_ps(input1, input0, _MM_SHUFFLE(int(comp3) % 4, int(comp2) % 4, int(comp1) % 4, int(comp0) % 4));

		// Low words from both inputs are interleaved
		if (static_condition<comp0 == VectorMix::X && comp1 == VectorMix::A && comp2 == VectorMix::Y && comp3 == VectorMix::B>::test())
			return _mm_unpacklo_ps(input0, input1);

		// Low words from both inputs are interleaved
		if (static_condition<comp0 == VectorMix::A && comp1 == VectorMix::X && comp2 == VectorMix::B && comp3 == VectorMix::Y>::test())
			return _mm_unpacklo_ps(input1, input0);

		// High words from both inputs are interleaved
		if (static_condition<comp0 == VectorMix::Z && comp1 == VectorMix::C && comp2 == VectorMix::W && comp3 == VectorMix::D>::test())
			return _mm_unpackhi_ps(input0, input1);

		// High words from both inputs are interleaved
		if (static_condition<comp0 == VectorMix::C && comp1 == VectorMix::Z && comp2 == VectorMix::D && comp3 == VectorMix::W>::test())
			return _mm_unpackhi_ps(input1, input0);
#endif	// defined(ACL_SSE2_INTRINSICS)

		// Slow code path, not yet optimized or not using intrinsics
		const float x = math_impl::is_vector_mix_arg_xyzw(comp0) ? vector_get_component<comp0>(input0) : vector_get_component<comp0>(input1);
		const float y = math_impl::is_vector_mix_arg_xyzw(comp1) ? vector_get_component<comp1>(input0) : vector_get_component<comp1>(input1);
		const float z = math_impl::is_vector_mix_arg_xyzw(comp2) ? vector_get_component<comp2>(input0) : vector_get_component<comp2>(input1);
		const float w = math_impl::is_vector_mix_arg_xyzw(comp3) ? vector_get_component<comp3>(input0) : vector_get_component<comp3>(input1);
		return vector_set(x, y, z, w);
	}

	inline Vector4_32 ACL_SIMD_CALL vector_mix_xxxx(Vector4_32Arg0 input) { return vector_mix<VectorMix::X, VectorMix::X, VectorMix::X, VectorMix::X>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_yyyy(Vector4_32Arg0 input) { return vector_mix<VectorMix::Y, VectorMix::Y, VectorMix::Y, VectorMix::Y>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zzzz(Vector4_32Arg0 input) { return vector_mix<VectorMix::Z, VectorMix::Z, VectorMix::Z, VectorMix::Z>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_wwww(Vector4_32Arg0 input) { return vector_mix<VectorMix::W, VectorMix::W, VectorMix::W, VectorMix::W>(input, input); }

	inline Vector4_32 ACL_SIMD_CALL vector_mix_xxyy(Vector4_32Arg0 input) { return vector_mix<VectorMix::X, VectorMix::X, VectorMix::Y, VectorMix::Y>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_xzyw(Vector4_32Arg0 input) { return vector_mix<VectorMix::X, VectorMix::Z, VectorMix::Y, VectorMix::W>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_yzxy(Vector4_32Arg0 input) { return vector_mix<VectorMix::Y, VectorMix::Z, VectorMix::X, VectorMix::Y>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_ywxz(Vector4_32Arg0 input) { return vector_mix<VectorMix::Y, VectorMix::W, VectorMix::X, VectorMix::Z>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zxyx(Vector4_32Arg0 input) { return vector_mix<VectorMix::Z, VectorMix::X, VectorMix::Y, VectorMix::X>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zwyz(Vector4_32Arg0 input) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::Y, VectorMix::Z>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zwzw(Vector4_32Arg0 input) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::Z, VectorMix::W>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_wxwx(Vector4_32Arg0 input) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::W, VectorMix::X>(input, input); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_wzwy(Vector4_32Arg0 input) { return vector_mix<VectorMix::W, VectorMix::Z, VectorMix::W, VectorMix::Y>(input, input); }

	inline Vector4_32 ACL_SIMD_CALL vector_mix_xyab(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::B>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_xzac(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::X, VectorMix::Z, VectorMix::A, VectorMix::C>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_xbxb(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::X, VectorMix::B, VectorMix::X, VectorMix::B>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_xbzd(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::X, VectorMix::B, VectorMix::Z, VectorMix::D>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_ywbd(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::Y, VectorMix::W, VectorMix::B, VectorMix::D>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zyax(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::Z, VectorMix::Y, VectorMix::A, VectorMix::X>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zycx(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::Z, VectorMix::Y, VectorMix::C, VectorMix::X>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zwcd(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::C, VectorMix::D>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zbaz(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::Z, VectorMix::B, VectorMix::A, VectorMix::Z>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_zdcz(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::Z, VectorMix::D, VectorMix::C, VectorMix::Z>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_wxya(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::Y, VectorMix::A>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_wxyc(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::Y, VectorMix::C>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_wbyz(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::W, VectorMix::B, VectorMix::Y, VectorMix::Z>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_wdyz(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::W, VectorMix::D, VectorMix::Y, VectorMix::Z>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_bxwa(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::B, VectorMix::X, VectorMix::W, VectorMix::A>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_bywx(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::B, VectorMix::Y, VectorMix::W, VectorMix::X>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_dxwc(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::D, VectorMix::X, VectorMix::W, VectorMix::C>(input0, input1); }
	inline Vector4_32 ACL_SIMD_CALL vector_mix_dywx(Vector4_32Arg0 input0, Vector4_32Arg1 input1) { return vector_mix<VectorMix::D, VectorMix::Y, VectorMix::W, VectorMix::X>(input0, input1); }

	//////////////////////////////////////////////////////////////////////////
	// Misc

	inline Vector4_32 ACL_SIMD_CALL vector_sign(Vector4_32Arg0 input)
	{
		Vector4_32 mask = vector_greater_equal(input, vector_zero_32());
		return vector_blend(mask, vector_set(1.0f), vector_set(-1.0f));
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns per component the rounded input using a symmetric algorithm.
	// symmetric_round(1.5) = 2.0
	// symmetric_round(1.2) = 1.0
	// symmetric_round(-1.5) = -2.0
	// symmetric_round(-1.2) = -1.0
	//////////////////////////////////////////////////////////////////////////
	inline Vector4_32 ACL_SIMD_CALL vector_symmetric_round(Vector4_32Arg0 input)
	{
		const Vector4_32 half = vector_set(0.5f);
		const Vector4_32 floored = vector_floor(vector_add(input, half));
		const Vector4_32 ceiled = vector_ceil(vector_sub(input, half));
		const Vector4_32 is_greater_equal = vector_greater_equal(input, vector_zero_32());
		return vector_blend(is_greater_equal, floored, ceiled);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
