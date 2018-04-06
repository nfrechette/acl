#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette, Cody Jones, and sjson-cpp contributors
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

// To override these macros, simply define them before including headers from the library.

#if !defined(SJSON_CPP_NO_ERROR_CHECKS)
	// Always enabled by default
	#define SJSON_CPP_USE_ERROR_CHECKS
#endif

#if (!defined(SJSON_CPP_ASSERT) || !defined(SJSON_CPP_ENSURE)) && defined(SJSON_CPP_USE_ERROR_CHECKS)
	#include <assert.h>
	#include <cstdlib>
#endif

#include <cstdio>
#include <cstdarg>

// Asserts are properly handled by the library and can be optionally skipped by the user.
// The code found something unexpected but recovered.
#if !defined(SJSON_CPP_ASSERT)
	#if defined(SJSON_CPP_USE_ERROR_CHECKS)
		// Note: STD assert(..) is fatal, it calls abort
		//#define SJSON_CPP_ASSERT(expression, format, ...) assert(expression)
		namespace sjson
		{
			namespace error_impl
			{
				inline void assert_impl(bool expression, const char* format, ...)
				{
				#if !defined(NDEBUG)
					assert(expression);
				#endif

					if (!expression) std::abort();
				}
			}
		}

		#define SJSON_CPP_ASSERT(expression, format, ...) sjson::error_impl::assert_impl(expression, format, ## __VA_ARGS__)
	#else
		#define SJSON_CPP_ASSERT(expression, format, ...) ((void)0)
	#endif
#endif

// Ensure is fatal, the library does not handle skipping this safely.
#if !defined(SJSON_CPP_ENSURE)
	#if defined(SJSON_CPP_USE_ERROR_CHECKS)
		#define SJSON_CPP_ENSURE(expression, format, ...) sjson::error_impl::assert_impl(expression, format, ## __VA_ARGS__)
	#else
		#define SJSON_CPP_ENSURE(expression, format, ...) ((void)0)
	#endif
#endif

// Handy macro to handle asserts in if statement, usage:
// if (SJSON_CPP_TRY_ASSERT(foo != bar, "omg so bad!")) return error;
#if !defined(SJSON_CPP_TRY_ASSERT)
	#if defined(SJSON_CPP_USE_ERROR_CHECKS)
		namespace sjson
		{
			namespace error_impl
			{
				// A shim is often required because an engine assert macro might do any number of things which
				// are not compatible with an 'if' statement which breaks SJSON_CPP_TRY_ASSERT
				inline void assert_shim(bool expression, const char* format, ...)
				{
					if (!expression)
					{
						constexpr int buffer_size = 64 * 1024;
						char buffer[buffer_size];

						va_list args;
						va_start(args, format);

						int count = vsnprintf(buffer, buffer_size, format, args);
						SJSON_CPP_ENSURE(count >= 0 && count < buffer_size, "Failed to format assert");

						SJSON_CPP_ASSERT(expression, &buffer[0]);

						va_end(args);
					}
				}
			}
		}

		#define SJSON_CPP_TRY_ASSERT(expression, format, ...) sjson::error_impl::assert_shim(expression, format, ## __VA_ARGS__), !(expression)
	#else
		#define SJSON_CPP_TRY_ASSERT(expression, format, ...) !(expression)
	#endif
#endif
