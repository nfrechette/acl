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

#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/core/compiler_utils.h"

#include <type_traits>
#include <utility>
#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// A simple memory allocator interface.
	//
	// In order to integrate this library into your own code base, you will
	// need to provide some functions with an allocator instance that derives
	// from this interface.
	//
	// See ansi_allocator.h for an implementation that uses the system malloc/free.
	////////////////////////////////////////////////////////////////////////////////
	class IAllocator
	{
	public:
		static constexpr size_t k_default_alignment = 16;

		IAllocator() {}
		virtual ~IAllocator() {}

		IAllocator(const IAllocator&) = delete;
		IAllocator& operator=(const IAllocator&) = delete;

		////////////////////////////////////////////////////////////////////////////////
		// Allocates memory with the specified size and alignment.
		//
		// size: Size in bytes to allocate.
		// alignment: Alignment to allocate the memory with.
		virtual void* allocate(size_t size, size_t alignment = k_default_alignment) = 0;

		////////////////////////////////////////////////////////////////////////////////
		// Deallocates previously allocated memory and releases it.
		//
		// ptr: A pointer to memory previously allocated or nullptr.
		// size: Size in bytes of the allocation. This will match the original size requested through 'allocate'.
		virtual void deallocate(void* ptr, size_t size) = 0;
	};

	//////////////////////////////////////////////////////////////////////////

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type(IAllocator& allocator, Args&&... args)
	{
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType), alignof(AllocatedType)));
		if (acl_impl::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		return new(ptr) AllocatedType(std::forward<Args>(args)...);
	}

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type_aligned(IAllocator& allocator, size_t alignment, Args&&... args)
	{
		ACL_ASSERT(is_alignment_valid<AllocatedType>(alignment), "Invalid alignment: %u. Expected a power of two at least equal to %u", alignment, alignof(AllocatedType));
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType), alignment));
		if (acl_impl::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		return new(ptr) AllocatedType(std::forward<Args>(args)...);
	}

	template<typename AllocatedType>
	void deallocate_type(IAllocator& allocator, AllocatedType* ptr)
	{
		if (ptr == nullptr)
			return;

		if (!std::is_trivially_destructible<AllocatedType>::value)
			ptr->~AllocatedType();

		allocator.deallocate(ptr, sizeof(AllocatedType));
	}

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type_array(IAllocator& allocator, size_t num_elements, Args&&... args)
	{
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType) * num_elements, alignof(AllocatedType)));
		if (acl_impl::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		for (size_t element_index = 0; element_index < num_elements; ++element_index)
			new(&ptr[element_index]) AllocatedType(std::forward<Args>(args)...);
		return ptr;
	}

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type_array_aligned(IAllocator& allocator, size_t num_elements, size_t alignment, Args&&... args)
	{
		ACL_ASSERT(is_alignment_valid<AllocatedType>(alignment), "Invalid alignment: %u. Expected a power of two at least equal to %u", alignment, alignof(AllocatedType));
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType) * num_elements, alignment));
		if (acl_impl::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		for (size_t element_index = 0; element_index < num_elements; ++element_index)
			new(&ptr[element_index]) AllocatedType(std::forward<Args>(args)...);
		return ptr;
	}

	template<typename AllocatedType>
	void deallocate_type_array(IAllocator& allocator, AllocatedType* elements, size_t num_elements)
	{
		if (elements == nullptr)
			return;

		if (!std::is_trivially_destructible<AllocatedType>::value)
		{
			for (size_t element_index = 0; element_index < num_elements; ++element_index)
				elements[element_index].~AllocatedType();
		}

		allocator.deallocate(elements, sizeof(AllocatedType) * num_elements);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
