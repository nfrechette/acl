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

#include <cstring>
#include <memory>

namespace acl
{
	class StringView
	{
	public:
		StringView()
			: m_chars(nullptr)
			, m_length(0)
		{
		}
		
		StringView(const char* chars, size_t length)
			: m_chars(chars)
			, m_length(length)
		{
		}

		StringView& operator=(const char* chars)
		{
			this->m_chars = chars;
			this->m_length = chars == nullptr ? 0 : std::strlen(chars);
			return *this;
		}	

		const char* get_chars() const { return m_chars; }
		size_t get_length() const { return m_length; }

		bool operator==(const char* c_str) const
		{
			return m_chars != nullptr && c_str != nullptr &&
				std::strlen(c_str) == m_length && std::memcmp(c_str, m_chars, m_length) == 0;
		}

		bool operator !=(const char* c_str) const { return !(*this == c_str); }

		bool operator==(const StringView& view) const
		{
			return m_chars != nullptr && view.m_chars != nullptr &&
				m_length == view.m_length && std::memcmp(view.m_chars, m_chars, m_length) == 0;
		}

		bool operator !=(const StringView& view) const { return !(*this == view); }

	private:
		const char* m_chars;
		size_t m_length;
	};
}
