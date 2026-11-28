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

#include "acl/config.h"
#include "acl/version.h"

#include <rtm/impl/compiler_utils.h>
#include <rtm/impl/detect_cpp_version.h>

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
//    - Disable floating point contraction (Clang only), the compiler must not merge a multiply and an add into one FMA instruction.
//      See docs/misc_integration_details.md for the flags that give the same results on all the targets.
//////////////////////////////////////////////////////////////////////////
#if defined(RTM_COMPILER_MSVC)
	#define ACL_IMPL_FILE_PRAGMA_PUSH \
		__pragma(float_control(precise, on, push))

	#define ACL_IMPL_FILE_PRAGMA_POP \
		__pragma(float_control(pop))
#elif defined(RTM_COMPILER_CLANG)
	// Clang supports this pragma only on some targets: x86 and x64, AArch64 (clang 16+), and RISC-V (clang 18+).
	// On other targets, clang ignores the pragma and gives a warning. We disable that warning.
	// See: https://github.com/llvm/llvm-project/issues/55392
	//
	// Limits of the pragma under clang:
	//    - Clang defines most SSE and NEON intrinsics with operators in system headers.
	//      These headers are outside of our pragma region and fast math stays on for them.
	//    - The pragma does not disable -ffinite-math-only. Clang can still assume that values are not NaN or infinity.
	//    - The pragma does not disable floating point contraction. By default (-ffp-contract=on), Clang merges a multiply
	//      and an add in one expression into one FMA instruction when the target has FMA (for example ARM64).
	//      The FMA does not round the product, thus the result can change with the compiler flags and the target.
	//
	// We also set "clang fp contract(off)" to stop the contraction. float_control(pop) restores the previous mode.
	// On the targets where clang ignores float_control, the pop does nothing and the mode stays off in the code
	// that includes our headers. Thus we set the mode only on the targets that support float_control.
	// Apple clang 15 uses LLVM 16.
	//
	// With -ffp-contract=fast, LLVM clang ignores the pragma and merges the operations of our functions after
	// they are inlined. Use -ffp-contract=fast-honor-pragmas instead. Apple clang honors the pragma in both modes.
	#if defined(__x86_64__) || defined(__i386__) \
		|| (defined(__aarch64__) && defined(__apple_build_version__) && __clang_major__ >= 15) \
		|| (defined(__aarch64__) && !defined(__apple_build_version__) && __clang_major__ >= 16) \
		|| (defined(__riscv) && !defined(__apple_build_version__) && __clang_major__ >= 18)
		#define ACL_IMPL_CLANG_FP_CONTRACT_OFF _Pragma("clang fp contract(off)")
	#else
		#define ACL_IMPL_CLANG_FP_CONTRACT_OFF
	#endif

	#define ACL_IMPL_FILE_PRAGMA_PUSH \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wignored-pragmas\"") \
		_Pragma("float_control(precise, on, push)") \
		ACL_IMPL_CLANG_FP_CONTRACT_OFF \
		_Pragma("clang diagnostic pop")

	#define ACL_IMPL_FILE_PRAGMA_POP \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wignored-pragmas\"") \
		_Pragma("float_control(pop)") \
		_Pragma("clang diagnostic pop")
#elif defined(RTM_COMPILER_GCC)
	// GCC defaults to -ffp-contract=fast in C++. With that option, GCC can merge a multiply and an add into one
	// FMA instruction. The FMA does not round the product, thus the result can change with the flags and the target.
	//
	// We do not set fp-contract=off here. With -ffp-contract=fast, GCC merges the operations of our functions
	// after it inlines them in the code of the caller: the pragma does not apply there. Also, GCC does not inline
	// a function when its optimize options are not the same as those of the caller ("optimization level attribute
	// mismatch"). Thus the pragma makes the code slower and does not stop the contraction.
	// To get the same results on all the targets, compile with -ffp-contract=off.
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
// ACL code must still read the deprecated values that the user can set.
// That code must not give deprecation warnings: we wrap it in these two macros.
// The user code that sets a deprecated value still gives a warning.
//////////////////////////////////////////////////////////////////////////
#if defined(RTM_COMPILER_MSVC)
	#define ACL_IMPL_DEPRECATION_WARNING_PUSH \
		__pragma(warning(push)) \
		__pragma(warning(disable : 4996))

	#define ACL_IMPL_DEPRECATION_WARNING_POP \
		__pragma(warning(pop))
#elif defined(RTM_COMPILER_CLANG)
	#define ACL_IMPL_DEPRECATION_WARNING_PUSH \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")

	#define ACL_IMPL_DEPRECATION_WARNING_POP \
		_Pragma("clang diagnostic pop")
#elif defined(RTM_COMPILER_GCC)
	#define ACL_IMPL_DEPRECATION_WARNING_PUSH \
		_Pragma("GCC diagnostic push") \
		_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

	#define ACL_IMPL_DEPRECATION_WARNING_POP \
		_Pragma("GCC diagnostic pop")
#else
	#define ACL_IMPL_DEPRECATION_WARNING_PUSH
	#define ACL_IMPL_DEPRECATION_WARNING_POP
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
// Wraps the __has_attribute and __has_cpp_attribute pre-processor macros
// to allow for C++ language feature detection
//////////////////////////////////////////////////////////////////////////
#if defined(__has_cpp_attribute)
	#define ACL_HAS_ATTRIBUTE(x) __has_cpp_attribute(x)
#elif defined(__has_attribute)
	#define ACL_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
	#define ACL_HAS_ATTRIBUTE(x) 0
#endif

//////////////////////////////////////////////////////////////////////////
// Silence compiler warnings within switch cases that fall through
//////////////////////////////////////////////////////////////////////////
#if RTM_CPP_VERSION >= RTM_CPP_VERSION_17
	#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL [[fallthrough]]
#elif ACL_HAS_ATTRIBUTE(fallthrough) && (defined(RTM_COMPILER_GCC) || defined(RTM_COMPILER_CLANG))
	// For pre-C++17 support in GCC/Clang
	#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL __attribute__ ((fallthrough))
#else
	#define ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL (void)0
#endif

//////////////////////////////////////////////////////////////////////////
// Marks a function whose return value must be used.
// With C++17 and later, the compiler gives a warning when the caller does not use the return value.
// With an earlier version, the macro is empty.
// To use a different attribute, or none, define ACL_NO_DISCARD before you include ACL.
//////////////////////////////////////////////////////////////////////////
#if !defined(ACL_NO_DISCARD)
	#if RTM_CPP_VERSION >= RTM_CPP_VERSION_17
		#define ACL_NO_DISCARD [[nodiscard]]
	#else
		#define ACL_NO_DISCARD
	#endif
#endif

//////////////////////////////////////////////////////////////////////////
// Allows force inlined functions to be debugged temporarily by disabling inlining
//////////////////////////////////////////////////////////////////////////
#if defined(ACL_IMPL_ENABLE_DEBUG_FORCE_INLINE)
	#define ACL_IMPL_DEBUG_FORCE_INLINE inline RTM_FORCE_NOINLINE
#else
	#define ACL_IMPL_DEBUG_FORCE_INLINE RTM_FORCE_INLINE
#endif

//////////////////////////////////////////////////////////////////////////
// Allows us to specify branch hints
//////////////////////////////////////////////////////////////////////////
#if RTM_CPP_VERSION >= RTM_CPP_VERSION_20
	#define ACL_BRANCH_LIKELY [[likely]]
	#define ACL_BRANCH_UNLIKELY [[unlikely]]
#elif defined(RTM_COMPILER_CLANG) && ACL_HAS_ATTRIBUTE(likely) && ACL_HAS_ATTRIBUTE(unlikely)
	// Clang supported the same syntax as C++20 much earlier
	#define ACL_BRANCH_LIKELY [[likely]]
	#define ACL_BRANCH_UNLIKELY [[unlikely]]
#else
	#define ACL_BRANCH_LIKELY
	#define ACL_BRANCH_UNLIKELY
#endif

// When enabled, constant sub-tracks will use the weighted average of every sample instead of the first sample
// Disabled by default, most clips have no measurable gain but some clips suffer greatly, needs to be investigated, possibly a bug somewhere
// Note: Code has been removed in the pull request that closes: https://github.com/nfrechette/acl/issues/353
//#define ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS
