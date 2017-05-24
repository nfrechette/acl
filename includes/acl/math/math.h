#pragma once

//#define ACL_NO_INTRINSICS

#if defined(__AVX__) && !defined(ACL_NO_INTRINSICS)
	#define ACL_AVX_INTRINSICS
	#define ACL_SSE4_INTRINSICS
	#define ACL_SSE3_INTRINSICS
	#define ACL_SSE2_INTRINSICS
#endif

#if !defined(ACL_SSE2_INTRINSICS) && !defined(ACL_NO_INTRINSICS)
	#if defined(_M_IX86) || defined(_M_X64)
		#define ACL_SSE2_INTRINSICS
	//#elif defined(_M_ARM) || defined(_M_ARM64)
		// TODO: Implement ARM NEON
		//#define ACL_ARM_NEON_INTRINSICS
	#else
		#define ACL_NO_INTRINSICS
	#endif
#endif

#if defined(ACL_SSE2_INTRINSICS)
#include <xmmintrin.h>
#include <emmintrin.h>
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

#include "acl/math/math_types.h"
