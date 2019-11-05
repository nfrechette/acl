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
#include "acl/math/math.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace math_impl
	{
		union Converter
		{
			double dbl;
			uint64_t u64;
			float flt[2];

			explicit constexpr Converter(uint64_t value) : u64(value) {}
			explicit constexpr Converter(double value) : dbl(value) {}
			explicit constexpr Converter(float value) : flt{value, value} {}

			constexpr operator double() const { return dbl; }
			constexpr operator float() const { return flt[0]; }
		};

		constexpr Converter get_mask_value(bool is_true)
		{
			return Converter(is_true ? uint64_t(0xFFFFFFFFFFFFFFFFULL) : uint64_t(0));
		}

		constexpr double select(double mask, double if_true, double if_false)
		{
			return Converter(mask).u64 == 0 ? if_false : if_true;
		}

		constexpr float select(float mask, float if_true, float if_false)
		{
			return Converter(mask).u64 == 0 ? if_false : if_true;
		}
	}

#if defined(ACL_SSE2_INTRINSICS)
	typedef __m128 Quat_32;
	typedef __m128 Vector4_32;

	struct Quat_64
	{
		__m128d xy;
		__m128d zw;
	};

	struct Vector4_64
	{
		__m128d xy;
		__m128d zw;
	};
#elif defined(ACL_NEON_INTRINSICS)
	typedef float32x4_t Quat_32;
	typedef float32x4_t Vector4_32;

	struct alignas(16) Quat_64
	{
		double x;
		double y;
		double z;
		double w;
	};

	struct alignas(16) Vector4_64
	{
		double x;
		double y;
		double z;
		double w;
	};
#else
	struct alignas(16) Quat_32
	{
		float x;
		float y;
		float z;
		float w;
	};

	struct alignas(16) Vector4_32
	{
		float x;
		float y;
		float z;
		float w;
	};

	struct alignas(16) Quat_64
	{
		double x;
		double y;
		double z;
		double w;
	};

	struct alignas(16) Vector4_64
	{
		double x;
		double y;
		double z;
		double w;
	};
#endif

	struct Transform_32
	{
		Quat_32		rotation;
		Vector4_32	translation;
		Vector4_32	scale;
	};

	struct Transform_64
	{
		Quat_64		rotation;
		Vector4_64	translation;
		Vector4_64	scale;
	};

	struct AffineMatrix_32
	{
		Vector4_32	x_axis;
		Vector4_32	y_axis;
		Vector4_32	z_axis;
		Vector4_32	w_axis;
	};

	struct AffineMatrix_64
	{
		Vector4_64	x_axis;
		Vector4_64	y_axis;
		Vector4_64	z_axis;
		Vector4_64	w_axis;
	};

	enum class VectorMix
	{
		X = 0,
		Y = 1,
		Z = 2,
		W = 3,

		A = 4,
		B = 5,
		C = 6,
		D = 7,
	};

	enum class MatrixAxis
	{
		X = 0,
		Y = 1,
		Z = 2,
		W = 3,
	};

	// The result is sometimes required as part of an immediate for an intrinsic
	// and as such we much know the value at compile time and constexpr isn't always evaluated.
	// Required at least on GCC 5 in Debug
	#define IS_VECTOR_MIX_ARG_XYZW(arg) (int32_t(arg) >= int32_t(VectorMix::X) && int32_t(arg) <= int32_t(VectorMix::W))
	#define IS_VECTOR_MIX_ARG_ABCD(arg) (int32_t(arg) >= int32_t(VectorMix::A) && int32_t(arg) <= int32_t(VectorMix::D))
	#define GET_VECTOR_MIX_COMPONENT_INDEX(arg) (IS_VECTOR_MIX_ARG_XYZW(arg) ? int8_t(arg) : (int8_t(arg) - 4))

	namespace math_impl
	{
		constexpr bool is_vector_mix_arg_xyzw(VectorMix arg) { return int32_t(arg) >= int32_t(VectorMix::X) && int32_t(arg) <= int32_t(VectorMix::W); }
		constexpr bool is_vector_mix_arg_abcd(VectorMix arg) { return int32_t(arg) >= int32_t(VectorMix::A) && int32_t(arg) <= int32_t(VectorMix::D); }
		constexpr int8_t get_vector_mix_component_index(VectorMix arg) { return is_vector_mix_arg_xyzw(arg) ? int8_t(arg) : (int8_t(arg) - 4); }
	}

	//////////////////////////////////////////////////////////////////////////

#if defined(ACL_USE_VECTORCALL)
	// On x64 with __vectorcall, the first 6x vector4 arguments can be passed by value in a register, everything else afterwards is passed by const&
	using Vector4_32Arg0 = const Vector4_32;
	using Vector4_32Arg1 = const Vector4_32;
	using Vector4_32Arg2 = const Vector4_32;
	using Vector4_32Arg3 = const Vector4_32;
	using Vector4_32Arg4 = const Vector4_32;
	using Vector4_32Arg5 = const Vector4_32;
	using Vector4_32ArgN = const Vector4_32&;

	using Quat_32Arg0 = const Quat_32;
	using Quat_32Arg1 = const Quat_32;
	using Quat_32Arg2 = const Quat_32;
	using Quat_32Arg3 = const Quat_32;
	using Quat_32Arg4 = const Quat_32;
	using Quat_32Arg5 = const Quat_32;
	using Quat_32ArgN = const Quat_32&;

	// With __vectorcall, vector aggregates are also passed by register
	using Transform_32Arg0 = const Transform_32;
	using Transform_32Arg1 = const Transform_32;
	using Transform_32ArgN = const Transform_32&;

	using AffineMatrix_32Arg0 = const AffineMatrix_32;
	using AffineMatrix_32ArgN = const AffineMatrix_32&;
#elif defined(ACL_NEON_INTRINSICS)
	// On ARM NEON, the first 4x vector4 arguments can be passed by value in a register, everything else afterwards is passed by const&
	using Vector4_32Arg0 = const Vector4_32;
	using Vector4_32Arg1 = const Vector4_32;
	using Vector4_32Arg2 = const Vector4_32;
	using Vector4_32Arg3 = const Vector4_32;
	using Vector4_32Arg4 = const Vector4_32&;
	using Vector4_32Arg5 = const Vector4_32&;
	using Vector4_32ArgN = const Vector4_32&;

	using Quat_32Arg0 = const Quat_32;
	using Quat_32Arg1 = const Quat_32;
	using Quat_32Arg2 = const Quat_32;
	using Quat_32Arg3 = const Quat_32;
	using Quat_32Arg4 = const Quat_32&;
	using Quat_32Arg5 = const Quat_32&;
	using Quat_32ArgN = const Quat_32&;

	using Transform_32Arg0 = const Transform_32&;
	using Transform_32Arg1 = const Transform_32&;
	using Transform_32ArgN = const Transform_32&;

	using AffineMatrix_32Arg0 = const AffineMatrix_32&;
	using AffineMatrix_32ArgN = const AffineMatrix_32&;
#else
	// On every other platform, everything is passed by const&
	using Vector4_32Arg0 = const Vector4_32&;
	using Vector4_32Arg1 = const Vector4_32&;
	using Vector4_32Arg2 = const Vector4_32&;
	using Vector4_32Arg3 = const Vector4_32&;
	using Vector4_32Arg4 = const Vector4_32&;
	using Vector4_32Arg5 = const Vector4_32&;
	using Vector4_32ArgN = const Vector4_32&;

	using Quat_32Arg0 = const Quat_32&;
	using Quat_32Arg1 = const Quat_32&;
	using Quat_32Arg2 = const Quat_32&;
	using Quat_32Arg3 = const Quat_32&;
	using Quat_32Arg4 = const Quat_32&;
	using Quat_32Arg5 = const Quat_32&;
	using Quat_32ArgN = const Quat_32&;

	using Transform_32Arg0 = const Transform_32&;
	using Transform_32Arg1 = const Transform_32&;
	using Transform_32ArgN = const Transform_32&;

	using AffineMatrix_32Arg0 = const AffineMatrix_32&;
	using AffineMatrix_32ArgN = const AffineMatrix_32&;
#endif
}

ACL_IMPL_FILE_PRAGMA_POP
