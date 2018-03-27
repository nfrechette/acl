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

#include "acl/core/memory_utils.h"

#include <cstdint>

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// Represents an invalid pointer offset, used by 'PtrOffset'.
	////////////////////////////////////////////////////////////////////////////////
	struct InvalidPtrOffset {};

	////////////////////////////////////////////////////////////////////////////////
	// A type safe pointer offset.
	//
	// This class only wraps an integer of the 'OffsetType' type and adds type safety
	// by only casting to 'DataType'.
	////////////////////////////////////////////////////////////////////////////////
	template<typename DataType, typename OffsetType>
	class PtrOffset
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Constructs a valid but empty offset.
		constexpr PtrOffset() : m_value(0) {}

		////////////////////////////////////////////////////////////////////////////////
		// Constructs a valid offset with the specified value.
		constexpr PtrOffset(size_t value) : m_value(safe_static_cast<OffsetType>(value)) {}

		////////////////////////////////////////////////////////////////////////////////
		// Constructs an invalid offset.
		constexpr PtrOffset(InvalidPtrOffset) : m_value(std::numeric_limits<OffsetType>::max()) {}

		////////////////////////////////////////////////////////////////////////////////
		// Adds this offset to the provided pointer.
		template<typename BaseType>
		inline DataType* add_to(BaseType* ptr) const
		{
			ACL_ENSURE(is_valid(), "Invalid PtrOffset!");
			return add_offset_to_ptr<DataType>(ptr, m_value);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Adds this offset to the provided pointer.
		template<typename BaseType>
		inline const DataType* add_to(const BaseType* ptr) const
		{
			ACL_ENSURE(is_valid(), "Invalid PtrOffset!");
			return add_offset_to_ptr<const DataType>(ptr, m_value);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Adds this offset to the provided pointer or returns nullptr if the offset is invalid.
		template<typename BaseType>
		inline DataType* safe_add_to(BaseType* ptr) const
		{
			return is_valid() ? add_offset_to_ptr<DataType>(ptr, m_value) : nullptr;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Adds this offset to the provided pointer or returns nullptr if the offset is invalid.
		template<typename BaseType>
		inline const DataType* safe_add_to(const BaseType* ptr) const
		{
			return is_valid() ? add_offset_to_ptr<DataType>(ptr, m_value) : nullptr;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Coercion operator to the underlying 'OffsetType'.
		constexpr operator OffsetType() const { return m_value; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns true if the offset is valid.
		constexpr bool is_valid() const { return m_value != std::numeric_limits<OffsetType>::max(); }

	private:
		// Actual offset value.
		OffsetType m_value;
	};

	////////////////////////////////////////////////////////////////////////////////
	// A 16 bit offset.
	template<typename DataType>
	using PtrOffset16 = PtrOffset<DataType, uint16_t>;

	////////////////////////////////////////////////////////////////////////////////
	// A 32 bit offset.
	template<typename DataType>
	using PtrOffset32 = PtrOffset<DataType, uint32_t>;
}
