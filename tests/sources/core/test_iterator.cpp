////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include <acl/core/iterator.h>
#include <acl/core/memory_utils.h>

#include <cstdint>

using namespace acl;

TEST_CASE("iterator", "[core][iterator]")
{
	constexpr uint32_t num_items = 3;
	uint32_t items[num_items];

	auto i = iterator<uint32_t>(items, num_items);

	SECTION("mutable returns correct type")
	{
		CHECK(std::is_same<uint32_t*, decltype(i.begin())>::value);
		CHECK(std::is_same<uint32_t*, decltype(i.end())>::value);
	}

	SECTION("const returns correct type")
	{
		auto ci = const_iterator<uint32_t>(items, num_items);

		CHECK(std::is_same<const uint32_t*, decltype(ci.begin())>::value);
		CHECK(std::is_same<const uint32_t*, decltype(ci.end())>::value);
	}

	SECTION("bounds are correct")
	{
		CHECK(i.begin() == items + 0);
		CHECK(i.end() == items + num_items);
	}
}
