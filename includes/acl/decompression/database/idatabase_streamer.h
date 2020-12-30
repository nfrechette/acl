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
#include <functional>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// The interface for database streamers.
	//
	// Streamers are responsible for allocating/freeing the bulk data as well as
	// streaming the data in/out. Streaming in is safe from any thread but streaming out
	// cannot happen while decompression is in progress otherwise the behavior is undefined.
	////////////////////////////////////////////////////////////////////////////////
	class idatabase_streamer
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// Streamer destructor
		virtual ~idatabase_streamer() {}

		//////////////////////////////////////////////////////////////////////////
		// Returns true if the streamer is initialized.
		virtual bool is_initialized() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns a valid pointer to the bulk data used to decompress from.
		// Note that the pointer will not be used until after the first successful stream in
		// request is completed. As such, it is safe to allocate the bulk data when the first
		// stream in request happens.
		virtual const uint8_t* get_bulk_data() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Called when we request some data to be streamed in.
		// Only one stream in/out request can be in flight at a time.
		// Streaming in animation data can be done while animations are decompressing.
		//
		// The offset into the bulk data and the size in bytes to stream in are provided as arguments.
		// On the first stream in request, the bulk data can be allocated but it cannot change with subsequent
		// stream in requests until everything has been streamed out.
		// Once the streaming request has been fulfilled (sync or async), call the continuation function with
		// the status result.
		// The continuation can be safely called from any thread.
		virtual void stream_in(uint32_t offset, uint32_t size, bool can_allocate_bulk_data, const std::function<void(bool success)>& continuation) = 0;

		//////////////////////////////////////////////////////////////////////////
		// Called when we request some data to be streamed out.
		// Only one stream in/out request can be in flight at a time.
		// Streaming out animation data cannot be done while animations are decompressing.
		// Doing so will result in undefined behavior as the data could be in use while we stream it out.
		//
		// The offset into the bulk data and the size in bytes to stream out are provided as arguments.
		// On the last stream out request, the bulk data can be deallocated. It will be allocated again
		// if the data streams back in.
		// Once the streaming request has been fulfilled (sync or async), call the continuation function.
		// The continuation can be safely called from any thread.
		virtual void stream_out(uint32_t offset, uint32_t size, bool can_deallocate_bulk_data, const std::function<void()>& continuation) = 0;
	};
}

ACL_IMPL_FILE_PRAGMA_POP
