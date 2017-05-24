#pragma once

#include "acl/assert.h"

#include <malloc.h>

namespace acl
{
	class Allocator
	{
	public:
		static constexpr size_t DEFAULT_ALIGNMENT = 16;

		Allocator() {}
		virtual ~Allocator() {}

		Allocator(const Allocator&) = delete;
		Allocator& operator=(const Allocator&) = delete;

		virtual void* allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT)
		{
			return _aligned_malloc(size, alignment);
		}

		virtual void deallocate(void* ptr)
		{
			if (ptr == nullptr)
			{
				return;
			}

			_aligned_free(ptr);
		}
	};

	template<typename AllocatedType>
	AllocatedType* allocate_type(Allocator& allocator)
	{
		return reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType), alignof(AllocatedType)));
	}

	template<typename AllocatedType>
	AllocatedType* allocate_type_array(Allocator& allocator, size_t num_elements)
	{
		return reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType) * num_elements, alignof(AllocatedType)));
	}

	template<typename AllocatedType>
	AllocatedType* allocate_type_array(Allocator& allocator, size_t num_elements, size_t alignment)
	{
		ensure(alignment >= alignof(AllocatedType));
		return reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType) * num_elements, alignment));
	}

	template<typename PtrType>
	constexpr bool is_aligned_to(PtrType* value, size_t alignment)
	{
		return (reinterpret_cast<uintptr_t>(value) & (alignment - 1)) == 0;
	}
}
