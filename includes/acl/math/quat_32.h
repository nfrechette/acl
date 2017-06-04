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

#include "acl/math/math.h"
#include "acl/math/scalar_32.h"
#include "acl/math/vector4_32.h"

namespace acl
{
	inline Quat_32 quat_set(float x, float y, float z, float w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Quat_32(_mm_set_ps(w, z, y, x));
#else
		return Quat_32{ x, y, z, w };
#endif
	}

	inline Quat_32 quat_unaligned_load(const float* input)
	{
		return quat_set(input[0], input[1], input[2], input[3]);
	}

	inline Quat_32 quat_identity_32()
	{
		return quat_set(0.0f, 0.0f, 0.0f, 1.0f);
	}

	inline Quat_32 vector_to_quat(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
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
#else
		return input.x;
#endif
	}

	inline float quat_get_y(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline float quat_get_z(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2)));
#else
		return input.z;
#endif
	}

	inline float quat_get_w(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3)));
#else
		return input.w;
#endif
	}

	inline void quat_unaligned_write(const Quat_32& input, float* output)
	{
		output[0] = quat_get_x(input);
		output[1] = quat_get_y(input);
		output[2] = quat_get_z(input);
		output[3] = quat_get_w(input);
	}

	inline float quat_length_squared(const Quat_32& input)
	{
		// TODO: Use dot instruction
		return (quat_get_x(input) * quat_get_x(input)) + (quat_get_y(input) * quat_get_y(input)) + (quat_get_z(input) * quat_get_z(input)) + (quat_get_w(input) * quat_get_w(input));
	}

	inline float quat_length(const Quat_32& input)
	{
		// TODO: Use intrinsics to avoid scalar coercion
		return sqrt(quat_length_squared(input));
	}

	inline float quat_length_reciprocal(const Quat_32& input)
	{
		// TODO: Use recip instruction
		return 1.0f / quat_length(input);
	}

	inline Quat_32 quat_normalize(const Quat_32& input)
	{
		float length_recip = quat_length_reciprocal(input);
		Vector4_32 input_vector = quat_to_vector(input);
		return vector_to_quat(vector_mul(input_vector, length_recip));
	}

	inline Quat_32 quat_lerp(const Quat_32& start, const Quat_32& end, float alpha)
	{
		Vector4_32 start_vector = quat_to_vector(start);
		Vector4_32 end_vector = quat_to_vector(end);
		Vector4_32 value = vector_add(start_vector, vector_mul(vector_sub(end_vector, start_vector), alpha));
		return quat_normalize(vector_to_quat(value));
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
		float w_squared = 1.0f - vector_length_squared3(input);
		float w = w_squared > 0.0f ? sqrt(w_squared) : 0.0f;
		return quat_set(vector_get_x(input), vector_get_y(input), vector_get_z(input), w);
	}

	inline bool quat_is_valid(const Quat_32& input)
	{
		return is_finite(quat_get_x(input)) && is_finite(quat_get_y(input)) && is_finite(quat_get_z(input)) && is_finite(quat_get_w(input));
	}

	inline bool quat_is_normalized(const Quat_32& input, float threshold = 0.00001f)
	{
		float length_squared = quat_length_squared(input);
		return abs(length_squared - 1.0) < threshold;
	}

	inline bool quat_near_equal(const Quat_32& lhs, const Quat_32& rhs, float threshold = 0.00001f)
	{
		return vector_near_equal(quat_to_vector(lhs), quat_to_vector(rhs), threshold);
	}
}
