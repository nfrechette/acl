#include <catch.hpp>

#include <acl/math/transform_32.h>

using namespace acl;

TEST_CASE("transform math 32", "[math][transform32]")
{
	constexpr float threshold = 1e-4f;

	{
		Vector4_32 x_axis = vector_set(1.0f, 0.0f, 0.0f);
		Vector4_32 y_axis = vector_set(0.0f, 1.0f, 0.0f);

		Quat_32 rotation_around_z = quat_from_euler(deg2rad(0.0f), deg2rad(90.0f), deg2rad(0.0f));
		Transform_32 transform_a = transform_set(rotation_around_z, x_axis, vector_set(1.0f));
		Vector4_32 result = transform_position(transform_a, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0f, 1.0f, 0.0f), threshold));
		result = transform_position(transform_a, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 0.0f, 0.0f), threshold));

		Quat_32 rotation_around_x = quat_from_euler(deg2rad(0.0f), deg2rad(0.0f), deg2rad(90.0f));
		Transform_32 transform_b = transform_set(rotation_around_x, y_axis, vector_set(1.0f));
		result = transform_position(transform_b, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0f, 1.0f, 0.0f), threshold));
		result = transform_position(transform_b, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 1.0f, -1.0f), threshold));

		Transform_32 transform_ab = transform_mul(transform_a, transform_b);
		Transform_32 transform_ba = transform_mul(transform_b, transform_a);
		result = transform_position(transform_ab, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0f, 1.0f, -1.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_b, transform_position(transform_a, x_axis)), threshold));
		result = transform_position(transform_ab, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 1.0f, 0.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_b, transform_position(transform_a, y_axis)), threshold));
		result = transform_position(transform_ba, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 1.0f, 0.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_a, transform_position(transform_b, x_axis)), threshold));
		result = transform_position(transform_ba, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 0.0f, -1.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, transform_position(transform_a, transform_position(transform_b, y_axis)), threshold));
	}
}
