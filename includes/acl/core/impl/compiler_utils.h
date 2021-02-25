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

#include <rtm/impl/compiler_utils.h>

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
#if defined(RTM_COMPILER_MSVC)
	#define ACL_IMPL_FILE_PRAGMA_PUSH \
		/* Disable fast math, it can hurt precision for little to no performance gain due to the high level of hand tuned optimizations. */ \
		__pragma(float_control(precise, on, push))

	#define ACL_IMPL_FILE_PRAGMA_POP \
		__pragma(float_control(pop))
#else
	#define ACL_IMPL_FILE_PRAGMA_PUSH
	#define ACL_IMPL_FILE_PRAGMA_POP
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

//////////////////////////////////////////////////////////////////////////
// Silence compiler warnings within switch cases that fall through
// Note: C++17 has [[fallthrough]];
//////////////////////////////////////////////////////////////////////////
#if defined(RTM_COMPILER_GCC) || defined(RTM_COMPILER_CLANG)
	#if defined(__has_attribute) && __has_attribute(fallthrough)
		#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL __attribute__ ((fallthrough))
	#else
		#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL (void)0
	#endif
#else
	#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL (void)0
#endif
