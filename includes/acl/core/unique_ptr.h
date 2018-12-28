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

#include "acl/core/compiler_utils.h"
#include <acl/core/iallocator.h>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	template<typename AllocatedType>
	class Deleter
	{
	public:
		Deleter() : m_allocator(nullptr) {}
		Deleter(IAllocator& allocator) : m_allocator(&allocator) {}
		Deleter(const Deleter& deleter) : m_allocator(deleter.m_allocator) {}

		void operator()(AllocatedType* ptr)
		{
			if (ptr == nullptr)
				return;

			if (!std::is_trivially_destructible<AllocatedType>::value)
				ptr->~AllocatedType();

			m_allocator->deallocate(ptr, sizeof(AllocatedType));
		}

	private:
		IAllocator* m_allocator;
	};

	template<typename AllocatedType, typename... Args>
	std::unique_ptr<AllocatedType, Deleter<AllocatedType>> make_unique(IAllocator& allocator, Args&&... args)
	{
		return std::unique_ptr<AllocatedType, Deleter<AllocatedType>>(
			allocate_type<AllocatedType>(allocator, std::forward<Args>(args)...),
			Deleter<AllocatedType>(allocator));
	}

	template<typename AllocatedType, typename... Args>
	std::unique_ptr<AllocatedType, Deleter<AllocatedType>> make_unique_aligned(IAllocator& allocator, size_t alignment, Args&&... args)
	{
		return std::unique_ptr<AllocatedType, Deleter<AllocatedType>>(
			allocate_type_aligned<AllocatedType>(allocator, alignment, std::forward<Args>(args)...),
			Deleter<AllocatedType>(allocator));
	}
}

ACL_IMPL_FILE_PRAGMA_POP
