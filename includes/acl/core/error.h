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

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"

ACL_IMPL_FILE_PRAGMA_PUSH

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
// No checks:
//    By default if no macro mentioned above is defined, all asserts will be stripped
//    at compile time.
//////////////////////////////////////////////////////////////////////////

#if defined(ACL_ON_ASSERT_ABORT)

	#include "acl/version.h"

	#include <cstdio>
	#include <cstdarg>
	#include <cstdlib>

	namespace acl
	{
		ACL_IMPL_VERSION_NAMESPACE_BEGIN

		namespace error_impl
		{
			inline void on_assert_abort(const char* expression, int line, const char* file, const char* format, ...)
			{
				(void)expression;
				(void)line;
				(void)file;

				va_list args;
				va_start(args, format);

				std::vfprintf(stderr, format, args);
				std::fprintf(stderr, "\n");

				va_end(args);

				std::abort();
			}
		}

		ACL_IMPL_VERSION_NAMESPACE_END
	}

	#define ACL_ASSERT(expression, format, ...) do { if (!(expression)) ACL_IMPL_NAMESPACE::error_impl::on_assert_abort(#expression, __LINE__, __FILE__, (format), ## __VA_ARGS__); } while(0)
	#define ACL_HAS_ASSERT_CHECKS
	#define ACL_NO_EXCEPT noexcept

#elif defined(ACL_ON_ASSERT_THROW)

	#include "acl/version.h"

	#include <cstdio>
	#include <cstdarg>
	#include <string>
	#include <stdexcept>

	namespace acl
	{
		ACL_IMPL_VERSION_NAMESPACE_BEGIN

		class runtime_assert final : public std::runtime_error
		{
			using std::runtime_error::runtime_error;	// Inherit constructors
		};

		namespace error_impl
		{
			inline void on_assert_throw(const char* expression, int line, const char* file, const char* format, ...)
			{
				(void)expression;
				(void)line;
				(void)file;

				constexpr int buffer_size = 64 * 1024;
				char buffer[buffer_size];

				va_list args;
				va_start(args, format);

				const int count = vsnprintf(buffer, buffer_size, format, args);

				va_end(args);

				if (count >= 0 && count < buffer_size)
					throw runtime_assert(std::string(&buffer[0], count));
				else
					throw runtime_assert("Failed to format assert message!\n");
			}
		}

		ACL_IMPL_VERSION_NAMESPACE_END
	}

	#define ACL_ASSERT(expression, format, ...) do { if (!(expression)) ACL_IMPL_NAMESPACE::error_impl::on_assert_throw(#expression, __LINE__, __FILE__, (format), ## __VA_ARGS__); } while(0)
	#define ACL_HAS_ASSERT_CHECKS
	#define ACL_NO_EXCEPT

#elif defined(ACL_ON_ASSERT_CUSTOM)

	#if !defined(ACL_ASSERT)
		#define ACL_ASSERT(expression, format, ...) do { if (!(expression)) ACL_ON_ASSERT_CUSTOM(#expression, __LINE__, __FILE__, (format), ## __VA_ARGS__); } while(0)
	#endif

	#define ACL_HAS_ASSERT_CHECKS
	#define ACL_NO_EXCEPT

#else

	#define ACL_ASSERT(expression, format, ...) ((void)0)
	#define ACL_NO_EXCEPT noexcept

#endif

//////////////////////////////////////////////////////////////////////////

// Allow deprecation support
#if defined(__has_cpp_attribute) && __cplusplus >= 201402L
	#if __has_cpp_attribute(deprecated)
		#define ACL_DEPRECATED(msg) [[deprecated(msg)]]
	#endif
#endif

#if !defined(ACL_DEPRECATED)
	#if defined(__GNUC__) || defined(__clang__)
		#define ACL_DEPRECATED(msg) __attribute__((deprecated))
	#elif defined(_MSC_VER)
		#define ACL_DEPRECATED(msg) __declspec(deprecated)
	#else
		#define ACL_DEPRECATED(msg)
	#endif
#endif

ACL_IMPL_FILE_PRAGMA_POP
