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
#include "acl/core/enum_utils.h"

#if defined(ACL_USE_SJSON)
#include <sjson/writer.h>
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	/// The amount of statistics that compression writes. Each level includes the statistics of the levels below it.
	enum class stat_logging
	{
		none						= 0x0000,	///< Compression writes no statistics
		summary						= 0x0001,	///< Compression writes the summary statistics of the compressed tracks
		detailed					= 0x0002 | summary,	///< Compression also writes the details of each segment and the timings
		exhaustive					= 0x0004 | detailed,	///< Compression also writes the error of each transform at each sample (the output is large)
	};

	ACL_IMPL_ENUM_FLAGS_OPERATORS(stat_logging)

	/// The statistics that compression writes, and where it writes them.
	struct output_stats
	{
		stat_logging			logging = stat_logging::none;	///< The amount of statistics to write

#if defined(ACL_USE_SJSON)
		sjson::ObjectWriter*	writer = nullptr;	///< The SJSON writer that receives the statistics, required when 'logging' is not 'none'
#endif
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
