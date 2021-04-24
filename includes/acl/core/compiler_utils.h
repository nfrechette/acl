#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
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

#include <type_traits>

#if !defined(__GNUG__) || defined(_LIBCPP_VERSION) || defined(_GLIBCXX_USE_CXX11_ABI)
	#include <cstdlib>
#else
	#include <stdlib.h>
#endif

//////////////////////////////////////////////////////////////////////////
// Because this library is made entirely of headers, we have no control over the
// compilation flags used. However, in some cases, certain options must be forced.
// To do this, every header is wrapped in two macros to push and pop the necessary
// pragmas.
//////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER)
	#define ACL_IMPL_FILE_PRAGMA_PUSH \
		/* Disable fast math, it can hurt precision for little to no performance gain due to the high level of hand tuned optimizations. */ \
		__pragma(float_control(precise, on, push))

	#define ACL_IMPL_FILE_PRAGMA_POP \
		__pragma(float_control(pop))
#else
	#define ACL_IMPL_FILE_PRAGMA_PUSH
	#define ACL_IMPL_FILE_PRAGMA_POP
#endif

//////////////////////////////////////////////////////////////////////////
// Force inline macros for when it is necessary.
//////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER)
	#define ACL_FORCE_INLINE __forceinline
#elif defined(__GNUG__) || defined(__clang__)
	#define ACL_FORCE_INLINE __attribute__((always_inline)) inline
#else
	#define ACL_FORCE_INLINE inline
#endif

//////////////////////////////////////////////////////////////////////////
//
// Stock ACL fails unit tests after including missing edge cases, and adding some new ones.
//
//////////////////////////////////////////////////////////////////////////
#define ACL_UNIT_TEST

//////////////////////////////////////////////////////////////////////////
//
// Integers between 0 and 2^24 are 100% accurate as floats. Leverage this with a maximum quantization of 24 bits.
//
// Floating point */ with 2^x is precision-friendly.  It shifts the exponent without touching the mantissa.  This drives our quantization.
//
// Normalizing to 0.0f..1.0F is less accurate than normalizing to -0.5F..0.5F.  The latter range can handle 1/(2^25), which is the error term of 24 bit quantization.
//
// If our goal was to minimize error within the range, we'd maximize error at the endpoints, so we could stop here.  However, ACL expects precise endpoints, so we modify the
// scale accordingly.  Note that division is more accurate than multiply-by-reciprocal when the divisor isn't a power of 2, so we monitor discretization error closely.
//
// Always floor after scaling, and before shifting from -halfQ..halfQ to 0..fullQ.  Otherwise, IEEE float addition will round the result before you get a chance to floor it.
//////////////////////////////////////////////////////////////////////////
#define ACL_PACKING

//////////////////////////////////////////////////////////////////////////
//
// Expand bit rate options, from [3..19] to [1..24].
//
//////////////////////////////////////////////////////////////////////////
//#define ACL_BIT_RATE

//////////////////////////////////////////////////////////////////////////
//
// Change the definition of "default transform" from "identity transform" to "bind transform", and presume that the bind pose has been set
// before decompression.
//
//////////////////////////////////////////////////////////////////////////
#define ACL_BIND_POSE
#ifdef ACL_BIND_POSE
#define	IF_ACL_BIND_POSE(...) __VA_ARGS__
#else
#define	IF_ACL_BIND_POSE(...)
#endif

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// The version of the STL shipped with versions of GCC older than 5.1 are missing a number of type traits and functions,
	// such as std::is_trivially_default_constructible.
	// In this case, we polyfill the proper standard names using the deprecated std::has_trivial_default_constructor.
	// This must also be done when the compiler is clang when it makes use of the GCC implementation of the STL,
	// which is the default behavior on linux. Properly detecting the version of the GCC STL used by clang cannot
	// be done with the __GNUC__  macro, which are overridden by clang. Instead, we check for the definition
	// of the macro ``_GLIBCXX_USE_CXX11_ABI`` which is only defined with GCC versions greater than 5.
	//////////////////////////////////////////////////////////////////////////
	namespace acl_impl
	{
#if !defined(__GNUG__) || defined(_LIBCPP_VERSION) || defined(_GLIBCXX_USE_CXX11_ABI)
		using std::strtoull;

		template <class Type>
		using is_trivially_default_constructible = std::is_trivially_default_constructible<Type>;
#else
		using ::strtoull;

		template <class Type>
		using is_trivially_default_constructible = std::has_trivial_default_constructor<Type>;
#endif
	}
}
