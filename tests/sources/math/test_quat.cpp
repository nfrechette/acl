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

#include <acl/math/quat_32.h>
#include <acl/math/quat_64.h>
#include <acl/math/vector4_32.h>
#include <acl/math/vector4_64.h>

using namespace acl;

template<typename QuatType, typename Vector4Type, typename FloatType>
static Vector4Type quat_rotate_scalar(const QuatType& rotation, const Vector4Type& vector)
{
	// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)
	Vector4Type qv = vector_set(quat_get_x(rotation), quat_get_y(rotation), quat_get_z(rotation));
	Vector4Type vOut = vector_mul(vector_cross3(qv, vector), FloatType(2.0) * quat_get_w(rotation));
	vOut = vector_add(vOut, vector_mul(vector, (quat_get_w(rotation) * quat_get_w(rotation)) - vector_dot(qv, qv)));
	vOut = vector_add(vOut, vector_mul(qv, FloatType(2.0) * vector_dot(qv, vector)));
	return vOut;
}

template<typename QuatType, typename Vector4Type, typename FloatType>
static QuatType quat_mul_scalar(const QuatType& lhs, const QuatType& rhs)
{
	FloatType lhs_raw[4] = { quat_get_x(lhs), quat_get_y(lhs), quat_get_z(lhs), quat_get_w(lhs) };
	FloatType rhs_raw[4] = { quat_get_x(rhs), quat_get_y(rhs), quat_get_z(rhs), quat_get_w(rhs) };

	FloatType x = (rhs_raw[3] * lhs_raw[0]) + (rhs_raw[0] * lhs_raw[3]) + (rhs_raw[1] * lhs_raw[2]) - (rhs_raw[2] * lhs_raw[1]);
	FloatType y = (rhs_raw[3] * lhs_raw[1]) - (rhs_raw[0] * lhs_raw[2]) + (rhs_raw[1] * lhs_raw[3]) + (rhs_raw[2] * lhs_raw[0]);
	FloatType z = (rhs_raw[3] * lhs_raw[2]) + (rhs_raw[0] * lhs_raw[1]) - (rhs_raw[1] * lhs_raw[0]) + (rhs_raw[2] * lhs_raw[3]);
	FloatType w = (rhs_raw[3] * lhs_raw[3]) - (rhs_raw[0] * lhs_raw[0]) - (rhs_raw[1] * lhs_raw[1]) - (rhs_raw[2] * lhs_raw[2]);

	return quat_set(x, y, z, w);
}

template<typename QuatType, typename Vector4Type, typename FloatType>
static void test_quat_impl(const Vector4Type& zero, const QuatType& identity, const FloatType threshold)
{
	{
		QuatType quat0 = quat_from_euler(deg2rad(FloatType(30.0)), deg2rad(FloatType(-45.0)), deg2rad(FloatType(90.0)));
		QuatType quat1 = quat_from_euler(deg2rad(FloatType(45.0)), deg2rad(FloatType(60.0)), deg2rad(FloatType(120.0)));
		QuatType result = quat_mul(quat0, quat1);
		QuatType result_ref = quat_mul_scalar<QuatType, Vector4Type, FloatType>(quat0, quat1);
		REQUIRE(quat_near_equal(result, result_ref, threshold));

		quat0 = quat_set(FloatType(0.39564531008956383), FloatType(0.044254239301713752), FloatType(0.22768840967675355), FloatType(0.88863059760894492));
		quat1 = quat_set(FloatType(1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0));
		result = quat_mul(quat0, quat1);
		result_ref = quat_mul_scalar<QuatType, Vector4Type, FloatType>(quat0, quat1);
		REQUIRE(quat_near_equal(result, result_ref, threshold));
	}

	{
		Vector4Type x_axis = vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0));
		Vector4Type y_axis = vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0));

		QuatType rotation_around_z = quat_from_euler(deg2rad(FloatType(0.0)), deg2rad(FloatType(90.0)), deg2rad(FloatType(0.0)));
		Vector4Type result = quat_rotate(rotation_around_z, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0)), threshold));
		result = quat_rotate(rotation_around_z, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(-1.0), FloatType(0.0), FloatType(0.0)), threshold));

		QuatType rotation_around_x = quat_from_euler(deg2rad(FloatType(0.0)), deg2rad(FloatType(0.0)), deg2rad(FloatType(90.0)));
		result = quat_rotate(rotation_around_x, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0)), threshold));
		result = quat_rotate(rotation_around_x, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(0.0), FloatType(-1.0)), threshold));

		QuatType rotation_xz = quat_mul(rotation_around_x, rotation_around_z);
		QuatType rotation_zx = quat_mul(rotation_around_z, rotation_around_x);
		result = quat_rotate(rotation_xz, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0)), threshold));
		result = quat_rotate(rotation_xz, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(0.0), FloatType(-1.0)), threshold));
		result = quat_rotate(rotation_zx, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(0.0), FloatType(0.0), FloatType(-1.0)), threshold));
		result = quat_rotate(rotation_zx, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(FloatType(-1.0), FloatType(0.0), FloatType(0.0)), threshold));
	}

	{
		const QuatType test_rotations[] = {
			identity,
			quat_from_euler(deg2rad(FloatType(30.0)), deg2rad(FloatType(-45.0)), deg2rad(FloatType(90.0))),
			quat_from_euler(deg2rad(FloatType(45.0)), deg2rad(FloatType(60.0)), deg2rad(FloatType(120.0))),
			quat_from_euler(deg2rad(FloatType(0.0)), deg2rad(FloatType(180.0)), deg2rad(FloatType(45.0))),
			quat_from_euler(deg2rad(FloatType(-120.0)), deg2rad(FloatType(-90.0)), deg2rad(FloatType(0.0))),
			quat_from_euler(deg2rad(FloatType(-0.01)), deg2rad(FloatType(0.02)), deg2rad(FloatType(-0.03))),
		};

		const Vector4Type test_vectors[] = {
			zero,
			vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0)),
			vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0)),
			vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0)),
			vector_set(FloatType(45.0), FloatType(-60.0), FloatType(120.0)),
			vector_set(FloatType(-45.0), FloatType(60.0), FloatType(-120.0)),
			vector_set(FloatType(0.57735026918962576451), FloatType(0.57735026918962576451), FloatType(0.57735026918962576451)),
			vector_set(FloatType(-1.0), FloatType(0.0), FloatType(0.0)),
		};

		for (size_t quat_index = 0; quat_index < get_array_size(test_rotations); ++quat_index)
		{
			const QuatType& rotation = test_rotations[quat_index];
			for (size_t vector_index = 0; vector_index < get_array_size(test_vectors); ++vector_index)
			{
				const Vector4Type& vector = test_vectors[vector_index];
				Vector4Type result = quat_rotate(rotation, vector);
				Vector4Type result_ref = quat_rotate_scalar<QuatType, Vector4Type, FloatType>(rotation, vector);
				REQUIRE(vector_all_near_equal3(result, result_ref, threshold));
			}
		}
	}

	{
		QuatType rotation = quat_from_euler(deg2rad(FloatType(0.0)), deg2rad(FloatType(90.0)), deg2rad(FloatType(0.0)));
		Vector4Type axis;
		FloatType angle;
		quat_to_axis_angle(rotation, axis, angle);
		REQUIRE(vector_all_near_equal3(axis, vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0)), threshold));
		REQUIRE(vector_all_near_equal3(quat_get_axis(rotation), vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0)), threshold));
		REQUIRE(scalar_near_equal(quat_get_angle(rotation), deg2rad(FloatType(90.0)), threshold));
	}

	{
		QuatType rotation = quat_from_euler(deg2rad(FloatType(0.0)), deg2rad(FloatType(90.0)), deg2rad(FloatType(0.0)));
		Vector4Type axis;
		FloatType angle;
		quat_to_axis_angle(rotation, axis, angle);
		QuatType rotation_new = quat_from_axis_angle(axis, angle);
		REQUIRE(quat_near_equal(rotation, rotation_new, threshold));
	}

	{
		QuatType rotation = quat_set(FloatType(0.39564531008956383), FloatType(0.044254239301713752), FloatType(0.22768840967675355), FloatType(0.88863059760894492));
		Vector4Type axis_ref = vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0));
		axis_ref = quat_rotate(rotation, axis_ref);
		FloatType angle_ref = deg2rad(FloatType(57.0));
		QuatType result = quat_from_axis_angle(axis_ref, angle_ref);
		Vector4Type axis;
		FloatType angle;
		quat_to_axis_angle(result, axis, angle);
		REQUIRE(vector_all_near_equal3(axis, axis_ref, threshold));
		REQUIRE(scalar_near_equal(angle, angle_ref, threshold));
	}
}

TEST_CASE("quat 32 math", "[math][quat]")
{
	test_quat_impl<Quat_32, Vector4_32, float>(vector_zero_32(), quat_identity_32(), 1.0e-4f);
}

TEST_CASE("quat 64 math", "[math][quat]")
{
	test_quat_impl<Quat_64, Vector4_64, double>(vector_zero_64(), quat_identity_64(), 1.0e-6);
}
