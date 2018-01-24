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

#include <acl/math/affine_matrix_32.h>
#include <acl/math/affine_matrix_64.h>
#include <acl/math/quat_32.h>
#include <acl/math/quat_64.h>

using namespace acl;

template<typename MatrixType, typename QuatType, typename Vector4Type, typename FloatType>
static void test_affine_matrix_impl(const MatrixType& identity, const FloatType threshold)
{
	{
		Vector4Type x_axis = vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0));
		Vector4Type y_axis = vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0));

		QuatType rotation_around_z = quat_from_euler(deg2rad(FloatType(0.0)), deg2rad(FloatType(90.0)), deg2rad(FloatType(0.0)));
		MatrixType mtx_a = matrix_set(rotation_around_z, x_axis, vector_set(FloatType(1.0)));
		Vector4Type result = matrix_mul_position(mtx_a, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(1.0), FloatType(1.0), FloatType(0.0)), threshold));
		result = matrix_mul_position(mtx_a, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(0.0), FloatType(0.0)), threshold));

		QuatType rotation_around_x = quat_from_euler(deg2rad(FloatType(0.0)), deg2rad(FloatType(0.0)), deg2rad(FloatType(90.0)));
		MatrixType mtx_b = matrix_set(rotation_around_x, y_axis, vector_set(FloatType(1.0)));
		result = matrix_mul_position(mtx_b, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(1.0), FloatType(1.0), FloatType(0.0)), threshold));
		result = matrix_mul_position(mtx_b, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(1.0), FloatType(-1.0)), threshold));

		MatrixType mtx_ab = matrix_mul(mtx_a, mtx_b);
		MatrixType mtx_ba = matrix_mul(mtx_b, mtx_a);
		result = matrix_mul_position(mtx_ab, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(1.0), FloatType(1.0), FloatType(-1.0)), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_b, matrix_mul_position(mtx_a, x_axis)), threshold));
		result = matrix_mul_position(mtx_ab, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0)), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_b, matrix_mul_position(mtx_a, y_axis)), threshold));
		result = matrix_mul_position(mtx_ba, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0)), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_a, matrix_mul_position(mtx_b, x_axis)), threshold));
		result = matrix_mul_position(mtx_ba, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(0.0), FloatType(-1.0)), threshold));
		REQUIRE(vector_all_near_equal3(result, matrix_mul_position(mtx_a, matrix_mul_position(mtx_b, y_axis)), threshold));
	}
}

TEST_CASE("affine matrix 32 math", "[math][affinematrix]")
{
	test_affine_matrix_impl<AffineMatrix_32, Quat_32, Vector4_32, float>(matrix_identity_32(), 1.0e-4f);

	//const Quat_32 src_rotation = quat_set(0.39564531008956383f, 0.044254239301713752f, 0.22768840967675355f, 0.88863059760894492f);
	//const Vector4_32 src_translation = vector_set(-2.65f, 2.996113f, 0.68123521f);
	//const Vector4_32 src_scale = vector_set(1.2f, 0.8f, 2.1f);
	//const Transform_32 src = transform_set(src_rotation, src_translation, src_scale);
	//const Transform_64 dst = transform_cast(src);
	//REQUIRE(quat_near_equal(src.rotation, quat_cast(dst.rotation), 1.0e-6f));
	//REQUIRE(vector_all_near_equal3(src.translation, vector_cast(dst.translation), 1.0e-6f));
	//REQUIRE(vector_all_near_equal3(src.scale, vector_cast(dst.scale), 1.0e-6f));
}

TEST_CASE("affine matrix 64 math", "[math][affinematrix]")
{
	test_affine_matrix_impl<AffineMatrix_64, Quat_64, Vector4_64, double>(matrix_identity_64(), 1.0e-4);

	//const Quat_64 src_rotation = quat_set(0.39564531008956383, 0.044254239301713752, 0.22768840967675355, 0.88863059760894492);
	//const Vector4_64 src_translation = vector_set(-2.65, 2.996113, 0.68123521);
	//const Vector4_64 src_scale = vector_set(1.2, 0.8, 2.1);
	//const Transform_64 src = transform_set(src_rotation, src_translation, src_scale);
	//const Transform_32 dst = transform_cast(src);
	//REQUIRE(quat_near_equal(src.rotation, quat_cast(dst.rotation), 1.0e-6));
	//REQUIRE(vector_all_near_equal3(src.translation, vector_cast(dst.translation), 1.0e-6));
	//REQUIRE(vector_all_near_equal3(src.scale, vector_cast(dst.scale), 1.0e-6));
}
