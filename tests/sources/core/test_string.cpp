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

#include <catch.hpp>

// Enable allocation tracking
#define ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS
#define ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS

#include <acl/core/ansi_allocator.h>
#include <acl/core/string.h>

#include <cstring>

using namespace acl;

TEST_CASE("String", "[core][string]")
{
	ANSIAllocator allocator;

	CHECK(String().size() == 0);
	CHECK(String().c_str() != nullptr);
	CHECK(String(allocator, "").size() == 0);
	CHECK(String(allocator, "").c_str() != nullptr);

	const char* str0 = "this is a test string";
	const char* str1 = "this is not a test string";
	const char* str2 = "this is a test asset!";

	CHECK(String(allocator, str0) == str0);
	CHECK(String(allocator, str0) != str1);
	CHECK(String(allocator, str0) != str2);
	CHECK(String(allocator, str0) == String(allocator, str0));
	CHECK(String(allocator, str0) != String(allocator, str1));
	CHECK(String(allocator, str0) != String(allocator, str2));
	CHECK(String(allocator, str0).c_str() != str0);
	CHECK(String(allocator, str0).size() == std::strlen(str0));
	CHECK(String(allocator, str0, 4) == String(allocator, str1, 4));
	CHECK(String(allocator, str0, 4) == "this");

	CHECK(String().empty() == true);
	CHECK(String(allocator, "").empty() == true);
	CHECK(String(allocator, str0).empty() == false);
}
