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

#include <cstring>
#include <memory>

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// A StringView is just a pointer to a string and an associated length.
	// It does NOT own the memory and no allocation or deallocation ever takes place.
	// An empty StringView() is equal to a StringView("") of the empty string.
	// It is not legal to create a StringView that contains multiple NULL terminators
	// and if you do so, the behavior is undefined.
	//
	// Two different StringViews are equal if the strings pointed to are equal.
	// They do not need to be pointing to the same physical string.
	// StringView("this") == StringView("this is fun", 4)
	//
	// The string pointed to is immutable.
	//////////////////////////////////////////////////////////////////////////
	class StringView
	{
	public:
		constexpr StringView()
			: m_c_str(nullptr)
			, m_length(0)
		{}

		StringView(const char* c_str, size_t length)
			: m_c_str(c_str)
			, m_length(length)
		{
#if defined(ACL_USE_ERROR_CHECKS) && !defined(NDEBUG)
			for (size_t i = 0; i < length; ++i)
				ACL_ENSURE(c_str[i] != '\0', "StringView cannot contain NULL terminators");
#endif
		}

		StringView(const char* c_str)
			: m_c_str(c_str)
			, m_length(c_str == nullptr ? 0 : std::strlen(c_str))
		{}

		StringView& operator=(const char* c_str)
		{
			m_c_str = c_str;
			m_length = c_str == nullptr ? 0 : std::strlen(c_str);
			return *this;
		}

		constexpr const char* c_str() const { return m_length == 0 ? "" : m_c_str; }
		constexpr size_t size() const { return m_length; }
		constexpr bool empty() const { return m_length == 0; }

		bool operator==(const char* c_str) const
		{
			const size_t length = c_str == nullptr ? 0 : std::strlen(c_str);
			if (m_length != length)
				return false;

			if (m_c_str == c_str)
				return true;

			return std::memcmp(m_c_str, c_str, length) == 0;
		}

		bool operator !=(const char* c_str) const { return !(*this == c_str); }

		bool operator==(const StringView& other) const
		{
			if (m_length != other.m_length)
				return false;

			if (m_c_str == other.m_c_str)
				return true;

			return std::memcmp(m_c_str, other.m_c_str, m_length) == 0;
		}

		bool operator !=(const StringView& other) const { return !(*this == other); }

	private:
		const char* m_c_str;
		size_t m_length;
	};
}
