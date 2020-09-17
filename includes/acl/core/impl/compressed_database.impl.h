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

// Included only once from compressed_database.h

namespace acl
{
	namespace acl_impl
	{
		// Hide these implementations, they shouldn't be needed in user-space
		inline const database_header& get_database_header(const compressed_database& db)
		{
			return *reinterpret_cast<const database_header*>(reinterpret_cast<const uint8_t*>(&db) + sizeof(raw_buffer_header));
		}
	}

	inline uint32_t compressed_database::get_total_size() const
	{
		return 0;	// TODO
	}

	inline buffer_tag32 compressed_database::get_tag() const { return static_cast<buffer_tag32>(acl_impl::get_database_header(*this).tag); }

	inline compressed_database_version16 compressed_database::get_version() const { return acl_impl::get_database_header(*this).version; }

	inline error_result compressed_database::is_valid(bool check_hash) const
	{
		if (!is_aligned_to(this, alignof(compressed_database)))
			return error_result("Invalid alignment");

		const acl_impl::database_header& header = acl_impl::get_database_header(*this);
		if (header.tag != static_cast<uint32_t>(buffer_tag32::compressed_database))
			return error_result("Invalid tag");

		if (header.version < compressed_database_version16::first || header.version > compressed_database_version16::latest)
			return error_result("Invalid database version");

		if (check_hash)
		{
			const uint32_t hash = hash32(safe_ptr_cast<const uint8_t>(&m_padding[0]), m_buffer_header.size - sizeof(acl_impl::raw_buffer_header));
			if (hash != m_buffer_header.hash)
				return error_result("Invalid hash");
		}

		return error_result();
	}

	inline const compressed_database* make_compressed_database(const void* buffer, error_result* out_error_result)
	{
		if (buffer == nullptr)
		{
			if (out_error_result != nullptr)
				*out_error_result = error_result("Buffer is not a valid pointer");

			return nullptr;
		}

		const compressed_database* db = static_cast<const compressed_database*>(buffer);
		if (out_error_result != nullptr)
		{
			const error_result result = db->is_valid(false);
			*out_error_result = result;

			if (result.any())
				return nullptr;
		}

		return db;
	}
}
