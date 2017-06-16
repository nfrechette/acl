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
		return std::sqrt(input);
	}

	inline float sqrt_reciprocal(float input)
	{
		// TODO: Use recip instruction
		return 1.0f / sqrt(input);
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

	inline float is_finite(float input)
	{
		return std::isfinite(input);
	}

	inline float symmetric_round(float input)
	{
		return floor(input >= 0.0f ? (input + 0.5f) : (input - 0.5f));
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
