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

// This file gathers all defines that can be used to control ACL's behavior
// along with context.

//////////////////////////////////////////////////////////////////////////
// This library uses a simple system to handle asserts. Asserts are fatal and must terminate
// otherwise the behavior is undefined if execution continues.
//
// A total of 4 behaviors are supported:
//    - We can print to stderr and abort
//    - We can throw and exception
//    - We can call a custom function
//    - Do nothing and strip the check at compile time (default behavior)
//
// Aborting:
//    In order to enable the aborting behavior, simply define the macro ACL_ON_ASSERT_ABORT:
//    #define ACL_ON_ASSERT_ABORT
//
// Throwing:
//    In order to enable the throwing behavior, simply define the macro ACL_ON_ASSERT_THROW:
//    #define ACL_ON_ASSERT_THROW
//    Note that the type of the exception thrown is acl::runtime_assert.
//
// Custom function:
//    In order to enable the custom function calling behavior, define the macro ACL_ON_ASSERT_CUSTOM
//    with the name of the function to call:
//    #define ACL_ON_ASSERT_CUSTOM on_custom_assert_impl
//    Note that the function signature is as follow:
//    void on_custom_assert_impl(const char* expression, int line, const char* file, const char* format, ...) {}
//
//    You can also define your own assert implementation by defining the ACL_ASSERT macro as well:
//    #define ACL_ON_ASSERT_CUSTOM
//    #define ACL_ASSERT(expression, format, ...) checkf(expression, ANSI_TO_TCHAR(format), #__VA_ARGS__)
//
//    [Custom String Format Specifier]
//    Note that if you use a custom function, you may need to override the ACL_ASSERT_STRING_FORMAT_SPECIFIER
//    to properly handle ANSI/Unicode support. The C++11 standard does not support a way to say that '%s'
//    always means an ANSI string (with 'const char*' as type). MSVC does support '%hs' but other compilers
//    do not.
//
// No checks:
//    By default if no macro mentioned above is defined, all asserts will be stripped
//    at compile time.
//////////////////////////////////////////////////////////////////////////

// You can uncomment one of these or specify it on the compilation command line
//#define ACL_ON_ASSERT_ABORT
//#define ACL_ON_ASSERT_THROW
//#define ACL_ON_ASSERT_CUSTOM(expression, line, file, format, ...) (void)expression

// See [Custom String Format Specifier] for details
#if !defined(ACL_ASSERT_STRING_FORMAT_SPECIFIER)
	#define ACL_ASSERT_STRING_FORMAT_SPECIFIER "%s"
#endif

// You can disable deprecation warnings by defining ACL_NO_DEPRECATION
//#define ACL_NO_DEPRECATION

// x86 popcount/leading zero count intrinsic support can be enabled by defining ACL_USE_POPCOUNT
// Note that this is a hardware feature and if the CPU does not support it, the
// behavior is undefined. Make sure your platform supports it before enabling this.
// XboxOne and PlayStation 4 are both based on AMD Jaguar which supports this. New generations support it as well.
// popcnt/lzcnt are a minimum requirement for Windows 11.
//#define ACL_USE_POPCOUNT

// x86 BMI intrinsic support can be enabled by defining ACL_BMI_INTRINSICS
// Note that this is a hardware feature and if the CPU does not support it, the
// behavior is undefined. Make sure your platform supports it before enabling this.
// XboxOne and PlayStation 4 are both based on AMD Jaguar which supports this. New generations support it as well.
// BMI is a minimum requirement for Windows 11.
//#define ACL_BMI_INTRINSICS

// Disables usage of 8-wide AVX when AVX is enabled
// This provides a small performance boost with modern AVX hardware but
// with older hardware that generates 2x uOps per 8-wide instruction, it
// can often yield a small performance loss (e.g. AMD Jaguar)
// 8-wide AVX is enabled by default when AVX is enabled
//#define ACL_NO_8_WIDE_AVX

// If you use C++20 or greater, you can use older C++11 std::memory_order entries by defining ACL_USE_CPP11_STD_MEMORY_ORDER.
// This is sometimes necessary if you compile with a modern compiler version but use an older stdlib
// that does not contain the necessary enum entries
//#define ACL_USE_CPP11_STD_MEMORY_ORDER

////////////////////////////////////////////////////////////////////////////////
// Defines below are for development debugging/profiling purposes
////////////////////////////////////////////////////////////////////////////////

// When using the acl::ansi_allocator
// You can disable all allocator sanitizing by defining ACL_NO_ALLOCATOR_SANITIZING
//#define ACL_NO_ALLOCATOR_SANITIZING

// Enable this to prevent bind pose stripping
// This will set each default track value to the identity
//#define ACL_IMPL_DISABLE_BIND_POSE_STRIPPING

// Enable this to disable inlining
// This is handy to debug the code and to more easily view
// the generated assembly for each piece
//#define ACL_IMPL_ENABLE_DEBUG_FORCE_INLINE

////////////////////////////////////////////////////////////////////////////////
// Macro feature validation and auto-detection is implemented in its own header
#include "acl/core/impl/config.impl.h"
