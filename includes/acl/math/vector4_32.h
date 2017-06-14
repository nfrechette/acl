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
#include "acl/core/memory.h"
#include "acl/math/math.h"

namespace acl
{
	inline Vector4_32 vector_set(float x, float y, float z, float w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps(w, z, y, x));
#else
		return Vector4_32{ x, y, z, w };
#endif
	}

	inline Vector4_32 vector_set(float x, float y, float z)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps(0.0f, z, y, x));
#else
		return Vector4_32{ x, y, z, 0.0f };
#endif
	}

	inline Vector4_32 vector_set(float xyzw)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps1(xyzw));
#else
		return Vector4_32{ xyzw, xyzw, xyzw, xyzw };
#endif
	}

	inline Vector4_32 vector_unaligned_load(const float* input)
	{
		ACL_ENSURE(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], input[3]);
	}

	inline Vector4_32 vector_unaligned_load(const uint8_t* input)
	{
		// TODO: Cross platform unaligned read needs to be safe
		const float* input_f = reinterpret_cast<const float*>(input);
		return vector_set(input_f[0], input_f[1], input_f[2], input_f[3]);
	}

	inline Vector4_32 vector_unaligned_load3(const float* input)
	{
		ACL_ENSURE(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], 0.0f);
	}

	inline Vector4_32 vector_unaligned_load3(const uint8_t* input)
	{
		// TODO: Cross platform unaligned read needs to be safe
		const float* input_f = reinterpret_cast<const float*>(input);
		return vector_set(input_f[0], input_f[1], input_f[2], 0.0f);
	}

	inline Vector4_32 vector_zero_32()
	{
		return vector_set(0.0f, 0.0f, 0.0f, 0.0f);
	}

	inline Vector4_32 quat_to_vector(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return input;
#else
		return Vector4_32{ input.x, input.y, input.z, input.w };
#endif
	}

	inline Vector4_32 vector_cast(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_shuffle_ps(_mm_cvtpd_ps(input.xy), _mm_cvtpd_ps(input.zw), _MM_SHUFFLE(1, 0, 1, 0));
#else
		return Vector4_32{ float(input.x), float(input.y), float(input.z), float(input.w) };
#endif
	}

	inline float vector_get_x(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(input);
#else
		return input.x;
#endif
	}

	inline float vector_get_y(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline float vector_get_z(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2)));
#else
		return input.z;
#endif
	}

	inline float vector_get_w(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3)));
#else
		return input.w;
#endif
	}

	inline float* vector_as_float_ptr(Vector4_32& input)
	{
		return reinterpret_cast<float*>(&input);
	}

	inline const float* vector_as_float_ptr(const Vector4_32& input)
	{
		return reinterpret_cast<const float*>(&input);
	}

	inline void vector_unaligned_write(const Vector4_32& input, float* output)
	{
		ACL_ENSURE(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
		output[3] = vector_get_w(input);
	}

	inline void vector_unaligned_write(const Vector4_32& input, uint8_t* output)
	{
		// TODO: Cross platform unaligned write needs to be safe
		float* output_f = reinterpret_cast<float*>(output);
		output_f[0] = vector_get_x(input);
		output_f[1] = vector_get_y(input);
		output_f[2] = vector_get_z(input);
		output_f[3] = vector_get_w(input);
	}

	inline void vector_unaligned_write3(const Vector4_32& input, float* output)
	{
		ACL_ENSURE(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
	}

	inline void vector_unaligned_write3(const Vector4_32& input, uint8_t* output)
	{
		// TODO: Cross platform unaligned write needs to be safe
		float* output_f = reinterpret_cast<float*>(output);
		output_f[0] = vector_get_x(input);
		output_f[1] = vector_get_y(input);
		output_f[2] = vector_get_z(input);
	}

	inline Vector4_32 vector_add(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_add_ps(lhs, rhs);
#else
		return vector_set(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
#endif
	}

	inline Vector4_32 vector_sub(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_sub_ps(lhs, rhs);
#else
		return vector_set(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
#endif
	}

	inline Vector4_32 vector_mul(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_mul_ps(lhs, rhs);
#else
		return vector_set(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
#endif
	}

	inline Vector4_32 vector_mul(const Vector4_32& lhs, float rhs)
	{
		return vector_mul(lhs, vector_set(rhs));
	}

	inline Vector4_32 vector_max(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_max_ps(lhs, rhs);
#else
		return vector_set(max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w));
#endif
	}

	inline Vector4_32 vector_min(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_min_ps(lhs, rhs);
#else
		return vector_set(min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w));
#endif
	}

	inline Vector4_32 vector_abs(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return vector_max(vector_sub(_mm_setzero_ps(), input), input);
#else
		return vector_set(abs(input.x), abs(input.y), abs(input.z), abs(input.w));
#endif
	}

	inline Vector4_32 vector_neg(const Vector4_32& input)
	{
		return vector_mul(input, -1.0f);
	}

	inline float vector_dot(const Vector4_32& lhs, const Vector4_32& rhs)
	{
		// TODO: Use dot instruction
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs)) + (vector_get_w(lhs) * vector_get_w(rhs));
	}

	inline float vector_dot3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
		// TODO: Use dot instruction
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs));
	}

	inline float vector_length_squared(const Vector4_32& input)
	{
		return vector_dot(input, input);
	}

	inline float vector_length_squared3(const Vector4_32& input)
	{
		return vector_dot3(input, input);
	}

	inline float vector_length(const Vector4_32& input)
	{
		// TODO: Use intrinsics to avoid scalar coercion
		return sqrt(vector_length_squared(input));
	}

	inline float vector_length3(const Vector4_32& input)
	{
		// TODO: Use intrinsics to avoid scalar coercion
		return sqrt(vector_length_squared3(input));
	}

	inline float vector_length_reciprocal(const Vector4_32& input)
	{
		// TODO: Use recip instruction
		return 1.0f / vector_length(input);
	}

	inline float vector_length_reciprocal3(const Vector4_32& input)
	{
		// TODO: Use recip instruction
		return 1.0f / vector_length3(input);
	}

	inline float vector_distance3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
		return vector_length3(vector_sub(rhs, lhs));
	}

	inline Vector4_32 vector_lerp(const Vector4_32& start, const Vector4_32& end, float alpha)
	{
		return vector_add(start, vector_mul(vector_sub(end, start), vector_set(alpha)));
	}

	inline bool vector_all_less_than(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) == 15;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z && lhs.w < rhs.w;
#endif
	}

	inline bool vector_any_less_than(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) != 0;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z || lhs.w < rhs.w;
#endif
	}

	inline bool vector_near_equal(const Vector4_32& lhs, const Vector4_32& rhs, float threshold = 0.00001f)
	{
		return vector_all_less_than(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	// output = (input * scale) + offset
	inline Vector4_32 vector_mul_add(const Vector4_32& input, const Vector4_32& scale, const Vector4_32& offset)
	{
		return vector_add(vector_mul(input, scale), offset);
	}
}
