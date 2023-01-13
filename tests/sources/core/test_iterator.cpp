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

TEST_CASE("array_iterator", "[core][iterator]")
{
	SECTION("mutable returns correct type")
	{
		constexpr size_t num_items = 3;
		uint32_t items[num_items] = { 0, 1, 2 };

		auto i = array_iterator<uint32_t>(items, num_items);

		CHECK(std::is_same<uint32_t*, decltype(i.begin())>::value);
		CHECK(std::is_same<uint32_t*, decltype(i.end())>::value);
	}

	SECTION("const returns correct type")
	{
		constexpr size_t num_items = 3;
		const uint32_t items[num_items] = { 0, 1, 2 };

		auto ci = const_array_iterator<uint32_t>(items, num_items);

		CHECK(std::is_same<const uint32_t*, decltype(ci.begin())>::value);
		CHECK(std::is_same<const uint32_t*, decltype(ci.end())>::value);
	}

	SECTION("bounds are correct")
	{
		constexpr size_t num_items = 3;
		uint32_t items[num_items] = { 0, 1, 2 };

		auto i = array_iterator<uint32_t>(items, num_items);

		CHECK(i.begin() == items + 0);
		CHECK(i.end() == items + num_items);
	}

	SECTION("const bounds are correct")
	{
		constexpr size_t num_items = 3;
		const uint32_t items[num_items] = { 0, 1, 2 };

		auto ci = const_array_iterator<uint32_t>(items, num_items);

		CHECK(ci.begin() == items + 0);
		CHECK(ci.end() == items + num_items);
	}

	SECTION("make_iterator matches")
	{
		constexpr size_t num_items = 3;
		uint32_t items[num_items] = { 0, 1, 2 };

		auto i = array_iterator<uint32_t>(items, num_items);
		auto j = make_iterator(items);

		CHECK(i.begin() == j.begin());
		CHECK(i.end() == j.end());
	}

	SECTION("make_iterator const matches")
	{
		constexpr size_t num_items = 3;
		const uint32_t items[num_items] = { 0, 1, 2 };

		auto ci = const_array_iterator<uint32_t>(items, num_items);
		auto cj = make_iterator(items);

		CHECK(ci.begin() == cj.begin());
		CHECK(ci.end() == cj.end());
	}
}

TEST_CASE("array_reverse_iterator", "[core][iterator]")
{
	SECTION("mutable returns correct type")
	{
		constexpr size_t num_items = 3;
		uint32_t items[num_items] = { 0, 1, 2 };

		auto i = array_reverse_iterator<uint32_t>(items, num_items);

		CHECK(std::is_same<uint32_t*, decltype(&*i.begin())>::value);
		CHECK(std::is_same<uint32_t*, decltype(&*i.end())>::value);
	}

	SECTION("const returns correct type")
	{
		constexpr size_t num_items = 3;
		const uint32_t items[num_items] = { 0, 1, 2 };

		auto ci = const_array_reverse_iterator<uint32_t>(items, num_items);

		CHECK(std::is_same<const uint32_t*, decltype(&*ci.begin())>::value);
		CHECK(std::is_same<const uint32_t*, decltype(&*ci.end())>::value);
	}

	// Suppress warning about array bounds check around 'items - 1' since we don't read from that value, it is a false positive
#if defined(RTM_COMPILER_GCC)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

	SECTION("bounds are correct")
	{
		constexpr size_t num_items = 3;
		uint32_t items[num_items] = { 0, 1, 2 };

		auto i = array_reverse_iterator<uint32_t>(items, num_items);

		CHECK(&*i.begin() == items + num_items - 1);
		CHECK(&*i.end() == items - 1);
	}

	SECTION("const bounds are correct")
	{
		constexpr size_t num_items = 3;
		const uint32_t items[num_items] = { 0, 1, 2 };

		auto ci = const_array_reverse_iterator<uint32_t>(items, num_items);

		CHECK(&*ci.begin() == items + num_items - 1);
		CHECK(&*ci.end() == items - 1);
	}

#if defined(RTM_COMPILER_GCC)
	#pragma GCC diagnostic pop
#endif

	SECTION("make_reverse_iterator matches")
	{
		constexpr size_t num_items = 3;
		uint32_t items[num_items] = { 0, 1, 2 };

		auto i = array_reverse_iterator<uint32_t>(items, num_items);
		auto j = make_reverse_iterator(items);

		CHECK(i.begin() == j.begin());
		CHECK(i.end() == j.end());
	}

	SECTION("make_reverse_iterator const matches")
	{
		constexpr size_t num_items = 3;
		const uint32_t items[num_items] = { 0, 1, 2 };

		auto ci = const_array_reverse_iterator<uint32_t>(items, num_items);
		auto cj = make_reverse_iterator(items);

		CHECK(ci.begin() == cj.begin());
		CHECK(ci.end() == cj.end());
	}
}
