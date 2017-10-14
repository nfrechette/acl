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

#include "acl/core/error.h"
#include "acl/math/math.h"

#include <malloc.h>
#include <stdint.h>
#include <type_traits>
#include <limits>
#include <memory>
#include <algorithm>

#if defined(__ANDROID__)
namespace std
{
	template<typename Type>
	using is_trivially_default_constructible = has_trivial_default_constructor<Type>;
}
#endif

namespace acl
{
	constexpr bool is_power_of_two(size_t input)
	{
		return input != 0 && (input & (input - 1)) == 0;
	}

	template<typename Type>
	constexpr bool is_alignment_valid(size_t alignment)
	{
		return is_power_of_two(alignment) && alignment >= alignof(Type);
	}

	template<typename ElementType, size_t num_elements>
	constexpr size_t get_array_size(ElementType const (&)[num_elements]) { return num_elements; }

	//////////////////////////////////////////////////////////////////////////

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

		virtual void deallocate(void* ptr, size_t size)
		{
			if (ptr == nullptr)
				return;

			_aligned_free(ptr);
		}
	};

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type(Allocator& allocator, Args&&... args)
	{
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType), alignof(AllocatedType)));
		if (std::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		return new(ptr) AllocatedType(std::forward<Args>(args)...);
	}

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type_aligned(Allocator& allocator, size_t alignment, Args&&... args)
	{
		ACL_ENSURE(is_alignment_valid<AllocatedType>(alignment), "Invalid alignment: %u. Expected a power of two at least equal to %u", alignment, alignof(AllocatedType));
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType), alignment));
		if (std::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		return new(ptr) AllocatedType(std::forward<Args>(args)...);
	}

	template<typename AllocatedType>
	void deallocate_type(Allocator& allocator, AllocatedType* ptr)
	{
		if (ptr == nullptr)
			return;

		if (!std::is_trivially_destructible<AllocatedType>::value)
			ptr->~AllocatedType();

		allocator.deallocate(ptr, sizeof(AllocatedType));
	}

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type_array(Allocator& allocator, size_t num_elements, Args&&... args)
	{
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType) * num_elements, alignof(AllocatedType)));
		if (std::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		for (size_t element_index = 0; element_index < num_elements; ++element_index)
			new(&ptr[element_index]) AllocatedType(std::forward<Args>(args)...);
		return ptr;
	}

	template<typename AllocatedType, typename... Args>
	AllocatedType* allocate_type_array_aligned(Allocator& allocator, size_t num_elements, size_t alignment, Args&&... args)
	{
		ACL_ENSURE(is_alignment_valid<AllocatedType>(alignment), "Invalid alignment: %u. Expected a power of two at least equal to %u", alignment, alignof(AllocatedType));
		AllocatedType* ptr = reinterpret_cast<AllocatedType*>(allocator.allocate(sizeof(AllocatedType) * num_elements, alignment));
		if (std::is_trivially_default_constructible<AllocatedType>::value)
			return ptr;
		for (size_t element_index = 0; element_index < num_elements; ++element_index)
			new(&ptr[element_index]) AllocatedType(std::forward<Args>(args)...);
		return ptr;
	}

	template<typename AllocatedType>
	void deallocate_type_array(Allocator& allocator, AllocatedType* elements, size_t num_elements)
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

	//////////////////////////////////////////////////////////////////////////

	template<typename AllocatedType>
	class Deleter
	{
	public:
		Deleter() : m_allocator(nullptr) {}
		Deleter(Allocator& allocator) : m_allocator(&allocator) {}
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
		Allocator* m_allocator;
	};

	template<typename AllocatedType, typename... Args>
	std::unique_ptr<AllocatedType, Deleter<AllocatedType>> make_unique(Allocator& allocator, Args&&... args)
	{
		return std::unique_ptr<AllocatedType, Deleter<AllocatedType>>(
			allocate_type<AllocatedType>(allocator, std::forward<Args>(args)...),
			Deleter<AllocatedType>(allocator));
	}

	template<typename AllocatedType, typename... Args>
	std::unique_ptr<AllocatedType, Deleter<AllocatedType>> make_unique_aligned(Allocator& allocator, size_t alignment, Args&&... args)
	{
		return std::unique_ptr<AllocatedType, Deleter<AllocatedType>>(
			allocate_type_aligned<AllocatedType>(allocator, alignment, std::forward<Args>(args)...),
			Deleter<AllocatedType>(allocator));
	}

	template<typename AllocatedType, typename... Args>
	std::unique_ptr<AllocatedType, Deleter<AllocatedType>> make_unique_array(Allocator& allocator, size_t num_elements, Args&&... args)
	{
		return std::unique_ptr<AllocatedType, Deleter<AllocatedType>>(
			allocate_type_array<AllocatedType>(allocator, num_elements, std::forward<Args>(args)...),
			Deleter<AllocatedType>(allocator));
	}

	template<typename AllocatedType, typename... Args>
	std::unique_ptr<AllocatedType, Deleter<AllocatedType>> make_unique_array_aligned(Allocator& allocator, size_t num_elements, size_t alignment, Args&&... args)
	{
		return std::unique_ptr<AllocatedType, Deleter<AllocatedType>>(
			allocate_type_array_aligned<AllocatedType>(allocator, num_elements, alignment, std::forward<Args>(args)...),
			Deleter<AllocatedType>(allocator));
	}

	//////////////////////////////////////////////////////////////////////////

	template<typename PtrType>
	constexpr bool is_aligned_to(PtrType* value, size_t alignment)
	{
		return (reinterpret_cast<uintptr_t>(value) & (alignment - 1)) == 0;
	}

	template<typename IntegralType>
	constexpr bool is_aligned_to(IntegralType value, size_t alignment)
	{
		return (static_cast<size_t>(value) & (alignment - 1)) == 0;
	}

	template<typename PtrType>
	constexpr bool is_aligned(PtrType* value)
	{
		return is_aligned_to(value, alignof(PtrType));
	}

	template<typename PtrType>
	constexpr PtrType* align_to(PtrType* value, size_t alignment)
	{
		return reinterpret_cast<PtrType*>((reinterpret_cast<uintptr_t>(value) + (alignment - 1)) & ~(alignment - 1));
	}

	template<typename IntegralType>
	constexpr IntegralType align_to(IntegralType value, size_t alignment)
	{
		return static_cast<IntegralType>((static_cast<size_t>(value) + (alignment - 1)) & ~(alignment - 1));
	}

	template<typename DestPtrType, typename SrcPtrType>
	inline DestPtrType* safe_ptr_cast(SrcPtrType* input)
	{
		ACL_ENSURE(is_aligned_to(input, alignof(DestPtrType)), "reinterpret_cast would result in an unaligned pointer");
		return reinterpret_cast<DestPtrType*>(input);
	}

	template<typename DestPtrType, typename SrcIntegralType>
	inline DestPtrType* safe_ptr_cast(SrcIntegralType input)
	{
		ACL_ENSURE(is_aligned_to(input, alignof(DestPtrType)), "reinterpret_cast would result in an unaligned pointer");
		return reinterpret_cast<DestPtrType*>(input);
	}

	namespace memory_impl
	{
		template<bool is_enum = true>
		struct safe_static_cast_impl
		{
			template<typename DestIntegralType, typename SrcEnumType>
			static inline DestIntegralType cast(SrcEnumType input)
			{
				typedef typename std::underlying_type<SrcEnumType>::type SrcIntegralType;
				SrcIntegralType integral_input = static_cast<SrcIntegralType>(input);
				ACL_ENSURE(integral_input >= std::numeric_limits<DestIntegralType>::min() && integral_input <= std::numeric_limits<DestIntegralType>::max(), "static_cast would result in truncation");
				return static_cast<DestIntegralType>(input);
			}
		};

		template<>
		struct safe_static_cast_impl<false>
		{
			template<typename DestNumericType, typename SrcNumericType>
			static inline DestNumericType cast(SrcNumericType input)
			{
				ACL_ENSURE(input >= std::numeric_limits<DestNumericType>::min() && input <= std::numeric_limits<DestNumericType>::max(), "static_cast would result in truncation");
				return static_cast<DestNumericType>(input);
			}
		};
	}

	template<typename DestIntegralType, typename SrcType>
	inline DestIntegralType safe_static_cast(SrcType input)
	{
		return memory_impl::safe_static_cast_impl<std::is_enum<SrcType>::value>::template cast<DestIntegralType, SrcType>(input);
	}

	template<typename OutputPtrType, typename InputPtrType, typename OffsetType>
	constexpr OutputPtrType* add_offset_to_ptr(InputPtrType* ptr, OffsetType offset)
	{
		return safe_ptr_cast<OutputPtrType>(reinterpret_cast<uintptr_t>(ptr) + offset);
	}

	//////////////////////////////////////////////////////////////////////////

	struct InvalidPtrOffset {};

	template<typename DataType, typename OffsetType>
	class PtrOffset
	{
	public:
		constexpr PtrOffset() : m_value(0) {}
		constexpr PtrOffset(size_t value) : m_value(safe_static_cast<OffsetType>(value)) {}
		constexpr PtrOffset(InvalidPtrOffset) : m_value(std::numeric_limits<OffsetType>::max()) {}

		template<typename BaseType>
		constexpr DataType* add_to(BaseType* ptr) const
		{
			ACL_ENSURE(is_valid(), "Invalid PtrOffset!");
			return add_offset_to_ptr<DataType>(ptr, m_value);
		}

		template<typename BaseType>
		constexpr const DataType* add_to(const BaseType* ptr) const
		{
			ACL_ENSURE(is_valid(), "Invalid PtrOffset!");
			return add_offset_to_ptr<const DataType>(ptr, m_value);
		}

		template<typename BaseType>
		constexpr DataType* safe_add_to(BaseType* ptr) const
		{
			return is_valid() ? add_offset_to_ptr<DataType>(ptr, m_value) : nullptr;
		}

		template<typename BaseType>
		constexpr const DataType* safe_add_to(const BaseType* ptr) const
		{
			return is_valid() ? add_offset_to_ptr<DataType>(ptr, m_value) : nullptr;
		}

		constexpr operator OffsetType() const { return m_value; }

		constexpr bool is_valid() const { return m_value != std::numeric_limits<OffsetType>::max(); }

	private:
		OffsetType m_value;
	};

	template<typename DataType>
	using PtrOffset16 = PtrOffset<DataType, uint16_t>;

	template<typename DataType>
	using PtrOffset32 = PtrOffset<DataType, uint32_t>;

	//////////////////////////////////////////////////////////////////////////

	constexpr uint16_t byte_swap(uint16_t value)
	{
		return (value & 0x00FF) << 8 | (value & 0xFF00) >> 8;
	}

	inline uint32_t byte_swap(uint32_t value)
	{
		value = (value & 0x0000FFFF) << 16 | (value & 0xFFFF0000) >> 16;
		value = (value & 0x00FF00FF) << 8 | (value & 0xFF00FF00) >> 8;
		return value;
	}

	inline uint64_t byte_swap(uint64_t value)
	{
		value = (value & 0x00000000FFFFFFFF) << 32 | (value & 0xFFFFFFFF00000000) >> 32;
		value = (value & 0x0000FFFF0000FFFF) << 16 | (value & 0xFFFF0000FFFF0000) >> 16;
		value = (value & 0x00FF00FF00FF00FF) << 8 | (value & 0xFF00FF00FF00FF00) >> 8;
		return value;
	}

	// We copy bits assuming big-endian ordering for 'dest' and 'src'
	inline void memcpy_bits(void* dest, uint64_t dest_bit_offset, const void* src, uint64_t src_bit_offset, uint64_t num_bits_to_copy)
	{
		while (true)
		{
			uint64_t src_byte_offset = src_bit_offset / 8;
			uint8_t src_byte_bit_offset = safe_static_cast<uint8_t>(src_bit_offset % 8);
			uint64_t dest_byte_offset = dest_bit_offset / 8;
			uint8_t dest_byte_bit_offset = safe_static_cast<uint8_t>(dest_bit_offset % 8);

			const uint8_t* src_bytes = add_offset_to_ptr<const uint8_t>(src, src_byte_offset);
			uint8_t* dest_byte = add_offset_to_ptr<uint8_t>(dest, dest_byte_offset);

			// We'll copy only as many bits as there fits within 'dest' or as there are left
			uint8_t num_bits_dest_remain_in_byte = 8 - dest_byte_bit_offset;
			uint8_t num_bits_src_remain_in_byte = 8 - src_byte_bit_offset;
			uint64_t num_bits_copied = std::min<uint64_t>(std::min<uint8_t>(num_bits_dest_remain_in_byte, num_bits_src_remain_in_byte), num_bits_to_copy);
			uint8_t num_bits_copied_u8 = safe_static_cast<uint8_t>(num_bits_copied);

			// We'll shift and mask to retain the 'dest' bits prior to our offset and whatever remains after the copy
			uint8_t dest_shift_offset = dest_byte_bit_offset;
			uint8_t dest_byte_mask = ~(0xFF >> dest_shift_offset) | ~(0xFF << (8 - num_bits_copied_u8 - dest_byte_bit_offset));

			uint8_t src_shift_offset = 8 - src_byte_bit_offset - num_bits_copied_u8;
			uint8_t src_byte_mask = 0xFF >> (8 - num_bits_copied_u8);
			uint8_t src_insert_shift_offset = 8 - num_bits_copied_u8 - dest_byte_bit_offset;

			uint8_t partial_dest_value = *dest_byte & dest_byte_mask;
			uint8_t partial_src_value = (*src_bytes >> src_shift_offset) & src_byte_mask;
			*dest_byte = partial_dest_value | (partial_src_value << src_insert_shift_offset);

			if (num_bits_to_copy <= num_bits_copied)
				break;	// Done

			num_bits_to_copy -= num_bits_copied;
			dest_bit_offset += num_bits_copied;
			src_bit_offset += num_bits_copied;
		}
	}

	//////////////////////////////////////////////////////////////////////////

	// TODO: get an official L3 cache size
	constexpr size_t CACHE_FLUSH_BUFFER_BYTES = 20 * 1024 * 1024;

	inline uint8_t* allocate_cache_flush_buffer(Allocator& allocator)
	{
#if defined(ACL_SSE2_INTRINSICS)
		size_t alignment = alignof(__m128i);
#else
		size_t alignment = 1;
#endif

		return allocate_type_array_aligned<uint8_t>(allocator, CACHE_FLUSH_BUFFER_BYTES, alignment);
	}

	inline void deallocate_cache_flush_buffer(Allocator& allocator, uint8_t* buffer) { deallocate_type_array(allocator, buffer, CACHE_FLUSH_BUFFER_BYTES); }

	inline void flush_cache(uint8_t* buffer)
	{
#if defined(ACL_SSE2_INTRINSICS)
		const __m128i ones = _mm_set1_epi8(1);

		const __m128i* sentinel_ptr = safe_ptr_cast<__m128i>(buffer + CACHE_FLUSH_BUFFER_BYTES);

		for (__m128i* buffer_ptr = safe_ptr_cast<__m128i>(buffer); buffer_ptr < sentinel_ptr; ++buffer_ptr)
		{
			__m128i values = _mm_load_si128(buffer_ptr);
			values = _mm_add_epi8(values, ones);
			_mm_store_si128(buffer_ptr, values);
		}
#else
		for (size_t i = 0; i < CACHE_FLUSH_BUFFER_BYTES; ++i)
			++buffer[i];
#endif
	}
}
