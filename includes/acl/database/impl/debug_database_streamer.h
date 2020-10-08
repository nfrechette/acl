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

#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/database/idatabase_streamer.h"

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// Implements a debug streamer where we duplicate the bulk data in memory and use
	// memcpy to stream in the data. Streamed out data is explicitly set to 0xCD with memset.
	////////////////////////////////////////////////////////////////////////////////
	class debug_database_streamer final : public idatabase_streamer
	{
	public:
		debug_database_streamer(iallocator& allocator, const uint8_t* bulk_data, uint32_t bulk_data_size)
			: m_allocator(allocator)
			, m_src_bulk_data(bulk_data)
			, m_streamed_bulk_data(allocate_type_array<uint8_t>(allocator, bulk_data_size))
			, m_bulk_data_size(bulk_data_size)
		{
			std::memset(m_streamed_bulk_data, 0xCD, bulk_data_size);
		}

		virtual ~debug_database_streamer()
		{
			deallocate_type_array(m_allocator, m_streamed_bulk_data, m_bulk_data_size);
		}

		virtual bool is_initialized() const override { return m_src_bulk_data != nullptr; }

		virtual const uint8_t* get_bulk_data() const override { return m_streamed_bulk_data; }

		virtual void stream_in(uint32_t offset, uint32_t size, const std::function<void(bool success)>& continuation) override
		{
			std::memcpy(m_streamed_bulk_data + offset, m_src_bulk_data + offset, size);
			continuation(true);
		}

		virtual void stream_out(uint32_t offset, uint32_t size, const std::function<void(bool success)>& continuation) override
		{
			std::memset(m_streamed_bulk_data + offset, 0xCD, size);
			continuation(true);
		}

	private:
		debug_database_streamer(const debug_database_streamer&) = delete;
		debug_database_streamer& operator=(const debug_database_streamer&) = delete;

		iallocator& m_allocator;
		const uint8_t* m_src_bulk_data;
		uint8_t* m_streamed_bulk_data;
		uint32_t m_bulk_data_size;
	};
}

ACL_IMPL_FILE_PRAGMA_POP
