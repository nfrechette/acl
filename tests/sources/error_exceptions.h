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

#include <cstdio>
#include <cstdarg>
#include <stdexcept>

#if !defined(ACL_NO_ERROR_CHECKS)
	// Always enabled by default
	#define ACL_USE_ERROR_CHECKS
#endif

namespace acl
{
	namespace error_impl
	{
		class AssertFailed : public std::exception
		{
		public:
			template<typename ... Args>
			AssertFailed(const char* format, Args&& ... args)
			{
				snprintf(m_reason, sizeof(m_reason), format, std::forward<Args>(args) ...);
			}

			virtual const char* what() const noexcept override { return m_reason; }

		private:
			char m_reason[1024];
		};

		class EnsureFailed : public AssertFailed
		{
		public:
			template<typename ... Args>
			EnsureFailed(const char* format, Args&& ... args)
				: AssertFailed(format, std::forward<Args>(args) ...)
			{
			}
		};

		template<typename Exception, typename ... Args>
		inline void assert_impl(bool expression, const char* format, Args&& ... args)
		{
			if (!expression)
				throw Exception(format, std::forward<Args>(args) ...);
		}
	}
}

#if defined(ACL_USE_ERROR_CHECKS)
	#define ACL_ASSERT(expression, format, ...) acl::error_impl::assert_impl<acl::error_impl::AssertFailed>(expression, format, ## __VA_ARGS__)
#else
	#define ACL_ASSERT(expression, format, ...) ((void)0)
#endif

#if defined(ACL_USE_ERROR_CHECKS)
	#define ACL_ENSURE(expression, format, ...) acl::error_impl::assert_impl<acl::error_impl::EnsureFailed>(expression, format, ## __VA_ARGS__)
#else
	#define ACL_ENSURE(expression, format, ...) ((void)0)
#endif

#if defined(ACL_USE_ERROR_CHECKS)
	#define ACL_TRY_ASSERT(expression, format, ...) acl::error_impl::assert_impl<acl::error_impl::AssertFailed>(expression, format, ## __VA_ARGS__), !(expression)
#else
	#define ACL_TRY_ASSERT(expression, format, ...) !(expression)
#endif
