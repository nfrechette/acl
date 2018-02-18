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

#include "../error_exceptions.h"
#include <acl/core/string_view.h>

#include <cstring>

using namespace acl;

TEST_CASE("StringView", "[core][string]")
{
	REQUIRE(StringView() == StringView(""));
	REQUIRE(StringView() == "");
	REQUIRE(StringView().size() == 0);
	REQUIRE(StringView().c_str() != nullptr);
	REQUIRE(StringView("").size() == 0);
	REQUIRE(StringView("").c_str() != nullptr);

	const char* str0 = "this is a test string";
	const char* str1 = "this is not a test string";
	const char* str2 = "this is a test asset!";

	REQUIRE(StringView(str0) == str0);
	REQUIRE(StringView(str0) != str1);
	REQUIRE(StringView(str0) != str2);
	REQUIRE(StringView(str0) == StringView(str0));
	REQUIRE(StringView(str0) != StringView(str1));
	REQUIRE(StringView(str0) != StringView(str2));
	REQUIRE(StringView(str0).c_str() == str0);
	REQUIRE(StringView(str0).size() == std::strlen(str0));
	REQUIRE(StringView(str0, 4) == StringView(str1, 4));
	REQUIRE(StringView(str0, 4) == StringView("this"));

	StringView view0(str0);
	REQUIRE(view0 == str0);
	view0 = str1;
	REQUIRE(view0 == str1);

	REQUIRE(StringView().empty() == true);
	REQUIRE(StringView("").empty() == true);
	REQUIRE(view0.empty() == false);
}
