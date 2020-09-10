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

#include "acl/core/buffer_tag.h"
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/impl/compressed_headers.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// An instance of a compressed database.
	// The compressed data immediately follows this instance in memory.
	// The total size of the buffer can be queried with `get_size()`.
	// A compressed database can either contain all the data inline within its buffer
	// in one blob or it can be split into smaller chunks that can be streamed in and out.
	////////////////////////////////////////////////////////////////////////////////
	class alignas(16) compressed_database final
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of the compressed database.
		// Includes the 'compressed_database' instance size and the size of all inline chunks
		// but the streamable chunks.
		uint32_t get_size() const { return m_buffer_header.size; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the total size in bytes of the compressed database.
		// Includes 'get_size()' and all streamable chunks.
		uint32_t get_total_size() const;

		//////////////////////////////////////////////////////////////////////////
		// Returns the hash for the compressed database.
		// This is only used for sanity checking in case of memory corruption.
		uint32_t get_hash() const { return m_buffer_header.hash; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the binary tag for the compressed database.
		// This uniquely identifies the buffer as a proper 'compressed_database' object.
		buffer_tag32 get_tag() const;

		////////////////////////////////////////////////////////////////////////////////
		// Returns true if the compressed database is valid and usable.
		// This mainly validates some invariants as well as ensuring that the
		// memory has not been corrupted.
		//
		// check_hash: If true, the compressed tracks hash will also be compared.
		error_result is_valid(bool check_hash) const;

	private:
		////////////////////////////////////////////////////////////////////////////////
		// Hide everything
		compressed_database() = delete;
		compressed_database(const compressed_database&) = delete;
		compressed_database(compressed_database&&) = delete;
		compressed_database* operator=(const compressed_database&) = delete;
		compressed_database* operator=(compressed_database&&) = delete;

		////////////////////////////////////////////////////////////////////////////////
		// Raw buffer header that isn't included in the hash.
		////////////////////////////////////////////////////////////////////////////////

		acl_impl::raw_buffer_header		m_buffer_header;

		////////////////////////////////////////////////////////////////////////////////
		// Everything starting here is included in the hash.
		////////////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// Compressed data follows here in memory.
		//////////////////////////////////////////////////////////////////////////

		// Here we define some unspecified padding but the 'database_header' starts here.
		// This is done to ensure that this class is 16 byte aligned without requiring further padding
		// if the 'database_header' ends up causing us to be unaligned.
		uint32_t m_padding[2];
	};

	//////////////////////////////////////////////////////////////////////////
	// Create a compressed_database instance in place from a raw memory buffer.
	// If the buffer does not contain a valid compressed_database instance, nullptr is returned
	// along with an optional error result.
	//////////////////////////////////////////////////////////////////////////
	const compressed_database* make_compressed_database(const void* buffer, error_result* out_error_result = nullptr);
}

#include "acl/core/impl/compressed_database.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
