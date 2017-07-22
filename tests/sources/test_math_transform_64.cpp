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
		Transform_64 transform_a = transform_set(rotation_around_z, x_axis);
		Vector4_64 result = transform_position(transform_a, x_axis);
		REQUIRE(vector_near_equal(result, vector_set(1.0, 1.0, 0.0), threshold));
		result = transform_position(transform_a, y_axis);
		REQUIRE(vector_near_equal(result, vector_set(0.0, 0.0, 0.0), threshold));

		Quat_64 rotation_around_x = quat_from_euler(deg2rad(0.0), deg2rad(0.0), deg2rad(90.0));
		Transform_64 transform_b = transform_set(rotation_around_x, y_axis);
		result = transform_position(transform_b, x_axis);
		REQUIRE(vector_near_equal(result, vector_set(1.0, 1.0, 0.0), threshold));
		result = transform_position(transform_b, y_axis);
		REQUIRE(vector_near_equal(result, vector_set(0.0, 1.0, -1.0), threshold));

		Transform_64 transform_ab = transform_mul(transform_a, transform_b);
		Transform_64 transform_ba = transform_mul(transform_b, transform_a);
		result = transform_position(transform_ab, x_axis);
		REQUIRE(vector_near_equal(result, vector_set(1.0, 1.0, -1.0), threshold));
		REQUIRE(vector_near_equal(result, transform_position(transform_b, transform_position(transform_a, x_axis)), threshold));
		result = transform_position(transform_ab, y_axis);
		REQUIRE(vector_near_equal(result, vector_set(0.0, 1.0, 0.0), threshold));
		REQUIRE(vector_near_equal(result, transform_position(transform_b, transform_position(transform_a, y_axis)), threshold));
		result = transform_position(transform_ba, x_axis);
		REQUIRE(vector_near_equal(result, vector_set(0.0, 1.0, 0.0), threshold));
		REQUIRE(vector_near_equal(result, transform_position(transform_a, transform_position(transform_b, x_axis)), threshold));
		result = transform_position(transform_ba, y_axis);
		REQUIRE(vector_near_equal(result, vector_set(0.0, 0.0, -1.0), threshold));
		REQUIRE(vector_near_equal(result, transform_position(transform_a, transform_position(transform_b, y_axis)), threshold));
	}
}
