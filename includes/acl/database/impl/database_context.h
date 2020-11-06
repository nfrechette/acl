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
	class idatabase_streamer;

	namespace acl_impl
	{
		struct alignas(64) database_context_v0
		{
			//												//   offsets
			// Only member used to detect if we are initialized, must be first
			const compressed_database* db;					//   0 |   0
			iallocator* allocator;							//   4 |   8
			
			const uint8_t* bulk_data;						//   8 |  16
			idatabase_streamer* streamer;					//  12 |  24

			uint32_t* loaded_chunks;						//  16 |  32
			uint32_t* streaming_chunks;						//  20 |  40

			uint8_t* clip_segment_headers;					//  24 |  48

			uint8_t padding1[sizeof(void*) == 4 ? 36 : 8];	//  28 |  56

			//									Total size:	    64 |  64

			//////////////////////////////////////////////////////////////////////////

			const compressed_database* get_compressed_database() const { return db; }
			compressed_tracks_version16 get_version() const { return db->get_version(); }
			bool is_initialized() const { return db != nullptr; }
			void reset() { db = nullptr; }
		};

		static_assert(sizeof(database_context_v0) == 64, "Unexpected size");
	}
}

ACL_IMPL_FILE_PRAGMA_POP
