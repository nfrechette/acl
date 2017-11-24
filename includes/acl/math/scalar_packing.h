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
#include "acl/core/research.h"
#include "acl/math/scalar_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_64.h"

#include <stdint.h>

namespace acl
{
	inline size_t pack_scalar_unsigned(Scalar input, size_t num_bits)
	{
		ACL_ENSURE(input >= Scalar(0.0) && input <= Scalar(1.0), "Invalue input value: 0.0 <= %f <= 1.0", input);
		size_t max_value = (1 << num_bits) - 1;
		return static_cast<size_t>(symmetric_round(input * safe_to_float(max_value)));
	}

	inline Scalar unpack_scalar_unsigned(size_t input, size_t num_bits)
	{
		size_t max_value = (1 << num_bits) - 1;
		ACL_ENSURE(input <= max_value, "Invalue input value: %ull <= 1.0", input);
		return safe_to_float(input) / safe_to_float(max_value);
	}

	inline size_t pack_scalar_signed(Scalar input, size_t num_bits)
	{
		ACL_ENSURE(input >= Scalar(-1.0) && input <= Scalar(1.0), "Invalue input value: -1.0 <= %f <= 1.0", input);
		return pack_scalar_unsigned((input * Scalar(0.5)) + Scalar(0.5), num_bits);
	}

	inline Scalar unpack_scalar_signed(size_t input, size_t num_bits)
	{
		return (unpack_scalar_unsigned(input, num_bits) * Scalar(2.0)) - Scalar(1.0);
	}

	inline size_t pack_scalar_unsigned_24(Scalar input)
	{
		// float32 math is too inaccurate to quantize properly, use float64 math
		ACL_ENSURE(input >= Scalar(0.0) && input <= Scalar(1.0), "Invalue input value: 0.0 <= %f <= 1.0", input);
		input = input == Scalar(1.0) ? (input - std::numeric_limits<Scalar>::epsilon()) : input;
		Vector4_64 input_v = vector_set(double(input));
		Vector4_64 input_scaled = vector_mul(input_v, vector_set(1.0, 255.0, 255.0 * 255.0, 255.0 * 255.0 * 255.0));
		Vector4_64 input_fraction = vector_fraction(input_scaled);
		Vector4_64 input_bias = vector_div(vector_set(vector_get_y(input_fraction), vector_get_z(input_fraction), vector_get_w(input_fraction)), vector_set(255.0));
		Vector4_64 input_f64 = vector_sub(input_fraction, input_bias);
		Vector4_64 input_u8 = vector_mul(input_f64, vector_set(255.0));
		size_t x = static_cast<size_t>(symmetric_round(vector_get_x(input_u8)));
		size_t y = static_cast<size_t>(symmetric_round(vector_get_y(input_u8)));
		size_t z = static_cast<size_t>(symmetric_round(vector_get_z(input_u8)));
		return (x << 16) | (y << 8) | z;
	}

	inline Scalar unpack_scalar_unsigned_24(size_t input)
	{
		size_t x = input >> 16;
		size_t y = (input >> 8) & 0xFF;
		size_t z = input & 0xFF;
		Vector4 input_u8 = vector_set(Scalar(x), Scalar(y), Scalar(z));
		Vector4 input_fraction = vector_div(input_u8, vector_set(Scalar(255.0)));
		return vector_dot3(input_fraction, vector_set(Scalar(1.0), Scalar(1.0 / 255.0), Scalar(1.0 / (255.0 * 255.0))));
	}

	inline size_t pack_scalar_signed_24(Scalar input)
	{
		ACL_ENSURE(input >= Scalar(-1.0) && input <= Scalar(1.0), "Invalue input value: -1.0 <= %f <= 1.0", input);
		return pack_scalar_unsigned_24((input * Scalar(0.5)) + Scalar(0.5));
	}

	inline Scalar unpack_scalar_signed_24(size_t input)
	{
		return (unpack_scalar_unsigned_24(input) * Scalar(2.0)) - Scalar(1.0);
	}
}
