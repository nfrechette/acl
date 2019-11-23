#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include <rtm/types.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// We use a struct like this to allow an arbitrary format on the end user side.
	// Since our decode function is templated on this type implemented by the user,
	// the callbacks can trivially be inlined and customized.
	// Only called functions need to be overridden and implemented.
	//////////////////////////////////////////////////////////////////////////
	struct track_writer
	{
		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void write_float1(uint32_t track_index, rtm::scalarf_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void write_float2(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void write_float3(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void write_float4(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void write_vector4(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}
	};
}

ACL_IMPL_FILE_PRAGMA_POP
