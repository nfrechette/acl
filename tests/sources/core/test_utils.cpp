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

#include <acl/core/utils.h>
#include <acl/math/scalar_32.h>

#include <limits>

using namespace acl;

TEST_CASE("misc utils", "[core][utils]")
{
	REQUIRE(calculate_num_samples(0.0F, 30.0F) == 0);
	REQUIRE(calculate_num_samples(1.0F, 30.0F) == 31);
	REQUIRE(calculate_num_samples(1.0F, 24.0F) == 25);
	REQUIRE(calculate_num_samples(1.0F / 30.F, 30.0F) == 2);
	REQUIRE(calculate_num_samples(std::numeric_limits<float>::infinity(), 30.0F) == 1);

	REQUIRE(calculate_duration(0, 30.0F) == 0.0F);
	REQUIRE(calculate_duration(1, 30.0F) == std::numeric_limits<float>::infinity());
	REQUIRE(calculate_duration(1, 8.0F) == std::numeric_limits<float>::infinity());
	REQUIRE(scalar_near_equal(calculate_duration(31, 30.0F), 1.0F, 1.0E-8F));
	REQUIRE(scalar_near_equal(calculate_duration(9, 8.0F), 1.0F, 1.0E-8F));
}
