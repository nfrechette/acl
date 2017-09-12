#include <catch.hpp>

#include <acl/math/affine_matrix_32.h>
#include <acl/math/quat_32.h>

using namespace acl;

TEST_CASE("affine matrix math 32", "[math][affinematrix32]")
{
	constexpr float threshold = 1e-4f;

	{
		Vector4_32 x_axis = vector_set(1.0f, 0.0f, 0.0f);
		Vector4_32 y_axis = vector_set(0.0f, 1.0f, 0.0f);

		Quat_32 rotation_around_z = quat_from_euler(deg2rad(0.0f), deg2rad(90.0f), deg2rad(0.0f));
		AffineMatrix_32 mtx_a = matrix_set(rotation_around_z, x_axis, vector_set(1.0f));
		Vector4_32 result = matrix_mul_position(mtx_a, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0f, 1.0f, 0.0f), threshold));
		result = matrix_mul_position(mtx_a, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 0.0f, 0.0f), threshold));

		Quat_32 rotation_around_x = quat_from_euler(deg2rad(0.0f), deg2rad(0.0f), deg2rad(90.0f));
		AffineMatrix_32 mtx_b = matrix_set(rotation_around_x, y_axis, vector_set(1.0f));
		result = matrix_mul_position(mtx_b, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0f, 1.0f, 0.0f), threshold));
		result = matrix_mul_position(mtx_b, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 1.0f, -1.0f), threshold));

		AffineMatrix_32 mtx_ab = matrix_mul(mtx_a, mtx_b);
		AffineMatrix_32 mtx_ba = matrix_mul(mtx_b, mtx_a);
		result = matrix_mul_position(mtx_ab, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0f, 1.0f, -1.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_b, matrix_mul_position(mtx_a, x_axis)), threshold));
		result = matrix_mul_position(mtx_ab, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 1.0f, 0.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_b, matrix_mul_position(mtx_a, y_axis)), threshold));
		result = matrix_mul_position(mtx_ba, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 1.0f, 0.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_a, matrix_mul_position(mtx_b, x_axis)), threshold));
		result = matrix_mul_position(mtx_ba, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0f, 0.0f, -1.0f), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_a, matrix_mul_position(mtx_b, y_axis)), threshold));
	}
}
