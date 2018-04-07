#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/error.h"

#if defined(__APPLE__)
#include <cstdlib>	// For posix_memalign
#elif defined(_WIN32)
#include <malloc.h>
#endif

#if defined(ACL_HAS_ASSERT_CHECKS) && !defined(ACL_NO_ALLOCATOR_TRACKING)
#define ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS
#endif

// This is used for debugging memory leaks, double frees, etc.
// It should never be enabled in production!
//#define ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS

#if defined(ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
#include <atomic>
#endif

#if defined(ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS)
#include <unordered_map>
#endif

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// An ANSI allocator implementation. It uses the system malloc/free to manage
	// memory as well as provides some debugging functionality to track memory leaks.
	////////////////////////////////////////////////////////////////////////////////
	class ANSIAllocator : public IAllocator
	{
	public:
		ANSIAllocator()
			: IAllocator()
#if defined(ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
			, m_allocation_count(0)
#endif
#if defined(ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS)
			, m_debug_allocations()
#endif
		{}

		virtual ~ANSIAllocator()
		{
#if defined(ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS)
			if (!m_debug_allocations.empty())
			{
				for (const auto& pair : m_debug_allocations)
				{
					printf("Live allocation at the allocator destruction: 0x%p (%zu)\n", pair.second.ptr, pair.second.size);
				}
			}
#endif

#if defined(ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
			ACL_ASSERT(m_allocation_count == 0, "The number of allocations and deallocations does not match");
#endif
		}

		ANSIAllocator(const ANSIAllocator&) = delete;
		ANSIAllocator& operator=(const ANSIAllocator&) = delete;

		virtual void* allocate(size_t size, size_t alignment = k_default_alignment) override
		{
#if defined(ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
			m_allocation_count.fetch_add(1, std::memory_order_relaxed);
#endif

			void* ptr;

#if defined(_WIN32)
			ptr = _aligned_malloc(size, alignment);
#elif defined(__APPLE__)
			ptr = nullptr;
			posix_memalign(&ptr, std::max<size_t>(alignment, sizeof(void*)), size);
#elif defined(__ANDROID__)
			alignment = std::max<size_t>(std::max<size_t>(alignment, sizeof(void*)), sizeof(size_t));
			const size_t padded_size = size + alignment + sizeof(size_t);
			ptr = malloc(padded_size);
			if (ptr != nullptr)
			{
				const void* allocated_ptr = ptr;
				ptr = align_to(add_offset_to_ptr<void>(ptr, sizeof(size_t)), alignment);

				const size_t padding_size = safe_static_cast<size_t>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(allocated_ptr));
				size_t* padding_size_ptr = add_offset_to_ptr<size_t>(ptr, -sizeof(size_t));
				*padding_size_ptr = padding_size;
			}
#else
			ptr = aligned_alloc(alignment, size);
#endif

#if defined(ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS)
			m_debug_allocations.insert({ {ptr, AllocationEntry{ptr, size}} });
#endif

			return ptr;
		}

		virtual void deallocate(void* ptr, size_t size) override
		{
			if (ptr == nullptr)
				return;

#if defined(_WIN32)
			_aligned_free(ptr);
#elif defined(__ANDROID__)
			const size_t* padding_size_ptr = add_offset_to_ptr<size_t>(ptr, -sizeof(size_t));
			void* allocated_ptr = add_offset_to_ptr<void>(ptr, -*padding_size_ptr);
			free(allocated_ptr);
#else
			free(ptr);
#endif

#if defined(ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS)
			const auto it = m_debug_allocations.find(ptr);
			ACL_ASSERT(it != m_debug_allocations.end(), "Attempting to deallocate a pointer that isn't allocated");
			ACL_ASSERT(it->second.size == size, "Allocation and deallocation size do not match");
			m_debug_allocations.erase(ptr);
#endif

#if defined(ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
			const int32_t old_value = m_allocation_count.fetch_sub(1, std::memory_order_relaxed);
			ACL_ASSERT(old_value > 0, "The number of allocations and deallocations does not match");
#endif
		}

#if defined(ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
		int32_t get_allocation_count() const { return m_allocation_count.load(std::memory_order_relaxed); }
#endif

	private:
#if defined(ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
		std::atomic<int32_t>	m_allocation_count;
#endif

#if defined(ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS)
		struct AllocationEntry
		{
			void* ptr;
			size_t size;
		};

		std::unordered_map<void*, AllocationEntry> m_debug_allocations;
#endif
	};
}
