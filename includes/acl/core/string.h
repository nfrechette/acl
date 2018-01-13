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

#include "acl/core/iallocator.h"
#include "acl/core/string_view.h"

#include <memory>

namespace acl
{
	class String
	{
	public:
		String() : m_allocator(nullptr), m_chars(nullptr) {}

		String(IAllocator& allocator, const char* str, size_t len)
			: m_allocator(&allocator)
		{
			if (len > 0)
			{
				m_chars = allocate_type_array<char>(allocator, len + 1);
				std::memcpy(m_chars, str, len);
				m_chars[len] = '\0';
			}
			else
			{
				m_chars = nullptr;
			}
		}

		String(IAllocator& allocator, const char* str)
			: String(allocator, str, str != nullptr ? std::strlen(str) : 0)
		{}

		String(IAllocator& allocator, const StringView& view)
			: String(allocator, view.c_str(), view.size())
		{}

		String(IAllocator& allocator, const String& str)
			: String(allocator, str.c_str(), str.size())
		{}

		~String()
		{
			if (m_allocator != nullptr)
				deallocate_type_array(*m_allocator, m_chars, std::strlen(m_chars) + 1);
		}

		String(String&& other)
			: m_allocator(other.m_allocator)
			, m_chars(other.m_chars)
		{
			new(&other) String();
		}

		String& operator=(String&& other)
		{
			std::swap(m_allocator, other.m_allocator);
			std::swap(m_chars, other.m_chars);
			return *this;
		}

		bool operator==(const StringView& view) const { return view == m_chars; }

		const char* c_str() const { return m_chars != nullptr ? m_chars : ""; }
		size_t size() const { return m_chars != nullptr ? std::strlen(m_chars) : 0; }
		bool empty() const { return m_chars != nullptr ? (std::strlen(m_chars) == 1) : true; }

	private:
		IAllocator* m_allocator;
		char* m_chars;
	};
}
