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
#include <acl/core/enum_utils.h>

#include <cstdint>

using namespace acl;

enum class TestEnum : uint32_t
{
	Empty	= 0x0000,
	One		= 0x0001,
	Two		= 0x0002,
	Four	= 0x0004,
	All		= 0x0007,
};

ACL_IMPL_ENUM_FLAGS_OPERATORS(TestEnum)

TEST_CASE("enum utils", "[core][utils]")
{
	REQUIRE(are_any_enum_flags_set(TestEnum::One | TestEnum::Two, TestEnum::All) == true);
	REQUIRE(are_any_enum_flags_set(TestEnum::One | TestEnum::Two, TestEnum::One) == true);
	REQUIRE(are_any_enum_flags_set(TestEnum::All, TestEnum::One | TestEnum::Two) == true);
	REQUIRE(are_any_enum_flags_set(TestEnum::One | TestEnum::Two, TestEnum::Four) == false);

	REQUIRE(are_all_enum_flags_set(TestEnum::One | TestEnum::Two, TestEnum::One | TestEnum::Two) == true);
	REQUIRE(are_all_enum_flags_set(TestEnum::One, TestEnum::One | TestEnum::Two) == false);
	REQUIRE(are_all_enum_flags_set(TestEnum::One | TestEnum::Two, TestEnum::One) == true);
	REQUIRE(are_all_enum_flags_set(TestEnum::One | TestEnum::Two, TestEnum::All) == false);
}
