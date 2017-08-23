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

#include <algorithm>
#include <cmath>

namespace acl
{
	// TODO: Get a higher precision number
	static constexpr float ACL_PI_32 = 3.141592654f;

	inline float floor(float input)
	{
		return std::floor(input);
	}

	inline float clamp(float input, float min, float max)
	{
		return std::min(std::max(input, min), max);
	}

	inline float sqrt(float input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ps1(input)));
#else
		return std::sqrt(input);
#endif
	}

	inline float sqrt_reciprocal(float input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		// Perform two passes of Newton-Raphson iteration on the hardware estimate
		__m128 input_v = _mm_set_ps1(input);
		__m128 half = _mm_set_ps1(0.5f);
		__m128 input_half_v = _mm_mul_ss(input_v, half);
		__m128 x0 = _mm_rsqrt_ss(input_v);

		// First iteration
		__m128 x1 = _mm_mul_ss(x0, x0);
		x1 = _mm_sub_ss(half, _mm_mul_ss(input_half_v, x1));
		x1 = _mm_add_ss(_mm_mul_ss(x0, x1), x0);

		// Second iteration
		__m128 x2 = _mm_mul_ss(x1, x1);
		x2 = _mm_sub_ss(half, _mm_mul_ss(input_half_v, x2));
		x2 = _mm_add_ss(_mm_mul_ss(x1, x2), x1);

		return _mm_cvtss_f32(x2);
#else
		return 1.0f / sqrt(input);
#endif
	}

	inline float reciprocal(float input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		// Perform two passes of Newton-Raphson iteration on the hardware estimate
		__m128 input_v = _mm_set_ps1(input);
		__m128 x0 = _mm_rcp_ss(input_v);

		// First iteration
		__m128 x1 = _mm_sub_ss(_mm_add_ss(x0, x0), _mm_mul_ss(input_v, _mm_mul_ss(x0, x0)));

		// Second iteration
		__m128 x2 = _mm_sub_ss(_mm_add_ss(x1, x1), _mm_mul_ss(input_v, _mm_mul_ss(x1, x1)));

		return _mm_cvtss_f32(x2);
#else
		return 1.0f / input;
#endif
	}

	inline float sin(float angle)
	{
		return std::sin(angle);
	}

	inline float cos(float angle)
	{
		return std::cos(angle);
	}

	inline void sincos(float angle, float& out_sin, float& out_cos)
	{
		out_sin = sin(angle);
		out_cos = cos(angle);
	}

	inline float acos(float value)
	{
		return std::acos(value);
	}

	inline float atan2(float left, float right)
	{
		return std::atan2(left, right);
	}

	inline float min(float left, float right)
	{
		return std::min(left, right);
	}

	inline float max(float left, float right)
	{
		return std::max(left, right);
	}

	constexpr float deg2rad(float deg)
	{
		return (deg / 180.0f) * ACL_PI_32;
	}

	inline bool scalar_near_equal(float lhs, float rhs, float threshold)
	{
		return abs(lhs - rhs) < threshold;
	}

	inline float is_finite(float input)
	{
		return std::isfinite(input);
	}

	inline float symmetric_round(float input)
	{
		return floor(input >= 0.0f ? (input + 0.5f) : (input - 0.5f));
	}

	inline float fraction(float value)
	{
		return value - floor(value);
	}

	template<typename SrcIntegralType>
	inline float safe_to_float(SrcIntegralType input)
	{
		float input_f = float(input);
		ACL_ENSURE(SrcIntegralType(input_f) == input, "Convertion to float would result in truncation");
		return input_f;
	}

	inline void scalar_unaligned_load(const uint8_t* src, float& dest)
	{
		// TODO: Safe with SSE?
		uint8_t* dest_u8 = reinterpret_cast<uint8_t*>(&dest);
		for (size_t byte_index = 0; byte_index < sizeof(float); ++byte_index)
			dest_u8[byte_index] = src[byte_index];
	}

	inline void scalar_unaligned_write(const float& src, uint8_t* dest)
	{
		// TODO: Safe with SSE?
		const uint8_t* src_u8 = reinterpret_cast<const uint8_t*>(&src);
		for (size_t byte_index = 0; byte_index < sizeof(float); ++byte_index)
			dest[byte_index] = src_u8[byte_index];
	}
}
