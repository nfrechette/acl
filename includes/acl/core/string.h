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

#include "acl/core/memory.h"
#include "acl/core/string_view.h"

#include <memory>

namespace acl
{
	class String
	{
	public:
		String()
			: m_chars()
		{
		}

		String(String&& string)
			: m_chars(std::move(string.m_chars))
		{
		}

		String& operator=(String&& string)
		{
			std::swap(m_chars, string.m_chars);
			return *this;
		}

		String(Allocator& allocator, const char* c_string)
			: m_chars(allocate_unique_type_array<char>(allocator, std::strlen(c_string) + 1))
		{
			std::memcpy(m_chars.get(), c_string, std::strlen(c_string) + 1);
		}

		String(Allocator& allocator, StringView& view)
			: m_chars(allocate_unique_type_array<char>(allocator, view.get_length() + 1))
		{
			char* own = m_chars.get();
			std::memcpy(own, view.get_chars(), view.get_length());
			own[view.get_length()] = '\0';
		}

		bool operator==(StringView& view) const { return view == m_chars.get(); }

	private:
		std::unique_ptr<char, Deleter<char>> m_chars;
	};
}
