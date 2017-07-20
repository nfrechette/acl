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
#include "acl/math/scalar_64.h"
#include "acl/math/vector4_64.h"

namespace acl
{
	inline Quat_64 quat_set(double x, double y, double z, double w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Quat_64{ _mm_set_pd(y, x), _mm_set_pd(w, z) };
#else
		return Quat_64{ x, y, z, w };
#endif
	}

	inline Quat_64 quat_unaligned_load(const double* input)
	{
		return quat_set(input[0], input[1], input[2], input[3]);
	}

	inline Quat_64 quat_identity_64()
	{
		return quat_set(0.0, 0.0, 0.0, 1.0);
	}

	inline Quat_64 vector_to_quat(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Quat_64{ input.xy, input.zw };
#else
		return Quat_64{ input.x, input.y, input.z, input.w };
#endif
	}

	inline Quat_64 quat_cast(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Quat_64{ _mm_cvtps_pd(input), _mm_cvtps_pd(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 2, 3, 2))) };
#else
		return Quat_64{ double(input.x), double(input.y), double(input.z), double(input.w) };
#endif
	}

	inline double quat_get_x(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.xy);
#else
		return input.x;
#endif
	}

	inline double quat_get_y(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.xy, input.xy, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline double quat_get_z(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.zw);
#else
		return input.z;
#endif
	}

	inline double quat_get_w(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.zw, input.zw, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.w;
#endif
	}

	inline Quat_64 quat_conjugate(const Quat_64& input)
	{
		return quat_set(-quat_get_x(input), -quat_get_y(input), -quat_get_z(input), quat_get_w(input));
	}

	// Multiplication order is as follow: local_to_world = quat_mul(local_to_object, object_to_world)
	inline Quat_64 quat_mul(const Quat_64& lhs, const Quat_64& rhs)
	{
		double lhs_x = quat_get_x(lhs);
		double lhs_y = quat_get_y(lhs);
		double lhs_z = quat_get_z(lhs);
		double lhs_w = quat_get_w(lhs);

		double rhs_x = quat_get_x(rhs);
		double rhs_y = quat_get_y(rhs);
		double rhs_z = quat_get_z(rhs);
		double rhs_w = quat_get_w(rhs);

		double x = (rhs_w * lhs_x) + (rhs_x * lhs_w) + (rhs_y * lhs_z) - (rhs_z * lhs_y);
		double y = (rhs_w * lhs_y) - (rhs_x * lhs_z) + (rhs_y * lhs_w) + (rhs_z * lhs_x);
		double z = (rhs_w * lhs_z) + (rhs_x * lhs_y) - (rhs_y * lhs_x) + (rhs_z * lhs_w);
		double w = (rhs_w * lhs_w) - (rhs_x * lhs_x) - (rhs_y * lhs_y) - (rhs_z * lhs_z);

		return quat_set(x, y, z, w);
	}

	inline Vector4_64 quat_rotate(const Quat_64& rotation, const Vector4_64& vector)
	{
		Quat_64 vector_quat = vector_to_quat(vector_mul(vector, vector_set(1.0, 1.0, 1.0, 0.0)));
		Quat_64 inv_rotation = quat_conjugate(rotation);
		return quat_to_vector(quat_mul(quat_mul(inv_rotation, vector_quat), rotation));
	}

	inline void quat_to_axis_angle(const Quat_64& input, Vector4_64& out_axis, double& out_angle)
	{
		constexpr double EPSILON = 1.0e-8;
		constexpr double EPSILON_SQUARED = EPSILON * EPSILON;

		double real_length_squared = vector_length_squared3(quat_to_vector(input));

		if (real_length_squared < EPSILON_SQUARED)
		{
			out_axis = vector_set(1.0, 0.0, 0.0);
			out_angle = 0.0;
		}
		else
		{
			double real_length = sqrt_reciprocal(real_length_squared);
			out_axis = vector_mul(vector_set(quat_get_x(input), quat_get_y(input), quat_get_z(input)), real_length);
			out_angle = abs(quat_get_w(input)) < EPSILON ? ACL_PI_64 : atan2(real_length_squared * real_length, quat_get_w(input)) * 2.0;
		}
	}

	inline Vector4_64 quat_get_axis(const Quat_64& input)
	{
		constexpr double EPSILON = 1.0e-8;
		constexpr double EPSILON_SQUARED = EPSILON * EPSILON;

		double real_length_squared = vector_length_squared3(quat_to_vector(input));

		if (real_length_squared < EPSILON_SQUARED)
		{
			return vector_set(1.0, 0.0, 0.0);
		}
		else
		{
			double real_length = sqrt_reciprocal(real_length_squared);
			return vector_mul(vector_set(quat_get_x(input), quat_get_y(input), quat_get_z(input)), real_length);
		}
	}

	inline double quat_get_angle(const Quat_64& input)
	{
		constexpr double EPSILON = 1.0e-8;
		constexpr double EPSILON_SQUARED = EPSILON * EPSILON;

		double real_length_squared = vector_length_squared3(quat_to_vector(input));

		if (real_length_squared < EPSILON_SQUARED)
		{
			return 0.0;
		}
		else
		{
			double real_length = sqrt_reciprocal(real_length_squared);
			return abs(quat_get_w(input)) < EPSILON ? ACL_PI_64 : atan2(real_length_squared * real_length, quat_get_w(input)) * 2.0;
		}
	}

	inline Quat_64 quat_from_axis_angle(const Vector4_64& axis, double angle)
	{
		double s, c;
		sincos(0.5 * angle, s, c);

		return quat_set(s * vector_get_x(axis), s * vector_get_y(axis), s * vector_get_z(axis), c);
	}

	inline Quat_64 quat_from_euler(double pitch, double yaw, double roll)
	{
		double sp, sy, sr;
		double cp, cy, cr;

		sincos(pitch * 0.5, sp, cp);
		sincos(yaw * 0.5, sy, cy);
		sincos(roll * 0.5, sr, cr);

		return quat_set(cr * sp * sy - sr * cp * cy,
						-cr * sp * cy - sr * cp * sy,
						cr * cp * sy - sr * sp * cy,
						cr * cp * cy + sr * sp * sy);
	}

	inline double quat_length_squared(const Quat_64& input)
	{
		// TODO: Use dot instruction
		return (quat_get_x(input) * quat_get_x(input)) + (quat_get_y(input) * quat_get_y(input)) + (quat_get_z(input) * quat_get_z(input)) + (quat_get_w(input) * quat_get_w(input));
	}

	inline double quat_length(const Quat_64& input)
	{
		// TODO: Use intrinsics to avoid scalar coercion
		return sqrt(quat_length_squared(input));
	}

	inline double quat_length_reciprocal(const Quat_64& input)
	{
		// TODO: Use recip instruction
		return 1.0 / quat_length(input);
	}

	inline Quat_64 quat_normalize(const Quat_64& input)
	{
		// TODO: Use high precision recip sqrt function and vector_mul
		double length = quat_length(input);
		//float length_recip = quat_length_reciprocal(input);
		Vector4_64 input_vector = quat_to_vector(input);
		//return vector_to_quat(vector_mul(input_vector, length_recip));
		return vector_to_quat(vector_div(input_vector, vector_set(length)));
	}

	inline Quat_64 quat_lerp(const Quat_64& start, const Quat_64& end, double alpha)
	{
		// To ensure we take the shortest path, we apply a bias if the dot product is negative
		Vector4_64 start_vector = quat_to_vector(start);
		Vector4_64 end_vector = quat_to_vector(end);
		double dot = vector_dot(start_vector, end_vector);
		double bias = dot >= 0.0 ? 1.0 : -1.0;
		// TODO: Test with this instead: Rotation = (B * Alpha) + (A * (Bias * (1.f - Alpha)));
		Vector4_64 value = vector_add(start_vector, vector_mul(vector_sub(vector_mul(end_vector, bias), start_vector), alpha));
		//Vector4_64 value = vector_add(vector_mul(end_vector, alpha), vector_mul(start_vector, bias * (1.0 - alpha)));
		return quat_normalize(vector_to_quat(value));
	}

	inline Quat_64 quat_neg(const Quat_64& input)
	{
		return vector_to_quat(vector_mul(quat_to_vector(input), -1.0));
	}

	inline Quat_64 quat_ensure_positive_w(const Quat_64& input)
	{
		return quat_get_w(input) >= 0.0 ? input : quat_neg(input);
	}

	inline Quat_64 quat_from_positive_w(const Vector4_64& input)
	{
		double w_squared = 1.0 - vector_length_squared3(input);
		double w = w_squared > 0.0 ? sqrt(w_squared) : 0.0;
		return quat_set(vector_get_x(input), vector_get_y(input), vector_get_z(input), w);
	}

	inline bool quat_is_valid(const Quat_64& input)
	{
		return is_finite(quat_get_x(input)) && is_finite(quat_get_y(input)) && is_finite(quat_get_z(input)) && is_finite(quat_get_w(input));
	}

	inline bool quat_is_normalized(const Quat_64& input, double threshold = 0.00001)
	{
		double length_squared = quat_length_squared(input);
		return abs(length_squared - 1.0) < threshold;
	}

	inline bool quat_near_equal(const Quat_64& lhs, const Quat_64& rhs, double threshold = 0.00001)
	{
		return vector_near_equal(quat_to_vector(lhs), quat_to_vector(rhs), threshold);
	}

	inline bool quat_near_identity(const Quat_64& input, double threshold = 0.00001)
	{
		double angle = quat_get_angle(input);
		return abs(angle) < threshold;
	}
}
