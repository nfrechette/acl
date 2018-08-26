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

//#define ACL_NO_INTRINSICS

#if !defined(ACL_NO_INTRINSICS)
	#if defined(__AVX__)
		#define ACL_AVX_INTRINSICS
		#define ACL_SSE4_INTRINSICS
		#define ACL_SSE3_INTRINSICS
		#define ACL_SSE2_INTRINSICS
	#endif

	#if defined(__SSE4_1__)
		#define ACL_SSE4_INTRINSICS
		#define ACL_SSE3_INTRINSICS
		#define ACL_SSE2_INTRINSICS
	#endif

	#if defined(__SSSE3__)
		#define ACL_SSE3_INTRINSICS
		#define ACL_SSE2_INTRINSICS
	#endif

	#if defined(__SSE2__) || defined(_M_IX86) || defined(_M_X64)
		#define ACL_SSE2_INTRINSICS
	#endif

	#if defined(__ARM_NEON)
		#define ACL_NEON_INTRINSICS

		#if defined(__aarch64__)
			#define ACL_NEON64_INTRINSICS
		#endif
	#endif

	#if !defined(ACL_SSE2_INTRINSICS) && !defined(ACL_NEON_INTRINSICS)
		#define ACL_NO_INTRINSICS
	#endif
#endif

#if defined(ACL_SSE2_INTRINSICS)
	#include <xmmintrin.h>
	#include <emmintrin.h>

	// With MSVC and SSE2, we can use the __vectorcall calling convention to pass vector types and aggregates by value through registers
	// for improved code generation
	#if defined(_MSC_VER) && !defined(_M_ARM) && !defined(_MANAGED) && !defined(_M_CEE) && (!defined(_M_IX86_FP) || (_M_IX86_FP > 1)) && !defined(ACL_SIMD_CALL)
		#if ((_MSC_FULL_VER >= 170065501) && (_MSC_VER < 1800)) || (_MSC_FULL_VER >= 180020418)
			#define ACL_USE_VECTORCALL
		#endif
	#endif
#endif

#if defined(ACL_SSE3_INTRINSICS)
	#include <pmmintrin.h>
#endif

#if defined(ACL_SSE4_INTRINSICS)
	#include <smmintrin.h>
#endif

#if defined(ACL_AVX_INTRINSICS)
	#include <immintrin.h>
#endif

#if defined(ACL_NEON_INTRINSICS)
	#include <arm_neon.h>
#endif

#if !defined(ACL_SIMD_CALL)
	#if defined(ACL_USE_VECTORCALL)
		#define ACL_SIMD_CALL __vectorcall
	#else
		#define ACL_SIMD_CALL
	#endif
#endif

#include "acl/math/math_types.h"
