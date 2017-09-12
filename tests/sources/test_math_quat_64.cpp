#include <catch.hpp>

#include <acl/math/quat_64.h>
#include <acl/math/vector4_64.h>

using namespace acl;

static Vector4_64 quat_rotate_scalar(const Quat_64& rotation, const Vector4_64& vector)
{
	// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)
	Vector4_64 qv = vector_set(quat_get_x(rotation), quat_get_y(rotation), quat_get_z(rotation));
	Vector4_64 vOut = vector_mul(vector_cross3(qv, vector), 2.0 * quat_get_w(rotation));
	vOut = vector_add(vOut, vector_mul(vector, (quat_get_w(rotation) * quat_get_w(rotation)) - vector_dot(qv, qv)));
	vOut = vector_add(vOut, vector_mul(qv, 2.0 * vector_dot(qv, vector)));
	return vOut;
}

static Quat_64 quat_mul_scalar(const Quat_64& lhs, const Quat_64& rhs)
{
	double lhs_raw[4] = { quat_get_x(lhs), quat_get_y(lhs), quat_get_z(lhs), quat_get_w(lhs) };
	double rhs_raw[4] = { quat_get_x(rhs), quat_get_y(rhs), quat_get_z(rhs), quat_get_w(rhs) };

	double x = (rhs_raw[3] * lhs_raw[0]) + (rhs_raw[0] * lhs_raw[3]) + (rhs_raw[1] * lhs_raw[2]) - (rhs_raw[2] * lhs_raw[1]);
	double y = (rhs_raw[3] * lhs_raw[1]) - (rhs_raw[0] * lhs_raw[2]) + (rhs_raw[1] * lhs_raw[3]) + (rhs_raw[2] * lhs_raw[0]);
	double z = (rhs_raw[3] * lhs_raw[2]) + (rhs_raw[0] * lhs_raw[1]) - (rhs_raw[1] * lhs_raw[0]) + (rhs_raw[2] * lhs_raw[3]);
	double w = (rhs_raw[3] * lhs_raw[3]) - (rhs_raw[0] * lhs_raw[0]) - (rhs_raw[1] * lhs_raw[1]) - (rhs_raw[2] * lhs_raw[2]);

	return quat_set(x, y, z, w);
}


TEST_CASE("quat math 64", "[math][quat64]")
{
	constexpr double threshold = 1e-6;

	{
		Quat_64 quat0 = quat_from_euler(deg2rad(30.0), deg2rad(-45.0), deg2rad(90.0));
		Quat_64 quat1 = quat_from_euler(deg2rad(45.0), deg2rad(60.0), deg2rad(120.0));
		Quat_64 result = quat_mul(quat0, quat1);
		Quat_64 result_ref = quat_mul_scalar(quat0, quat1);
		REQUIRE(quat_near_equal(result, result_ref, threshold));

		quat0 = quat_set(0.39564531008956383, 0.044254239301713752, 0.22768840967675355, 0.88863059760894492);
		quat1 = quat_set(1.0, 0.0, 0.0, 0.0);
		result = quat_mul(quat0, quat1);
		result_ref = quat_mul_scalar(quat0, quat1);
		REQUIRE(quat_near_equal(result, result_ref, threshold));
	}

	{
		Vector4_64 x_axis = vector_set(1.0, 0.0, 0.0);
		Vector4_64 y_axis = vector_set(0.0, 1.0, 0.0);

		Quat_64 rotation_around_z = quat_from_euler(deg2rad(0.0), deg2rad(90.0), deg2rad(0.0));
		Vector4_64 result = quat_rotate(rotation_around_z, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, 0.0), threshold));
		result = quat_rotate(rotation_around_z, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(-1.0, 0.0, 0.0), threshold));

		Quat_64 rotation_around_x = quat_from_euler(deg2rad(0.0), deg2rad(0.0), deg2rad(90.0));
		result = quat_rotate(rotation_around_x, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(1.0, 0.0, 0.0), threshold));
		result = quat_rotate(rotation_around_x, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 0.0, -1.0), threshold));

		Quat_64 rotation_xz = quat_mul(rotation_around_x, rotation_around_z);
		Quat_64 rotation_zx = quat_mul(rotation_around_z, rotation_around_x);
		result = quat_rotate(rotation_xz, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 1.0, 0.0), threshold));
		result = quat_rotate(rotation_xz, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 0.0, -1.0), threshold));
		result = quat_rotate(rotation_zx, x_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(0.0, 0.0, -1.0), threshold));
		result = quat_rotate(rotation_zx, y_axis);
		REQUIRE(vector_all_near_equal3(result, vector_set(-1.0, 0.0, 0.0), threshold));
	}

	{
		const Quat_64 test_rotations[] = {
			quat_identity_64(),
			quat_from_euler(deg2rad(30.0), deg2rad(-45.0), deg2rad(90.0)),
			quat_from_euler(deg2rad(45.0), deg2rad(60.0), deg2rad(120.0)),
			quat_from_euler(deg2rad(0.0), deg2rad(180.0), deg2rad(45.0)),
			quat_from_euler(deg2rad(-120.0), deg2rad(-90.0), deg2rad(0.0)),
			quat_from_euler(deg2rad(-0.01), deg2rad(0.02), deg2rad(-0.03)),
		};

		const Vector4_64 test_vectors[] = {
			vector_zero_64(),
			vector_set(1.0, 0.0, 0.0),
			vector_set(0.0, 1.0, 0.0),
			vector_set(0.0, 0.0, 1.0),
			vector_set(45.0, -60.0, 120.0),
			vector_set(-45.0, 60.0, -120.0),
			vector_set(0.57735026918962576451, 0.57735026918962576451, 0.57735026918962576451),
			vector_set(-1.0, 0.0, 0.0),
		};

		for (size_t quat_index = 0; quat_index < (sizeof(test_rotations) / sizeof(Quat_64)); ++quat_index)
		{
			const Quat_64& rotation = test_rotations[quat_index];
			for (size_t vector_index = 0; vector_index < (sizeof(test_vectors) / sizeof(Vector4_64)); ++vector_index)
			{
				const Vector4_64& vector = test_vectors[vector_index];
				Vector4_64 result = quat_rotate(rotation, vector);
				Vector4_64 result_ref = quat_rotate_scalar(rotation, vector);
				REQUIRE(vector_all_near_equal3(result, result_ref, threshold));
			}
		}
	}

	{
		Quat_64 rotation = quat_from_euler(deg2rad(0.0), deg2rad(90.0), deg2rad(0.0));
		Vector4_64 axis;
		double angle;
		quat_to_axis_angle(rotation, axis, angle);
		REQUIRE(vector_all_near_equal3(axis, vector_set(0.0, 0.0, 1.0), threshold));
		REQUIRE(vector_all_near_equal3(quat_get_axis(rotation), vector_set(0.0, 0.0, 1.0), threshold));
		REQUIRE(scalar_near_equal(quat_get_angle(rotation), deg2rad(90.0), threshold));
	}

	{
		Quat_64 rotation = quat_from_euler(deg2rad(0.0), deg2rad(90.0), deg2rad(0.0));
		Vector4_64 axis;
		double angle;
		quat_to_axis_angle(rotation, axis, angle);
		Quat_64 rotation_new = quat_from_axis_angle(axis, angle);
		REQUIRE(quat_near_equal(rotation, rotation_new, threshold));
	}

	{
		Quat_64 rotation = quat_set(0.39564531008956383, 0.044254239301713752, 0.22768840967675355, 0.88863059760894492);
		Vector4_64 axis_ref = vector_set(1.0, 0.0, 0.0);
		axis_ref = quat_rotate(rotation, axis_ref);
		double angle_ref = deg2rad(57.0);
		Quat_64 result = quat_from_axis_angle(axis_ref, angle_ref);
		Vector4_64 axis;
		double angle;
		quat_to_axis_angle(result, axis, angle);
		REQUIRE(vector_all_near_equal3(axis, axis_ref, threshold));
		REQUIRE(scalar_near_equal(angle, angle_ref, threshold));
	}
}
