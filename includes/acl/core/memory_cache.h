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

#include "acl/core/iallocator.h"
#include "acl/math/vector4_32.h"

#include <cstdint>

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Simple helper to flush the CPU cache
	//////////////////////////////////////////////////////////////////////////
	class CPUCacheFlusher
	{
	public:
		CPUCacheFlusher() {}

		//////////////////////////////////////////////////////////////////////////
		// Flush the buffer data from the CPU cache
		void flush_cache(const void* buffer, size_t buffer_size)
		{
#if defined(ACL_SSE2_INTRINSICS)
			constexpr size_t k_cache_line_size = 64;

			const uint8_t* buffer_start = reinterpret_cast<const uint8_t*>(buffer);
			const uint8_t* buffer_ptr = buffer_start;
			const uint8_t* buffer_end = buffer_start + buffer_size;

			while (buffer_ptr < buffer_end)
			{
				_mm_clflush(buffer_ptr);
				buffer_ptr += k_cache_line_size;
			}
#else
			(void)buffer;
			(void)buffer_size;

			for (size_t entry_index = 0; entry_index < k_num_buffer_entries; ++entry_index)
				m_buffer[entry_index] = vector_add(m_buffer[entry_index], vector_set(1.0f));
#endif
		}

	private:
#if !defined(ACL_SSE2_INTRINSICS)
		// TODO: get an official CPU cache size
	#if defined(__ANDROID__)
		// Nexus 5X has 2MB cache
		static constexpr size_t k_cache_size = 3 * 1024 * 1024;
	#else
		// iPad Pro has 8MB cache
		static constexpr size_t k_cache_size = 9 * 1024 * 1024;
	#endif

		static constexpr size_t k_num_buffer_entries = k_cache_size / sizeof(Vector4_32);

		Vector4_32		m_buffer[k_num_buffer_entries];
#endif
	};
}
