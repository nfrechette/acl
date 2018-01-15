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

#include <acl/core/ansi_allocator.h>
#include <acl/core/string.h>
#include <acl/core/string_view.h>

#include <cstring>

using namespace acl;

TEST_CASE("String", "[core][string]")
{
	ANSIAllocator allocator;

	REQUIRE(String().size() == 0);
	REQUIRE(String().c_str() != nullptr);
	REQUIRE(String(allocator, "").size() == 0);
	REQUIRE(String(allocator, "").c_str() != nullptr);

	const char* str0 = "this is a test string";
	const char* str1 = "this is not a test string";
	const char* str2 = "this is a test asset!";

	REQUIRE(String(allocator, str0) == str0);
	REQUIRE(String(allocator, str0) == StringView(str0));
	REQUIRE(String(allocator, str0) != str1);
	REQUIRE(String(allocator, str0) != str2);
	REQUIRE(String(allocator, str0) == String(allocator, str0));
	REQUIRE(String(allocator, str0) != String(allocator, str1));
	REQUIRE(String(allocator, str0) != String(allocator, str2));
	REQUIRE(String(allocator, str0).c_str() != str0);
	REQUIRE(String(allocator, str0).size() == std::strlen(str0));
	REQUIRE(String(allocator, str0, 4) == String(allocator, str1, 4));
	REQUIRE(String(allocator, str0, 4) == "this");

	REQUIRE(String().empty() == true);
	REQUIRE(String(allocator, "").empty() == true);
	REQUIRE(String(allocator, str0).empty() == false);
}
