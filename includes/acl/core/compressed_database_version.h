#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Enum for versions used by 'compressed_database'.
	// These values are used by serialization, do not change them once assigned.
	// To add a new version, create a new entry and follow the naming and numbering scheme
	// described below.
	enum class compressed_database_version16 : uint16_t
	{
		//////////////////////////////////////////////////////////////////////////
		// Special version identifier used when decompressing.
		// This indicates that any version is supported by decompression and
		// the code isn't optimized for any one version in particular.
		// It is not a valid value for compressed databases.
		any = 0,

		//////////////////////////////////////////////////////////////////////////
		// Actual versions in sequential order.
		// Keep the enum values sequential.
		// Enum value name should be of the form: major, minor, patch version.
		// Two digits are reserved for safety and future proofing.

		v02_00_00 = 1,			// ACL v2.0.0

		//////////////////////////////////////////////////////////////////////////
		// First version marker, this is equal to the first version supported: ACL 2.0.0
		// Versions prior to ACL 2.0 are not backwards compatible.
		// It is used for range checks.
		first = v02_00_00,

		//////////////////////////////////////////////////////////////////////////
		// Always assigned to the latest version supported.
		latest = v02_00_00,
	};
}

ACL_IMPL_FILE_PRAGMA_POP
