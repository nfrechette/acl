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

#include <limits>

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

template<typename QuatType, typename FloatType>
static FloatType scalar_dot(const QuatType& lhs, const QuatType& rhs)
{
	return (quat_get_x(lhs) * quat_get_x(rhs)) + (quat_get_y(lhs) * quat_get_y(rhs)) + (quat_get_z(lhs) * quat_get_z(rhs)) + (quat_get_w(lhs) * quat_get_w(rhs));
}

template<typename QuatType, typename FloatType>
static QuatType scalar_normalize(const QuatType& input)
{
	FloatType inv_len = FloatType(1.0) / acl::sqrt(scalar_dot<QuatType, FloatType>(input, input));
	return quat_set(quat_get_x(input) * inv_len, quat_get_y(input) * inv_len, quat_get_z(input) * inv_len, quat_get_w(input) * inv_len);
}

template<typename QuatType, typename FloatType>
static QuatType scalar_lerp(const QuatType& start, const QuatType& end, FloatType alpha)
{
	FloatType dot = scalar_dot<QuatType, FloatType>(start, end);
	FloatType bias = dot >= FloatType(0.0) ? FloatType(1.0) : FloatType(-1.0);
	FloatType x = quat_get_x(start) + ((quat_get_x(end) * bias) - quat_get_x(start)) * alpha;
	FloatType y = quat_get_y(start) + ((quat_get_y(end) * bias) - quat_get_y(start)) * alpha;
	FloatType z = quat_get_z(start) + ((quat_get_z(end) * bias) - quat_get_z(start)) * alpha;
	FloatType w = quat_get_w(start) + ((quat_get_w(end) * bias) - quat_get_w(start)) * alpha;
	return quat_normalize(quat_set(x, y, z, w));
}

template<typename QuatType, typename Vector4Type, typename FloatType>
static void test_quat_impl(const Vector4Type& zero, const QuatType& identity, const FloatType threshold)
{
	//////////////////////////////////////////////////////////////////////////
	// Setters, getters, and casts

	REQUIRE(quat_get_x(quat_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(0.0));
	REQUIRE(quat_get_y(quat_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(2.34));
	REQUIRE(quat_get_z(quat_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(-3.12));
	REQUIRE(quat_get_w(quat_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(10000.0));

	REQUIRE(quat_get_x(identity) == FloatType(0.0));
	REQUIRE(quat_get_y(identity) == FloatType(0.0));
	REQUIRE(quat_get_z(identity) == FloatType(0.0));
	REQUIRE(quat_get_w(identity) == FloatType(1.0));

	{
		struct alignas(16) Tmp
		{
			int32_t padding;
			alignas(8) FloatType values[4];
		};

		Tmp tmp = { 0, { FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0) } };
		REQUIRE(quat_get_x(quat_unaligned_load(&tmp.values[0])) == tmp.values[0]);
		REQUIRE(quat_get_y(quat_unaligned_load(&tmp.values[0])) == tmp.values[1]);
		REQUIRE(quat_get_z(quat_unaligned_load(&tmp.values[0])) == tmp.values[2]);
		REQUIRE(quat_get_w(quat_unaligned_load(&tmp.values[0])) == tmp.values[3]);
	}

	{
		const Vector4Type vec = vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0));
		REQUIRE(quat_get_x(vector_to_quat(vec)) == vector_get_x(vec));
		REQUIRE(quat_get_y(vector_to_quat(vec)) == vector_get_y(vec));
		REQUIRE(quat_get_z(vector_to_quat(vec)) == vector_get_z(vec));
		REQUIRE(quat_get_w(vector_to_quat(vec)) == vector_get_w(vec));
	}

	//////////////////////////////////////////////////////////////////////////
	// Arithmetic

	{
		QuatType quat = quat_from_euler(deg2rad(FloatType(30.0)), deg2rad(FloatType(-45.0)), deg2rad(FloatType(90.0)));
		QuatType quat_conj = quat_conjugate(quat);
		REQUIRE(quat_get_x(quat_conj) == -quat_get_x(quat));
		REQUIRE(quat_get_y(quat_conj) == -quat_get_y(quat));
		REQUIRE(quat_get_z(quat_conj) == -quat_get_z(quat));
		REQUIRE(quat_get_w(quat_conj) == quat_get_w(quat));
	}

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
		QuatType quat = quat_from_euler(deg2rad(FloatType(30.0)), deg2rad(FloatType(-45.0)), deg2rad(FloatType(90.0)));
		Vector4Type vec = quat_to_vector(quat);

		REQUIRE(scalar_near_equal(quat_length_squared(quat), vector_length_squared(vec), threshold));
		REQUIRE(scalar_near_equal(quat_length(quat), vector_length(vec), threshold));
		REQUIRE(scalar_near_equal(quat_length_reciprocal(quat), vector_length_reciprocal(vec), threshold));
	}

	{
		QuatType quat = quat_set(FloatType(-0.001138), FloatType(0.91623), FloatType(-1.624598), FloatType(0.715671));
		const QuatType scalar_normalize_result = scalar_normalize<QuatType, FloatType>(quat);
		const QuatType quat_normalize_result = quat_normalize(quat);
		REQUIRE(scalar_near_equal(quat_get_x(quat_normalize_result), quat_get_x(scalar_normalize_result), threshold));
		REQUIRE(scalar_near_equal(quat_get_y(quat_normalize_result), quat_get_y(scalar_normalize_result), threshold));
		REQUIRE(scalar_near_equal(quat_get_z(quat_normalize_result), quat_get_z(scalar_normalize_result), threshold));
		REQUIRE(scalar_near_equal(quat_get_w(quat_normalize_result), quat_get_w(scalar_normalize_result), threshold));
	}

	{
		QuatType quat0 = quat_from_euler(deg2rad(FloatType(30.0)), deg2rad(FloatType(-45.0)), deg2rad(FloatType(90.0)));
		QuatType quat1 = quat_from_euler(deg2rad(FloatType(45.0)), deg2rad(FloatType(60.0)), deg2rad(FloatType(120.0)));

		REQUIRE(scalar_near_equal(quat_get_x(quat_lerp(quat0, quat1, FloatType(0.33))), quat_get_x(scalar_lerp<QuatType, FloatType>(quat0, quat1, FloatType(0.33))), threshold));
		REQUIRE(scalar_near_equal(quat_get_y(quat_lerp(quat0, quat1, FloatType(0.33))), quat_get_y(scalar_lerp<QuatType, FloatType>(quat0, quat1, FloatType(0.33))), threshold));
		REQUIRE(scalar_near_equal(quat_get_z(quat_lerp(quat0, quat1, FloatType(0.33))), quat_get_z(scalar_lerp<QuatType, FloatType>(quat0, quat1, FloatType(0.33))), threshold));
		REQUIRE(scalar_near_equal(quat_get_w(quat_lerp(quat0, quat1, FloatType(0.33))), quat_get_w(scalar_lerp<QuatType, FloatType>(quat0, quat1, FloatType(0.33))), threshold));
	}

	{
		QuatType quat0 = quat_from_euler(deg2rad(FloatType(30.0)), deg2rad(FloatType(-45.0)), deg2rad(FloatType(90.0)));
		QuatType quat1 = quat_neg(quat0);

		REQUIRE(quat_get_x(quat0) == -quat_get_x(quat1));
		REQUIRE(quat_get_y(quat0) == -quat_get_y(quat1));
		REQUIRE(quat_get_z(quat0) == -quat_get_z(quat1));
		REQUIRE(quat_get_w(quat0) == -quat_get_w(quat1));
	}

	{
		QuatType quat0 = quat_set(FloatType(0.39564531008956383), FloatType(0.044254239301713752), FloatType(0.22768840967675355), FloatType(-0.88863059760894492));
		QuatType quat1 = quat_ensure_positive_w(quat0);
		QuatType quat2 = quat_ensure_positive_w(quat1);

		REQUIRE(quat_get_x(quat0) == -quat_get_x(quat1));
		REQUIRE(quat_get_y(quat0) == -quat_get_y(quat1));
		REQUIRE(quat_get_z(quat0) == -quat_get_z(quat1));
		REQUIRE(quat_get_w(quat0) == -quat_get_w(quat1));

		REQUIRE(quat_get_x(quat2) == quat_get_x(quat1));
		REQUIRE(quat_get_y(quat2) == quat_get_y(quat1));
		REQUIRE(quat_get_z(quat2) == quat_get_z(quat1));
		REQUIRE(quat_get_w(quat2) == quat_get_w(quat1));

		Vector4Type vec1 = quat_to_vector(quat1);
		QuatType quat3 = quat_from_positive_w(vec1);
		REQUIRE(quat_get_x(quat1) == quat_get_x(quat3));
		REQUIRE(quat_get_y(quat1) == quat_get_y(quat3));
		REQUIRE(quat_get_z(quat1) == quat_get_z(quat3));
		REQUIRE(scalar_near_equal(quat_get_w(quat1), quat_get_w(quat3), threshold));
	}

	//////////////////////////////////////////////////////////////////////////
	// Conversion to/from axis/angle/euler

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

	//////////////////////////////////////////////////////////////////////////
	// Comparisons and masking

	{
		const FloatType inf = std::numeric_limits<FloatType>::infinity();
		const FloatType nan = std::numeric_limits<FloatType>::quiet_NaN();
		REQUIRE(quat_is_finite(identity) == true);
		REQUIRE(quat_is_finite(quat_set(inf, inf, inf, inf)) == false);
		REQUIRE(quat_is_finite(quat_set(inf, FloatType(1.0), FloatType(1.0), FloatType(1.0))) == false);
		REQUIRE(quat_is_finite(quat_set(FloatType(1.0), FloatType(inf), FloatType(1.0), FloatType(1.0))) == false);
		REQUIRE(quat_is_finite(quat_set(FloatType(1.0), FloatType(1.0), FloatType(inf), FloatType(1.0))) == false);
		REQUIRE(quat_is_finite(quat_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(inf))) == false);
		REQUIRE(quat_is_finite(quat_set(nan, nan, nan, nan)) == false);
		REQUIRE(quat_is_finite(quat_set(nan, FloatType(1.0), FloatType(1.0), FloatType(1.0))) == false);
		REQUIRE(quat_is_finite(quat_set(FloatType(1.0), FloatType(nan), FloatType(1.0), FloatType(1.0))) == false);
		REQUIRE(quat_is_finite(quat_set(FloatType(1.0), FloatType(1.0), FloatType(nan), FloatType(1.0))) == false);
		REQUIRE(quat_is_finite(quat_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(nan))) == false);
	}

	{
		QuatType quat0 = quat_set(FloatType(0.39564531008956383), FloatType(0.044254239301713752), FloatType(0.22768840967675355), FloatType(0.88863059760894492));
		FloatType quat_len = quat_length(quat0);
		REQUIRE(scalar_near_equal(quat_len, FloatType(1.0), threshold));
		REQUIRE(quat_is_normalized(quat0) == true);

		QuatType quat1 = vector_to_quat(vector_mul(quat_to_vector(quat0), FloatType(1.1)));
		REQUIRE(quat_is_normalized(quat1) == false);
	}

	{
		REQUIRE(quat_near_equal(identity, identity, threshold) == true);
		REQUIRE(quat_near_equal(identity, quat_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(2.0)), FloatType(1.0001)) == true);
		REQUIRE(quat_near_equal(identity, quat_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(2.0)), FloatType(1.0)) == true);
		REQUIRE(quat_near_equal(identity, quat_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(2.0)), FloatType(0.9999)) == false);
	}

	{
		REQUIRE(quat_near_identity(identity, threshold) == true);
		REQUIRE(quat_near_identity(quat_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(0.9999999)), FloatType(0.001)) == true);
		REQUIRE(quat_near_identity(quat_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(0.98)), FloatType(0.001)) == false);
	}
}

TEST_CASE("quat 32 math", "[math][quat]")
{
	test_quat_impl<Quat_32, Vector4_32, float>(vector_zero_32(), quat_identity_32(), 1.0e-4f);

	const Quat_32 src = quat_set(0.39564531008956383f, 0.044254239301713752f, 0.22768840967675355f, 0.88863059760894492f);
	const Quat_64 dst = quat_cast(src);
	REQUIRE(scalar_near_equal(quat_get_x(dst), 0.39564531008956383, 1.0e-6));
	REQUIRE(scalar_near_equal(quat_get_y(dst), 0.044254239301713752, 1.0e-6));
	REQUIRE(scalar_near_equal(quat_get_z(dst), 0.22768840967675355, 1.0e-6));
	REQUIRE(scalar_near_equal(quat_get_w(dst), 0.88863059760894492, 1.0e-6));
}

TEST_CASE("quat 64 math", "[math][quat]")
{
	test_quat_impl<Quat_64, Vector4_64, double>(vector_zero_64(), quat_identity_64(), 1.0e-6);

	const Quat_64 src = quat_set(0.39564531008956383, 0.044254239301713752, 0.22768840967675355, 0.88863059760894492);
	const Quat_32 dst = quat_cast(src);
	REQUIRE(scalar_near_equal(quat_get_x(dst), 0.39564531008956383f, 1.0e-6f));
	REQUIRE(scalar_near_equal(quat_get_y(dst), 0.044254239301713752f, 1.0e-6f));
	REQUIRE(scalar_near_equal(quat_get_z(dst), 0.22768840967675355f, 1.0e-6f));
	REQUIRE(scalar_near_equal(quat_get_w(dst), 0.88863059760894492f, 1.0e-6f));
}
