#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2025 Nicholas Frechette & Animation Compression Library contributors
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

// Included only once from config.h

#include <rtm/impl/detect_arch.h>

// popcnt/lzcnt intrinsic support
#if !defined(ACL_USE_POPCOUNT) && !defined(RTM_NO_INTRINSICS)
	// TODO: Enable this for other publicly available console defines as well
	#if defined(_DURANGO) || defined(_XBOX_ONE)
		// Enable pop-count type instructions on Xbox One
		#define ACL_USE_POPCOUNT
	#endif
#endif

#if defined(ACL_USE_POPCOUNT)
	#if !(defined(RTM_ARCH_X86) || defined(RTM_ARCH_X64))
		static_assert(false, "ACL_USE_POPCOUNT can only be used on x86/x64 architectures");
	#endif
#endif

// BMI intrinsic support
#if !defined(ACL_BMI_INTRINSICS) && !defined(RTM_NO_INTRINSICS)
	// TODO: Enable this for other publicly available console defines as well
	#if defined(_DURANGO) || defined(_XBOX_ONE)
		// Enable BMI type instructions on Xbox One
		#define ACL_BMI_INTRINSICS
	#elif defined(__BMI__)
		// Clang and GCC define __BMI__ when -mbmi is used
		#define ACL_BMI_INTRINSICS
	#elif defined(RTM_AVX_INTRINSICS) && defined(RTM_COMPILER_MSVC)
		// Enable BMI when AVX is enabled except with clang under Windows
		// Note: It seems that the Clang toolchain with MSVC enables BMI only with AVX2 unlike
		// MSVC which enables it with AVX
		#define ACL_BMI_INTRINSICS
	#endif
#endif

#if defined(ACL_BMI_INTRINSICS)
	#if !(defined(RTM_ARCH_X86) || defined(RTM_ARCH_X64))
		static_assert(false, "ACL_BMI_INTRINSICS can only be used on x86/x64 architectures");
	#endif
#endif
