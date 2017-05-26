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

#include "acl/assert.h"

#include <malloc.h>
#include <stdint.h>

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

	//////////////////////////////////////////////////////////////////////////

	template<typename PtrType>
	constexpr bool is_aligned_to(PtrType* value, size_t alignment)
	{
		return (reinterpret_cast<uintptr_t>(value) & (alignment - 1)) == 0;
	}

	template<typename OutputPtrType, typename InputPtrType, typename OffsetType>
	constexpr OutputPtrType add_offset_to_ptr(InputPtrType ptr, OffsetType offset)
	{
		return reinterpret_cast<OutputPtrType>(reinterpret_cast<uintptr_t>(ptr) + offset);
	}

	//////////////////////////////////////////////////////////////////////////

	template<typename DataType, typename OffsetType>
	class PtrOffset
	{
	public:
		PtrOffset() : m_value(0) {}
		PtrOffset(size_t value)
			: m_value(static_cast<OffsetType>(value))
		{
			ensure(value == m_value);
		}

		template<typename BaseType>
		DataType* get(BaseType ptr) { return add_offset_to_ptr<DataType*>(ptr, m_value); }

		template<typename BaseType>
		const DataType* get(const BaseType ptr) const { return add_offset_to_ptr<const DataType*>(ptr, m_value); }

		operator OffsetType() const { return m_value; }

	private:
		OffsetType m_value;
	};

	template<typename DataType>
	using PtrOffset16 = PtrOffset<DataType, uint16_t>;

	template<typename DataType>
	using PtrOffset32 = PtrOffset<DataType, uint32_t>;
}
