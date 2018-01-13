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

#include <catch.hpp>

#include <acl/math/transform_64.h>

using namespace acl;

TEST_CASE("transform math 64", "[math][transform64]")
{
	constexpr double threshold = 1e-6;

	{
		Vector4_64 x_axis = vector_set(1.0, 0.0, 0.0);
		Vector4_64 y_axis = vector_set(0.0, 1.0, 0.0);

		Quat_64 rotation_around_z = quat_from_euler(deg2rad(0.0), deg2rad(90.0), deg2rad(0.0));
		Transform_64 transform_a = transform_set(rotation_around_z, x_axis, vector_set(1.0));
		Vector4_64 result = transform_position(transform_a, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0, 1.0, 0.0), threshold));
		result = transform_position(transform_a, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 0.0, 0.0), threshold));

		Quat_64 rotation_around_x = quat_from_euler(deg2rad(0.0), deg2rad(0.0), deg2rad(90.0));
		Transform_64 transform_b = transform_set(rotation_around_x, y_axis, vector_set(1.0));
		result = transform_position(transform_b, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0, 1.0, 0.0), threshold));
		result = transform_position(transform_b, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, -1.0), threshold));

		Transform_64 transform_ab = transform_mul(transform_a, transform_b);
		Transform_64 transform_ba = transform_mul(transform_b, transform_a);
		result = transform_position(transform_ab, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0, 1.0, -1.0), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_b, transform_position(transform_a, x_axis)), threshold));
		result = transform_position(transform_ab, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, 0.0), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_b, transform_position(transform_a, y_axis)), threshold));
		result = transform_position(transform_ba, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, 0.0), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_a, transform_position(transform_b, x_axis)), threshold));
		result = transform_position(transform_ba, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 0.0, -1.0), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_a, transform_position(transform_b, y_axis)), threshold));
	}
}
