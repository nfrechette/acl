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

/// \file
/// The result of a function that can fail.

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	/// This class is meant to encapsulate a simple error message and easily
	/// coerce with `bool` in order to simplify and clean up error handling.
	/// In an ideal world, I would love for a C++11 equivalent of std::result in Rust.
	class error_result
	{
	public:
		/// Constructs a result without an error.
		error_result() : m_error(nullptr) {}

		/// Constructs a result with the given error message. The string must stay valid as long as the result uses it.
		explicit error_result(const char* error) : m_error(error) {}

		/// Returns true if there is no error.
		bool empty() const { return m_error == nullptr; }

		/// Returns true if there is an error.
		bool any() const { return m_error != nullptr; }

		/// Returns the error message, or "" if there is no error.
		const char* c_str() const { return m_error != nullptr ? m_error : ""; }

		/// Removes the error.
		void reset() { m_error = nullptr; }

	private:
		const char*		m_error;
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
