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

#include <catch2/catch.hpp>

// Enable allocation tracking
#define ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS
#define ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS

#include <acl/core/ansi_allocator.h>
#include <acl/core/string.h>

#include <cstring>

using namespace acl;

TEST_CASE("string", "[core][string]")
{
	ansi_allocator allocator;
	ansi_allocator allocator2;

	CHECK(string().get_allocator() == nullptr);
	CHECK(string().size() == 0);
	CHECK(string().c_str() != nullptr);
	CHECK(string().get_copy().size() == 0);
	CHECK(string().get_copy().get_allocator() == nullptr);
	CHECK(string().get_copy(allocator).get_allocator() == nullptr);
	CHECK(string(allocator, "").size() == 0);
	CHECK(string(allocator, "").c_str() != nullptr);

	const char* str0 = "this is a test string";
	const char* str1 = "this is not a test string";
	const char* str2 = "this is a test asset!";

	CHECK(string(allocator, str0) == str0);
	CHECK(string(allocator, str0) != str1);
	CHECK(string(allocator, str0) != str2);
	CHECK(string(allocator, str0) == string(allocator, str0));
	CHECK(string(allocator, str0) != string(allocator, str1));
	CHECK(string(allocator, str0) != string(allocator, str2));
	CHECK(string(allocator, str0).get_allocator() == &allocator);
	CHECK(string(allocator, str0).c_str() != str0);
	CHECK(string(allocator, str0).size() == std::strlen(str0));
	CHECK(string(allocator, str0).get_copy().size() == std::strlen(str0));
	CHECK(string(allocator, str0).get_copy().get_allocator() == &allocator);
	CHECK(string(allocator, str0).get_copy() == string(allocator, str0));
	CHECK(string(allocator, str0).get_copy(allocator2).size() == std::strlen(str0));
	CHECK(string(allocator, str0).get_copy(allocator2).get_allocator() == &allocator2);
	CHECK(string(allocator, str0).get_copy(allocator2) == string(allocator, str0));
	CHECK(string(allocator, str0, 4) == string(allocator, str1, 4));
	CHECK(string(allocator, str0, 4) == "this");

	CHECK(string().empty() == true);
	CHECK(string(allocator, "").empty() == true);
	CHECK(string(allocator, str0).empty() == false);
}
