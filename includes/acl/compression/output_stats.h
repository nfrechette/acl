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

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/enum_utils.h"

namespace sjson { class ObjectWriter; }

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	enum class stat_logging
	{
		none						= 0x0000,
		summary						= 0x0001,
		detailed					= 0x0002 | summary,
		exhaustive					= 0x0004 | detailed,
		summary_decompression		= 0x0010,
		exhaustive_decompression	= 0x0020,
	};

	ACL_IMPL_ENUM_FLAGS_OPERATORS(stat_logging)

	struct output_stats
	{
		stat_logging			logging = stat_logging::none;
		sjson::ObjectWriter*	writer = nullptr;
	};
}

ACL_IMPL_FILE_PRAGMA_POP
