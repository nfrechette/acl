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
	constexpr double k_pi_64 = 3.141592654;

	inline double floor(double input)
	{
		return std::floor(input);
	}

	inline double clamp(double input, double min, double max)
	{
		return std::min(std::max(input, min), max);
	}

	inline double abs(double input)
	{
		return std::fabs(input);
	}

	inline double sqrt(double input)
	{
		return std::sqrt(input);
	}

	inline double sqrt_reciprocal(double input)
	{
		// TODO: Use recip instruction
		return 1.0 / sqrt(input);
	}

	inline double reciprocal(double input)
	{
		return 1.0 / input;
	}

	inline double sin(double angle)
	{
		return std::sin(angle);
	}

	inline double cos(double angle)
	{
		return std::cos(angle);
	}

	inline void sincos(double angle, double& out_sin, double& out_cos)
	{
		out_sin = sin(angle);
		out_cos = cos(angle);
	}

	inline double acos(double value)
	{
		return std::acos(value);
	}

	inline double atan2(double left, double right)
	{
		return std::atan2(left, right);
	}

	inline double min(double left, double right)
	{
		return std::min(left, right);
	}

	inline double max(double left, double right)
	{
		return std::max(left, right);
	}

	constexpr double deg2rad(double deg)
	{
		return (deg / 180.0) * k_pi_64;
	}

	inline bool scalar_near_equal(double lhs, double rhs, double threshold)
	{
		return abs(lhs - rhs) < threshold;
	}

	inline double is_finite(double input)
	{
		return std::isfinite(input);
	}

	inline double symmetric_round(double input)
	{
		return floor(input >= 0.0 ? (input + 0.5) : (input - 0.5));
	}

	inline double fraction(double value)
	{
		return value - floor(value);
	}

	template<typename SrcIntegralType>
	inline double safe_to_double(SrcIntegralType input)
	{
		double input_f = double(input);
		ACL_ENSURE(SrcIntegralType(input_f) == input, "Convertion to double would result in truncation");
		return input_f;
	}
}
