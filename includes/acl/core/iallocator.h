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

/// \file
/// The allocator interface, and the functions that allocate and free typed memory with it.

#include "acl/version.h"
#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/core/impl/bit_cast.impl.h"
#include "acl/core/impl/compiler_utils.h"

#include <type_traits>
#include <utility>
#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	/// A simple memory allocator interface.
	///
	/// In order to integrate this library into your own code base, you will
	/// need to provide some functions with an allocator instance that derives
	/// from this interface.
	///
	/// See ansi_allocator.h for an implementation that uses the system malloc/free.
	class iallocator
	{
	public:
		static constexpr size_t k_default_alignment = 16;	///< The alignment in bytes of an allocation when the caller does not give one

		iallocator() noexcept = default;
		virtual ~iallocator() = default;

		/// Allocates memory with the specified size and alignment.
		///
		/// \param size Size in bytes to allocate.
		/// \param alignment Alignment to allocate the memory with.
		ACL_NO_DISCARD virtual void* allocate(size_t size, size_t alignment = k_default_alignment) = 0;

		/// Deallocates previously allocated memory and releases it.
		///
		/// \param ptr A pointer to memory previously allocated or nullptr.
		/// \param size Size in bytes of the allocation. This will match the original size requested through `allocate`.
		virtual void deallocate(void* ptr, size_t size) = 0;
	};

	/// Allocates an instance of a type with the allocator, and constructs it with the given arguments.
	/// A type that is trivially default constructible is not constructed.
	template<typename allocated_type, typename... args>
	ACL_NO_DISCARD allocated_type* allocate_type(iallocator& allocator, args&&... arguments)
	{
		allocated_type* ptr = acl_impl::bit_cast<allocated_type*>(allocator.allocate(sizeof(allocated_type), alignof(allocated_type)));
		if (acl_impl::is_trivially_default_constructible<allocated_type>::value)
			return ptr;
		return new(ptr) allocated_type(std::forward<args>(arguments)...);
	}

	/// Allocates an instance of a type with the given alignment, and constructs it with the given arguments.
	/// The alignment must be a power of two and at least the alignment of the type.
	template<typename allocated_type, typename... args>
	ACL_NO_DISCARD allocated_type* allocate_type_aligned(iallocator& allocator, size_t alignment, args&&... arguments)
	{
		ACL_ASSERT(is_alignment_valid<allocated_type>(alignment), "Invalid alignment: %u. Expected a power of two at least equal to %u", alignment, alignof(allocated_type));
		allocated_type* ptr = acl_impl::bit_cast<allocated_type*>(allocator.allocate(sizeof(allocated_type), alignment));
		if (acl_impl::is_trivially_default_constructible<allocated_type>::value)
			return ptr;
		return new(ptr) allocated_type(std::forward<args>(arguments)...);
	}

	/// Destroys an instance that allocate_type allocated, and releases its memory. A null pointer does nothing.
	template<typename allocated_type>
	void deallocate_type(iallocator& allocator, allocated_type* ptr)
	{
		if (ptr == nullptr)
			return;

		if (!std::is_trivially_destructible<allocated_type>::value)
			ptr->~allocated_type();

		allocator.deallocate(ptr, sizeof(allocated_type));
	}

	/// Allocates an array of instances of a type, and constructs each instance with the given arguments.
	/// A type that is trivially default constructible is not constructed.
	template<typename allocated_type, typename... args>
	ACL_NO_DISCARD allocated_type* allocate_type_array(iallocator& allocator, size_t num_elements, args&&... arguments)
	{
		allocated_type* ptr = acl_impl::bit_cast<allocated_type*>(allocator.allocate(sizeof(allocated_type) * num_elements, alignof(allocated_type)));
		if (acl_impl::is_trivially_default_constructible<allocated_type>::value)
			return ptr;
		for (size_t element_index = 0; element_index < num_elements; ++element_index)
			new(&ptr[element_index]) allocated_type(std::forward<args>(arguments)...);
		return ptr;
	}

	/// Allocates an array of instances of a type with the given alignment, and constructs each instance with the given arguments.
	/// The alignment must be a power of two and at least the alignment of the type.
	template<typename allocated_type, typename... args>
	ACL_NO_DISCARD allocated_type* allocate_type_array_aligned(iallocator& allocator, size_t num_elements, size_t alignment, args&&... arguments)
	{
		ACL_ASSERT(is_alignment_valid<allocated_type>(alignment), "Invalid alignment: %zu. Expected a power of two at least equal to %zu", alignment, alignof(allocated_type));
		allocated_type* ptr = acl_impl::bit_cast<allocated_type*>(allocator.allocate(sizeof(allocated_type) * num_elements, alignment));
		if (acl_impl::is_trivially_default_constructible<allocated_type>::value)
			return ptr;
		for (size_t element_index = 0; element_index < num_elements; ++element_index)
			new(&ptr[element_index]) allocated_type(std::forward<args>(arguments)...);
		return ptr;
	}

	/// Destroys the instances of an array that allocate_type_array allocated, and releases its memory.
	/// The number of elements must be the same as the allocation. A null pointer does nothing.
	template<typename allocated_type>
	void deallocate_type_array(iallocator& allocator, allocated_type* elements, size_t num_elements)
	{
		if (elements == nullptr)
			return;

		if (!std::is_trivially_destructible<allocated_type>::value)
		{
			for (size_t element_index = 0; element_index < num_elements; ++element_index)
				elements[element_index].~allocated_type();
		}

		allocator.deallocate(elements, sizeof(allocated_type) * num_elements);
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
