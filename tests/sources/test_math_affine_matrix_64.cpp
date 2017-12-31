#include <catch.hpp>

#include <acl/math/affine_matrix_64.h>
#include <acl/math/quat_64.h>

using namespace acl;

TEST_CASE("affine matrix math 64", "[math][affinematrix64]")
{
	constexpr double threshold = 1e-4;

	{
		Vector4_64 x_axis = vector_set(1.0, 0.0, 0.0);
		Vector4_64 y_axis = vector_set(0.0, 1.0, 0.0);

		Quat_64 rotation_around_z = quat_from_euler(deg2rad(0.0), deg2rad(90.0), deg2rad(0.0));
		AffineMatrix_64 mtx_a = matrix_set(rotation_around_z, x_axis, vector_set(1.0));
		Vector4_64 result = matrix_mul_position(mtx_a, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0, 1.0, 0.0), threshold));
		result = matrix_mul_position(mtx_a, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 0.0, 0.0), threshold));

		Quat_64 rotation_around_x = quat_from_euler(deg2rad(0.0), deg2rad(0.0), deg2rad(90.0));
		AffineMatrix_64 mtx_b = matrix_set(rotation_around_x, y_axis, vector_set(1.0));
		result = matrix_mul_position(mtx_b, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0, 1.0, 0.0), threshold));
		result = matrix_mul_position(mtx_b, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, -1.0), threshold));

		AffineMatrix_64 mtx_ab = matrix_mul(mtx_a, mtx_b);
		AffineMatrix_64 mtx_ba = matrix_mul(mtx_b, mtx_a);
		result = matrix_mul_position(mtx_ab, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0, 1.0, -1.0), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_b, matrix_mul_position(mtx_a, x_axis)), threshold));
		result = matrix_mul_position(mtx_ab, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, 0.0), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_b, matrix_mul_position(mtx_a, y_axis)), threshold));
		result = matrix_mul_position(mtx_ba, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, 0.0), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_a, matrix_mul_position(mtx_b, x_axis)), threshold));
		result = matrix_mul_position(mtx_ba, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 0.0, -1.0), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_a, matrix_mul_position(mtx_b, y_axis)), threshold));
	}
}
