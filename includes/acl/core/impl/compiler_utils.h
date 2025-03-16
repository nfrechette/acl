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

#include "acl/version.h"

#include <rtm/impl/compiler_utils.h>

#include <cstdlib>
#include <type_traits>

//////////////////////////////////////////////////////////////////////////
// Because this library is made entirely of headers, we have no control over the
// compilation flags used. However, in some cases, certain options must be forced.
// To do this, every header is wrapped in two macros to push and pop the necessary
// pragmas.
//
// Options we use:
//    - Disable fast math, it can hurt precision for little to no performance gain due to the high level of hand tuned optimizations.
//////////////////////////////////////////////////////////////////////////
#if defined(RTM_COMPILER_MSVC)
	#define ACL_IMPL_FILE_PRAGMA_PUSH \
		__pragma(float_control(precise, on, push))

	#define ACL_IMPL_FILE_PRAGMA_POP \
		__pragma(float_control(pop))
#elif defined(RTM_COMPILER_CLANG) && 0
	// For some reason, clang doesn't appear to support disabling fast-math through pragmas
	// See: https://github.com/llvm/llvm-project/issues/55392
	#define ACL_IMPL_FILE_PRAGMA_PUSH \
		_Pragma("float_control(precise, on, push)")

	#define ACL_IMPL_FILE_PRAGMA_POP \
		_Pragma("float_control(pop)")
#elif defined(RTM_COMPILER_GCC)
	#define ACL_IMPL_FILE_PRAGMA_PUSH \
		_Pragma("GCC push_options") \
		_Pragma("GCC optimize (\"no-fast-math\")")

	#define ACL_IMPL_FILE_PRAGMA_POP \
		_Pragma("GCC pop_options")
#else
	#define ACL_IMPL_FILE_PRAGMA_PUSH
	#define ACL_IMPL_FILE_PRAGMA_POP
#endif

//////////////////////////////////////////////////////////////////////////
// Wraps the __has_feature pre-processor macro to handle non-clang and early
// GCC compilers
//////////////////////////////////////////////////////////////////////////
#if defined(__has_feature)
	#define ACL_HAS_FEATURE(x) __has_feature(x)
#else
	#define ACL_HAS_FEATURE(x) 0
#endif

//////////////////////////////////////////////////////////////////////////
// Wraps the __has_builtin pre-processor macro to handle non-clang and early
// GCC compilers
//////////////////////////////////////////////////////////////////////////
#if defined(__has_builtin)
	#define ACL_HAS_BUILTIN(x) __has_builtin(x)
#else
	#define ACL_HAS_BUILTIN(x) 0
#endif

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// std::strtoull isn't available in stdlibs that partially support C++11
		// so we use the old C API
		using ::strtoull;

		// std::is_trivially_default_constructible is not always available in older stdlibs
		// around GCC 4.9 and Clang 4. To avoid this, we use their built-in feature testing
		// and we polyfill it.
#if ACL_HAS_FEATURE(__is_trivially_constructible) || ACL_HAS_BUILTIN(__is_trivially_constructible)
		template <class Type>
		struct is_trivially_default_constructible
		{
			static constexpr bool value = __is_trivially_constructible(Type);
		};
#elif ACL_HAS_FEATURE(__has_trivial_constructor) || ACL_HAS_BUILTIN(__has_trivial_constructor) || defined(__GNUG__)
		template <class Type>
		struct is_trivially_default_constructible
		{
			static constexpr bool value = __has_trivial_constructor(Type);
		};
#elif RTM_CPP_VERSION >= RTM_CPP_VERSION_11
		template <class Type>
		using is_trivially_default_constructible = std::is_trivially_default_constructible<Type>;
#else
		template <class Type>
		struct is_trivially_default_constructible
		{
			// Unknown or older compiler, assume we aren't trivially constructible
			static constexpr bool value = false;
		};
#endif
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

//////////////////////////////////////////////////////////////////////////
// Wraps the __has_attribute pre-processor macro to handle non-clang and early
// GCC compilers
//////////////////////////////////////////////////////////////////////////
#if defined(__has_attribute)
	#define ACL_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
	#define ACL_HAS_ATTRIBUTE(x) 0
#endif

//////////////////////////////////////////////////////////////////////////
// Silence compiler warnings within switch cases that fall through
// Note: C++17 has [[fallthrough]];
//////////////////////////////////////////////////////////////////////////
#if ACL_HAS_ATTRIBUTE(fallthrough)
	#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL __attribute__ ((fallthrough))
#else
	#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL (void)0
#endif

// When enabled, constant sub-tracks will use the weighted average of every sample instead of the first sample
// Disabled by default, most clips have no measurable gain but some clips suffer greatly, needs to be investigated, possibly a bug somewhere
// Note: Code has been removed in the pull request that closes: https://github.com/nfrechette/acl/issues/353
//#define ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS
