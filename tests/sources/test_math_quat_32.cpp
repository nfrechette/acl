#include <catch.hpp>

#include <acl/math/quat_32.h>
#include <acl/math/vector4_32.h>

using namespace acl;

static Vector4_32 quat_rotate_scalar(const Quat_32& rotation, const Vector4_32& vector)
{
	// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)
	Vector4_32 qv = vector_set(quat_get_x(rotation), quat_get_y(rotation), quat_get_z(rotation));
	Vector4_32 vOut = vector_mul(vector_cross3(qv, vector), 2.0f * quat_get_w(rotation));
	vOut = vector_add(vOut, vector_mul(vector, (quat_get_w(rotation) * quat_get_w(rotation)) - vector_dot(qv, qv)));
	vOut = vector_add(vOut, vector_mul(qv, 2.0f * vector_dot(qv, vector)));
	return vOut;
}

static Quat_32 quat_mul_scalar(const Quat_32& lhs, const Quat_32& rhs)
{
	float lhs_raw[4] = { quat_get_x(lhs), quat_get_y(lhs), quat_get_z(lhs), quat_get_w(lhs) };
	float rhs_raw[4] = { quat_get_x(rhs), quat_get_y(rhs), quat_get_z(rhs), quat_get_w(rhs) };

	float x = (rhs_raw[3] * lhs_raw[0]) + (rhs_raw[0] * lhs_raw[3]) + (rhs_raw[1] * lhs_raw[2]) - (rhs_raw[2] * lhs_raw[1]);
	float y = (rhs_raw[3] * lhs_raw[1]) - (rhs_raw[0] * lhs_raw[2]) + (rhs_raw[1] * lhs_raw[3]) + (rhs_raw[2] * lhs_raw[0]);
	float z = (rhs_raw[3] * lhs_raw[2]) + (rhs_raw[0] * lhs_raw[1]) - (rhs_raw[1] * lhs_raw[0]) + (rhs_raw[2] * lhs_raw[3]);
	float w = (rhs_raw[3] * lhs_raw[3]) - (rhs_raw[0] * lhs_raw[0]) - (rhs_raw[1] * lhs_raw[1]) - (rhs_raw[2] * lhs_raw[2]);

	return quat_set(x, y, z, w);
}


TEST_CASE("quat misc math 32", "[math][quat32]")
{
	constexpr float threshold = 1e-5f;

	{
		Quat_32 quat0 = quat_from_euler(deg2rad(30.0f), deg2rad(-45.0f), deg2rad(90.0f));
		Quat_32 quat1 = quat_from_euler(deg2rad(45.0f), deg2rad(60.0f), deg2rad(120.0f));
		Quat_32 result = quat_mul(quat0, quat1);
		Quat_32 result_ref = quat_mul_scalar(quat0, quat1);
		REQUIRE(quat_near_equal(result, result_ref, threshold));

		quat0 = quat_set(0.39564531008956383f, 0.044254239301713752f, 0.22768840967675355f, 0.88863059760894492f);
		quat1 = quat_set(1.0f, 0.0f, 0.0f, 0.0f);
		result = quat_mul(quat0, quat1);
		result_ref = quat_mul_scalar(quat0, quat1);
		REQUIRE(quat_near_equal(result, result_ref, threshold));
	}

	{
		const Quat_32 test_rotations[] = {
			quat_identity_32(),
			quat_from_euler(deg2rad(30.0f), deg2rad(-45.0f), deg2rad(90.0f)),
			quat_from_euler(deg2rad(45.0f), deg2rad(60.0f), deg2rad(120.0f)),
			quat_from_euler(deg2rad(0.0f), deg2rad(180.0f), deg2rad(45.0f)),
			quat_from_euler(deg2rad(-120.0f), deg2rad(-90.0f), deg2rad(0.0f)),
			quat_from_euler(deg2rad(-0.01f), deg2rad(0.02f), deg2rad(-0.03f)),
		};

		const Vector4_32 test_vectors[] = {
			vector_zero_32(),
			vector_set(1.0f, 0.0f, 0.0f),
			vector_set(0.0f, 1.0f, 0.0f),
			vector_set(0.0f, 0.0f, 1.0f),
			vector_set(45.0f, -60.0f, 120.0f),
			vector_set(-45.0f, 60.0f, -120.0f),
			vector_set(0.57735026918962576451f, 0.57735026918962576451f, 0.57735026918962576451f),
			vector_set(-1.0f, 0.0f, 0.0f),
		};

		for (size_t quat_index = 0; quat_index < (sizeof(test_rotations) / sizeof(Quat_32)); ++quat_index)
		{
			const Quat_32& rotation = test_rotations[quat_index];
			for (size_t vector_index = 0; vector_index < (sizeof(test_vectors) / sizeof(Vector4_32)); ++vector_index)
			{
				const Vector4_32& vector = test_vectors[vector_index];
				Vector4_32 result = quat_rotate(rotation, vector);
				Vector4_32 result_ref = quat_rotate_scalar(rotation, vector);
				if (!vector_near_equal(result, result_ref, threshold))
					printf("");
				//REQUIRE(vector_near_equal(result, result_ref, threshold));
			}
		}
	}

	{
		Quat_32 rotation = quat_set(0.39564531008956383f, 0.044254239301713752f, 0.22768840967675355f, 0.88863059760894492f);
		Vector4_32 axis_ref = vector_set(1.0f, 0.0f, 0.0f);
		axis_ref = quat_rotate(rotation, axis_ref);
		float angle_ref = deg2rad(57.0f);
		Quat_32 result = quat_from_axis_angle(axis_ref, angle_ref);
		Vector4_32 axis;
		float angle;
		quat_to_axis_angle(result, axis, angle);
		REQUIRE(vector_near_equal(axis, axis_ref, threshold));
		REQUIRE(scalar_near_equal(angle, angle_ref, threshold));
	}
}
