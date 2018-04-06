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

#include <stdexcept>

#if !defined(SJSON_CPP_NO_ERROR_CHECKS)
	// Always enabled by default
	#define SJSON_CPP_USE_ERROR_CHECKS
#endif

namespace sjson
{
	namespace error_impl
	{
		class AssertFailed : public std::exception {};
		class EnsureFailed : public AssertFailed {};

		template<typename Exception>
		inline void assert_impl(bool expression) { if (!expression) throw Exception(); }
	}
}

#if defined(SJSON_CPP_USE_ERROR_CHECKS)
	#define SJSON_CPP_ASSERT(expression, format, ...) sjson::error_impl::assert_impl<sjson::error_impl::AssertFailed>(expression)
#else
	#define SJSON_CPP_ASSERT(expression, format, ...) ((void)0)
#endif

#if defined(SJSON_CPP_USE_ERROR_CHECKS)
	#define SJSON_CPP_ENSURE(expression, format, ...) sjson::error_impl::assert_impl<sjson::error_impl::EnsureFailed>(expression)
#else
	#define SJSON_CPP_ENSURE(expression, format, ...) ((void)0)
#endif

#if defined(SJSON_CPP_USE_ERROR_CHECKS)
	#define SJSON_CPP_TRY_ASSERT(expression, format, ...) sjson::error_impl::assert_impl<sjson::error_impl::AssertFailed>(expression), !(expression)
#else
	#define SJSON_CPP_TRY_ASSERT(expression, format, ...) !(expression)
#endif
