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

#if defined(SJSON_CPP_PARSER)

#include <cstdint>

namespace acl
{
	struct ClipReaderError : sjson::ParserError
	{
		enum : uint32_t
		{
			UnsupportedVersion = sjson::ParserError::Last,
			NoParentBoneWithThatName,
			NoBoneWithThatName,
			UnsignedIntegerExpected,
		};

		ClipReaderError()
		{
		}

		ClipReaderError(const sjson::ParserError& e)
		{
			error = e.error;
			line = e.line;
			column = e.column;
		}

		virtual const char* get_description() const override
		{
			switch (error)
			{
			case UnsupportedVersion:
				return "This library does not support this version of animation file";
			case NoParentBoneWithThatName:
				return "There is no parent bone with this name";
			case NoBoneWithThatName:
				return "The skeleton does not define a bone with this name";
			case UnsignedIntegerExpected:
				return "An unsigned integer is expected here";
			default:
				return sjson::ParserError::get_description();
			}
		}
	};
}

#endif	// #if defined(SJSON_CPP_PARSER)
