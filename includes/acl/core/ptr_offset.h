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
	struct InvalidPtrOffset {};

	template<typename DataType, typename OffsetType>
	class PtrOffset
	{
	public:
		constexpr PtrOffset() : m_value(0) {}
		constexpr PtrOffset(size_t value) : m_value(safe_static_cast<OffsetType>(value)) {}
		constexpr PtrOffset(InvalidPtrOffset) : m_value(std::numeric_limits<OffsetType>::max()) {}

		template<typename BaseType>
		inline DataType* add_to(BaseType* ptr) const
		{
			ACL_ENSURE(is_valid(), "Invalid PtrOffset!");
			return add_offset_to_ptr<DataType>(ptr, m_value);
		}

		template<typename BaseType>
		inline const DataType* add_to(const BaseType* ptr) const
		{
			ACL_ENSURE(is_valid(), "Invalid PtrOffset!");
			return add_offset_to_ptr<const DataType>(ptr, m_value);
		}

		template<typename BaseType>
		inline DataType* safe_add_to(BaseType* ptr) const
		{
			return is_valid() ? add_offset_to_ptr<DataType>(ptr, m_value) : nullptr;
		}

		template<typename BaseType>
		inline const DataType* safe_add_to(const BaseType* ptr) const
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
}
