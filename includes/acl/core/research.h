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

#include "acl/math/quat_32.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_64.h"

#include <stdint.h>

// In order to keep things clean and re-use as much logic as possible,
// a template argument that defines the relevant logic will be added
// where necessary.

namespace acl
{
	enum class ArithmeticType8 : uint8_t
	{
		Float32,
		Float64,
		FixedPoint,
	};

	template<ArithmeticType8 type>
	struct ArithmeticImpl_ {};

	template<>
	struct ArithmeticImpl_<ArithmeticType8::Float32>
	{
		static constexpr ArithmeticType8 k_type = ArithmeticType8::Float32;
		typedef Quat_32 QuatType;
		typedef Vector4_32 Vector4Type;
		typedef float ScalarType;

		static constexpr QuatType cast(const Quat_64& input) { return quat_cast(input); }
		//static constexpr QuatType cast(const Quat_32& input) { return input; }
		static constexpr Vector4Type cast(const Vector4_64& input) { return vector_cast(input); }
		static constexpr Vector4Type cast(const Vector4_32& input) { return input; }
		static constexpr float cast(double input) { return float(input); }
		static constexpr float cast(float input) { return input; }

		static constexpr Vector4Type vector_zero() { return vector_zero_32(); }
		static constexpr Vector4Type vector_unaligned_load(const uint8_t* input) { return vector_unaligned_load_32(input); }
		static constexpr Vector4Type vector_unaligned_load3(const uint8_t* input) { return vector_unaligned_load3_32(input); }
	};

	template<>
	struct ArithmeticImpl_<ArithmeticType8::Float64>
	{
		static constexpr ArithmeticType8 k_type = ArithmeticType8::Float64;
		typedef Quat_64 QuatType;
		typedef Vector4_64 Vector4Type;
		typedef double ScalarType;

		static constexpr QuatType cast(const Quat_64& input) { return input; }
		static constexpr Vector4Type cast(const Vector4_64& input) { return input; }
		static constexpr double cast(double input) { return input; }
		static constexpr double cast(float input) { return double(input); }

		static constexpr Vector4Type vector_zero() { return vector_zero_64(); }
		static constexpr Vector4Type vector_unaligned_load(const uint8_t* input) { return vector_unaligned_load_64(input); }
		static constexpr Vector4Type vector_unaligned_load3(const uint8_t* input) { return vector_unaligned_load3_64(input); }
	};

	template<>
	struct ArithmeticImpl_<ArithmeticType8::FixedPoint>
	{
		static constexpr ArithmeticType8 k_type = ArithmeticType8::FixedPoint;
	};

	constexpr ArithmeticType8 k_arithmetic_type = ArithmeticType8::Float32;

	using ArithmeticImpl = ArithmeticImpl_<k_arithmetic_type>;
	using Vector4 = ArithmeticImpl::Vector4Type;
	using Quat = ArithmeticImpl::QuatType;
	using Scalar = ArithmeticImpl::ScalarType;
}
