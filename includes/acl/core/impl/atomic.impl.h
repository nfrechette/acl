#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2023 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/impl/compiler_utils.h"

#include <rtm/math.h>

//////////////////////////////////////////////////////////////////////////
// Include atomic header and polyfill what is missing for proper C++11 support
//////////////////////////////////////////////////////////////////////////
#include <atomic>

// See config.h for details on how to configure std::memory_order usage for your project

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// C++20 deprecated and renamed some std::memory_order members
		//////////////////////////////////////////////////////////////////////////

	#if RTM_CPP_VERSION >= RTM_CPP_VERSION_20 && !defined(ACL_USE_CPP11_STD_MEMORY_ORDER)
		constexpr std::memory_order k_memory_order_relaxed = std::memory_order::relaxed;
	#else
		constexpr std::memory_order k_memory_order_relaxed = std::memory_order::memory_order_relaxed;
	#endif
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
