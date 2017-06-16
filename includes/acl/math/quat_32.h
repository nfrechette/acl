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
		ACL_ENSURE(is_aligned(input), "Invalid alignment");
		return quat_set(input[0], input[1], input[2], input[3]);
	}

	inline Quat_32 quat_unaligned_load(const uint8_t* input)
	{
		// TODO: Cross platform unaligned read needs to be safe
		const float* input_f = reinterpret_cast<const float*>(input);
		return quat_set(input_f[0], input_f[1], input_f[2], input_f[3]);
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
		ACL_ENSURE(is_aligned(output), "Invalid alignment");
		output[0] = quat_get_x(input);
		output[1] = quat_get_y(input);
		output[2] = quat_get_z(input);
		output[3] = quat_get_w(input);
	}

	inline void quat_unaligned_write(const Quat_32& input, uint8_t* output)
	{
		// TODO: Cross platform unaligned write needs to be safe
		float* output_f = reinterpret_cast<float*>(output);
		output_f[0] = quat_get_x(input);
		output_f[1] = quat_get_y(input);
		output_f[2] = quat_get_z(input);
		output_f[3] = quat_get_w(input);
	}

	inline Quat_32 quat_conjugate(const Quat_32& input)
	{
		return quat_set(-quat_get_x(input), -quat_get_y(input), -quat_get_z(input), quat_get_w(input));
	}

	// Multiplication order is as follow: local_to_world = quat_mul(local_to_object, object_to_world)
	inline Quat_32 quat_mul(const Quat_32& lhs, const Quat_32& rhs)
	{
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
	}

	inline Vector4_32 quat_rotate(const Quat_32& rotation, const Vector4_32& vector)
	{
		Quat_32 vector_quat = vector_to_quat(vector_mul(vector, vector_set(1.0f, 1.0f, 1.0f, 0.0f)));
		Quat_32 inv_rotation = quat_conjugate(rotation);
		return quat_to_vector(quat_mul(quat_mul(inv_rotation, vector_quat), rotation));
	}

	inline void quat_to_axis_angle(const Quat_32& input, Vector4_32& out_axis, float& out_angle)
	{
		constexpr float EPSILON = 1.0e-8f;
		constexpr float EPSILON_SQUARED = EPSILON * EPSILON;

		float real_length_squared = vector_length_squared3(quat_to_vector(input));

		if (real_length_squared < EPSILON_SQUARED)
		{
			out_axis = vector_set(1.0f, 0.0f, 0.0f);
			out_angle = 0.0f;
		}
		else
		{
			float real_length = sqrt_reciprocal(real_length_squared);
			out_axis = vector_mul(vector_set(quat_get_x(input), quat_get_y(input), quat_get_z(input)), real_length);
			out_angle = abs(quat_get_w(input)) < EPSILON ? ACL_PI_32 : atan2(real_length_squared * real_length, quat_get_w(input)) * 2.0f;
		}
	}

	inline Vector4_32 quat_get_axis(const Quat_32& input)
	{
		constexpr float EPSILON = 1.0e-8f;
		constexpr float EPSILON_SQUARED = EPSILON * EPSILON;

		float real_length_squared = vector_length_squared3(quat_to_vector(input));

		if (real_length_squared < EPSILON_SQUARED)
		{
			return vector_set(1.0f, 0.0f, 0.0f);
		}
		else
		{
			float real_length = sqrt_reciprocal(real_length_squared);
			return vector_mul(vector_set(quat_get_x(input), quat_get_y(input), quat_get_z(input)), real_length);
		}
	}

	inline float quat_get_angle(const Quat_32& input)
	{
		constexpr float EPSILON = 1.0e-8f;
		constexpr float EPSILON_SQUARED = EPSILON * EPSILON;

		float real_length_squared = vector_length_squared3(quat_to_vector(input));

		if (real_length_squared < EPSILON_SQUARED)
		{
			return 0.0f;
		}
		else
		{
			float real_length = sqrt_reciprocal(real_length_squared);
			return abs(quat_get_w(input)) < EPSILON ? ACL_PI_32 : atan2(real_length_squared * real_length, quat_get_w(input)) * 2.0f;
		}
	}

	inline Quat_32 quat_from_axis_angle(const Vector4_32& axis, float angle)
	{
		float s, c;
		sincos(0.5f * angle, s, c);

		return quat_set(s * vector_get_x(axis), s * vector_get_y(axis), s * vector_get_z(axis), c);
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

	inline bool quat_near_identity(const Quat_32& input, float threshold = 0.00001f)
	{
		float angle = quat_get_angle(input);
		return abs(angle) < threshold;
	}
}
