#pragma once

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

#include <acl/math/vector4_32.h>
#include <acl/math/vector4_64.h>
#include <acl/math/quat_32.h>
#include <acl/math/quat_64.h>

#include <cstring>
#include <limits>

using namespace acl;

template<typename Vector4Type>
inline Vector4Type vector_unaligned_load_raw(const uint8_t* input);

template<>
inline Vector4_32 vector_unaligned_load_raw<Vector4_32>(const uint8_t* input)
{
	return vector_unaligned_load_32(input);
}

template<>
inline Vector4_64 vector_unaligned_load_raw<Vector4_64>(const uint8_t* input)
{
	return vector_unaligned_load_64(input);
}

template<typename Vector4Type>
inline Vector4Type vector_unaligned_load3_raw(const uint8_t* input);

template<>
inline Vector4_32 vector_unaligned_load3_raw<Vector4_32>(const uint8_t* input)
{
	return vector_unaligned_load3_32(input);
}

template<>
inline Vector4_64 vector_unaligned_load3_raw<Vector4_64>(const uint8_t* input)
{
	return vector_unaligned_load3_64(input);
}

template<typename Vector4Type, typename FloatType>
inline const FloatType* vector_as_float_ptr_raw(const Vector4Type& input);

template<>
inline const float* vector_as_float_ptr_raw<Vector4_32, float>(const Vector4_32& input)
{
	return vector_as_float_ptr(input);
}

template<>
inline const double* vector_as_float_ptr_raw<Vector4_64, double>(const Vector4_64& input)
{
	return vector_as_double_ptr(input);
}

template<typename Vector4Type>
inline Vector4Type scalar_cross3(const Vector4Type& lhs, const Vector4Type& rhs)
{
	return vector_set(vector_get_y(lhs) * vector_get_z(rhs) - vector_get_z(lhs) * vector_get_y(rhs),
		vector_get_z(lhs) * vector_get_x(rhs) - vector_get_x(lhs) * vector_get_z(rhs),
		vector_get_x(lhs) * vector_get_y(rhs) - vector_get_y(lhs) * vector_get_x(rhs));
}

template<typename Vector4Type, typename FloatType>
inline FloatType scalar_dot(const Vector4Type& lhs, const Vector4Type& rhs)
{
	return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs)) + (vector_get_w(lhs) * vector_get_w(rhs));
}

template<typename Vector4Type, typename FloatType>
inline FloatType scalar_dot3(const Vector4Type& lhs, const Vector4Type& rhs)
{
	return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs));
}

template<typename Vector4Type, typename FloatType>
inline Vector4Type scalar_normalize3(const Vector4Type& input, FloatType threshold)
{
	FloatType inv_len = FloatType(1.0) / acl::sqrt(scalar_dot3<Vector4Type, FloatType>(input, input));
	if (inv_len >= threshold)
		return vector_set(vector_get_x(input) * inv_len, vector_get_y(input) * inv_len, vector_get_z(input) * inv_len);
	else
		return input;
}

template<typename Vector4Type, VectorMix comp0, VectorMix comp1, VectorMix comp2, VectorMix comp3>
inline Vector4Type scalar_mix(const Vector4Type& input0, const Vector4Type& input1)
{
	const auto x = math_impl::is_vector_mix_arg_xyzw(comp0) ? vector_get_component<comp0>(input0) : vector_get_component<comp0>(input1);
	const auto y = math_impl::is_vector_mix_arg_xyzw(comp1) ? vector_get_component<comp1>(input0) : vector_get_component<comp1>(input1);
	const auto z = math_impl::is_vector_mix_arg_xyzw(comp2) ? vector_get_component<comp2>(input0) : vector_get_component<comp2>(input1);
	const auto w = math_impl::is_vector_mix_arg_xyzw(comp3) ? vector_get_component<comp3>(input0) : vector_get_component<comp3>(input1);
	return vector_set(x, y, z, w);
}

template<typename Vector4Type, typename QuatType, typename FloatType>
void test_vector4_impl(const Vector4Type& zero, const QuatType& identity, const FloatType threshold)
{
	struct alignas(16) Tmp
	{
		int32_t padding;
		alignas(8) FloatType values[4];
	};

	Tmp tmp = { 0, { FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0) } };
	alignas(16) uint8_t buffer[64];

	const FloatType test_value0_flt[4] = { FloatType(2.0), FloatType(9.34), FloatType(-54.12), FloatType(6000.0) };
	const FloatType test_value1_flt[4] = { FloatType(0.75), FloatType(-4.52), FloatType(44.68), FloatType(-54225.0) };
	const FloatType test_value2_flt[4] = { FloatType(-2.65), FloatType(2.996113), FloatType(0.68123521), FloatType(-5.9182) };
	const Vector4Type test_value0 = vector_set(test_value0_flt[0], test_value0_flt[1], test_value0_flt[2], test_value0_flt[3]);
	const Vector4Type test_value1 = vector_set(test_value1_flt[0], test_value1_flt[1], test_value1_flt[2], test_value1_flt[3]);
	const Vector4Type test_value2 = vector_set(test_value2_flt[0], test_value2_flt[1], test_value2_flt[2], test_value2_flt[3]);

	//////////////////////////////////////////////////////////////////////////
	// Setters, getters, and casts

	REQUIRE(vector_get_x(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(0.0));
	REQUIRE(vector_get_y(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(2.34));
	REQUIRE(vector_get_z(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(-3.12));
	REQUIRE(vector_get_w(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(10000.0));

	REQUIRE(vector_get_x(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12))) == FloatType(0.0));
	REQUIRE(vector_get_y(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12))) == FloatType(2.34));
	REQUIRE(vector_get_z(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12))) == FloatType(-3.12));

	REQUIRE(vector_get_x(vector_set(FloatType(-3.12))) == FloatType(-3.12));
	REQUIRE(vector_get_y(vector_set(FloatType(-3.12))) == FloatType(-3.12));
	REQUIRE(vector_get_z(vector_set(FloatType(-3.12))) == FloatType(-3.12));
	REQUIRE(vector_get_w(vector_set(FloatType(-3.12))) == FloatType(-3.12));

	REQUIRE(vector_get_x(zero) == FloatType(0.0));
	REQUIRE(vector_get_y(zero) == FloatType(0.0));
	REQUIRE(vector_get_z(zero) == FloatType(0.0));
	REQUIRE(vector_get_w(zero) == FloatType(0.0));

	REQUIRE(vector_get_x(vector_unaligned_load(&tmp.values[0])) == tmp.values[0]);
	REQUIRE(vector_get_y(vector_unaligned_load(&tmp.values[0])) == tmp.values[1]);
	REQUIRE(vector_get_z(vector_unaligned_load(&tmp.values[0])) == tmp.values[2]);
	REQUIRE(vector_get_w(vector_unaligned_load(&tmp.values[0])) == tmp.values[3]);

	REQUIRE(vector_get_x(vector_unaligned_load3(&tmp.values[0])) == tmp.values[0]);
	REQUIRE(vector_get_y(vector_unaligned_load3(&tmp.values[0])) == tmp.values[1]);
	REQUIRE(vector_get_z(vector_unaligned_load3(&tmp.values[0])) == tmp.values[2]);

	std::memcpy(&buffer[1], &tmp.values[0], sizeof(tmp.values));
	REQUIRE(vector_get_x(vector_unaligned_load_raw<Vector4Type>(&buffer[1])) == tmp.values[0]);
	REQUIRE(vector_get_y(vector_unaligned_load_raw<Vector4Type>(&buffer[1])) == tmp.values[1]);
	REQUIRE(vector_get_z(vector_unaligned_load_raw<Vector4Type>(&buffer[1])) == tmp.values[2]);
	REQUIRE(vector_get_w(vector_unaligned_load_raw<Vector4Type>(&buffer[1])) == tmp.values[3]);

	REQUIRE(vector_get_x(vector_unaligned_load3_raw<Vector4Type>(&buffer[1])) == tmp.values[0]);
	REQUIRE(vector_get_y(vector_unaligned_load3_raw<Vector4Type>(&buffer[1])) == tmp.values[1]);
	REQUIRE(vector_get_z(vector_unaligned_load3_raw<Vector4Type>(&buffer[1])) == tmp.values[2]);

	REQUIRE(vector_get_x(quat_to_vector(identity)) == quat_get_x(identity));
	REQUIRE(vector_get_y(quat_to_vector(identity)) == quat_get_y(identity));
	REQUIRE(vector_get_z(quat_to_vector(identity)) == quat_get_z(identity));
	REQUIRE(vector_get_w(quat_to_vector(identity)) == quat_get_w(identity));

	REQUIRE(vector_get_component<VectorMix::X>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(0.0));
	REQUIRE(vector_get_component<VectorMix::Y>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(2.34));
	REQUIRE(vector_get_component<VectorMix::Z>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(-3.12));
	REQUIRE(vector_get_component<VectorMix::W>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(10000.0));

	REQUIRE(vector_get_component<VectorMix::A>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(0.0));
	REQUIRE(vector_get_component<VectorMix::B>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(2.34));
	REQUIRE(vector_get_component<VectorMix::C>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(-3.12));
	REQUIRE(vector_get_component<VectorMix::D>(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0))) == FloatType(10000.0));

	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::X) == FloatType(0.0));
	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::Y) == FloatType(2.34));
	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::Z) == FloatType(-3.12));
	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::W) == FloatType(10000.0));

	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::A) == FloatType(0.0));
	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::B) == FloatType(2.34));
	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::C) == FloatType(-3.12));
	REQUIRE(vector_get_component(vector_set(FloatType(0.0), FloatType(2.34), FloatType(-3.12), FloatType(10000.0)), VectorMix::D) == FloatType(10000.0));

	REQUIRE((vector_as_float_ptr_raw<Vector4Type, FloatType>(vector_unaligned_load(&tmp.values[0]))[0] == tmp.values[0]));
	REQUIRE((vector_as_float_ptr_raw<Vector4Type, FloatType>(vector_unaligned_load(&tmp.values[0]))[1] == tmp.values[1]));
	REQUIRE((vector_as_float_ptr_raw<Vector4Type, FloatType>(vector_unaligned_load(&tmp.values[0]))[2] == tmp.values[2]));
	REQUIRE((vector_as_float_ptr_raw<Vector4Type, FloatType>(vector_unaligned_load(&tmp.values[0]))[3] == tmp.values[3]));

	vector_unaligned_write(test_value0, &tmp.values[0]);
	REQUIRE(vector_get_x(test_value0) == tmp.values[0]);
	REQUIRE(vector_get_y(test_value0) == tmp.values[1]);
	REQUIRE(vector_get_z(test_value0) == tmp.values[2]);
	REQUIRE(vector_get_w(test_value0) == tmp.values[3]);

	vector_unaligned_write3(test_value1, &tmp.values[0]);
	REQUIRE(vector_get_x(test_value1) == tmp.values[0]);
	REQUIRE(vector_get_y(test_value1) == tmp.values[1]);
	REQUIRE(vector_get_z(test_value1) == tmp.values[2]);
	REQUIRE(vector_get_w(test_value0) == tmp.values[3]);

	vector_unaligned_write3(test_value1, &buffer[1]);
	REQUIRE(vector_get_x(test_value1) == vector_get_x(vector_unaligned_load3_raw<Vector4Type>(&buffer[1])));
	REQUIRE(vector_get_y(test_value1) == vector_get_y(vector_unaligned_load3_raw<Vector4Type>(&buffer[1])));
	REQUIRE(vector_get_z(test_value1) == vector_get_z(vector_unaligned_load3_raw<Vector4Type>(&buffer[1])));

	//////////////////////////////////////////////////////////////////////////
	// Arithmetic

	REQUIRE(scalar_near_equal(vector_get_x(vector_add(test_value0, test_value1)), test_value0_flt[0] + test_value1_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_add(test_value0, test_value1)), test_value0_flt[1] + test_value1_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_add(test_value0, test_value1)), test_value0_flt[2] + test_value1_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_add(test_value0, test_value1)), test_value0_flt[3] + test_value1_flt[3], threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_sub(test_value0, test_value1)), test_value0_flt[0] - test_value1_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_sub(test_value0, test_value1)), test_value0_flt[1] - test_value1_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_sub(test_value0, test_value1)), test_value0_flt[2] - test_value1_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_sub(test_value0, test_value1)), test_value0_flt[3] - test_value1_flt[3], threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_mul(test_value0, test_value1)), test_value0_flt[0] * test_value1_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_mul(test_value0, test_value1)), test_value0_flt[1] * test_value1_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_mul(test_value0, test_value1)), test_value0_flt[2] * test_value1_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_mul(test_value0, test_value1)), test_value0_flt[3] * test_value1_flt[3], threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_mul(test_value0, FloatType(2.34))), test_value0_flt[0] * FloatType(2.34), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_mul(test_value0, FloatType(2.34))), test_value0_flt[1] * FloatType(2.34), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_mul(test_value0, FloatType(2.34))), test_value0_flt[2] * FloatType(2.34), threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_mul(test_value0, FloatType(2.34))), test_value0_flt[3] * FloatType(2.34), threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_div(test_value0, test_value1)), test_value0_flt[0] / test_value1_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_div(test_value0, test_value1)), test_value0_flt[1] / test_value1_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_div(test_value0, test_value1)), test_value0_flt[2] / test_value1_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_div(test_value0, test_value1)), test_value0_flt[3] / test_value1_flt[3], threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_max(test_value0, test_value1)), acl::max(test_value0_flt[0], test_value1_flt[0]), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_max(test_value0, test_value1)), acl::max(test_value0_flt[1], test_value1_flt[1]), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_max(test_value0, test_value1)), acl::max(test_value0_flt[2], test_value1_flt[2]), threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_max(test_value0, test_value1)), acl::max(test_value0_flt[3], test_value1_flt[3]), threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_min(test_value0, test_value1)), acl::min(test_value0_flt[0], test_value1_flt[0]), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_min(test_value0, test_value1)), acl::min(test_value0_flt[1], test_value1_flt[1]), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_min(test_value0, test_value1)), acl::min(test_value0_flt[2], test_value1_flt[2]), threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_min(test_value0, test_value1)), acl::min(test_value0_flt[3], test_value1_flt[3]), threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_abs(test_value0)), acl::abs(test_value0_flt[0]), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_abs(test_value0)), acl::abs(test_value0_flt[1]), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_abs(test_value0)), acl::abs(test_value0_flt[2]), threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_abs(test_value0)), acl::abs(test_value0_flt[3]), threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_neg(test_value0)), -test_value0_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_neg(test_value0)), -test_value0_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_neg(test_value0)), -test_value0_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_neg(test_value0)), -test_value0_flt[3], threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_reciprocal(test_value0)), acl::reciprocal(test_value0_flt[0]), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_reciprocal(test_value0)), acl::reciprocal(test_value0_flt[1]), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_reciprocal(test_value0)), acl::reciprocal(test_value0_flt[2]), threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_reciprocal(test_value0)), acl::reciprocal(test_value0_flt[3]), threshold));

	const Vector4Type scalar_cross3_result = scalar_cross3<Vector4Type>(test_value0, test_value1);
	const Vector4Type vector_cross3_result = vector_cross3(test_value0, test_value1);
	REQUIRE(scalar_near_equal(vector_get_x(vector_cross3_result), vector_get_x(scalar_cross3_result), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_cross3_result), vector_get_y(scalar_cross3_result), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_cross3_result), vector_get_z(scalar_cross3_result), threshold));

	const FloatType test_value10_flt[4] = { FloatType(-0.001138), FloatType(0.91623), FloatType(-1.624598), FloatType(0.715671) };
	const FloatType test_value11_flt[4] = { FloatType(0.1138), FloatType(-0.623), FloatType(1.4598), FloatType(-0.5671) };
	const Vector4Type test_value10 = vector_set(test_value10_flt[0], test_value10_flt[1], test_value10_flt[2], test_value10_flt[3]);
	const Vector4Type test_value11 = vector_set(test_value11_flt[0], test_value11_flt[1], test_value11_flt[2], test_value11_flt[3]);
	const FloatType scalar_dot_result = scalar_dot<Vector4Type, FloatType>(test_value10, test_value11);
	const FloatType vector_dot_result = vector_dot(test_value10, test_value11);
	REQUIRE(scalar_near_equal(vector_dot_result, scalar_dot_result, threshold));

	const FloatType scalar_dot3_result = scalar_dot3<Vector4Type, FloatType>(test_value10, test_value11);
	const FloatType vector_dot3_result = vector_dot3(test_value10, test_value11);
	REQUIRE(scalar_near_equal(vector_dot3_result, scalar_dot3_result, threshold));

	REQUIRE(scalar_near_equal(scalar_dot<Vector4Type, FloatType>(test_value0, test_value0), vector_length_squared(test_value0), threshold));
	REQUIRE(scalar_near_equal(scalar_dot3<Vector4Type, FloatType>(test_value0, test_value0), vector_length_squared3(test_value0), threshold));

	REQUIRE(scalar_near_equal(acl::sqrt(scalar_dot<Vector4Type, FloatType>(test_value0, test_value0)), vector_length(test_value0), threshold));
	REQUIRE(scalar_near_equal(acl::sqrt(scalar_dot3<Vector4Type, FloatType>(test_value0, test_value0)), vector_length3(test_value0), threshold));

	REQUIRE(scalar_near_equal(acl::sqrt_reciprocal(scalar_dot<Vector4Type, FloatType>(test_value0, test_value0)), vector_length_reciprocal(test_value0), threshold));
	REQUIRE(scalar_near_equal(acl::sqrt_reciprocal(scalar_dot3<Vector4Type, FloatType>(test_value0, test_value0)), vector_length_reciprocal3(test_value0), threshold));

	const Vector4Type test_value_diff = vector_sub(test_value0, test_value1);
	REQUIRE(scalar_near_equal(acl::sqrt(scalar_dot3<Vector4Type, FloatType>(test_value_diff, test_value_diff)), vector_distance3(test_value0, test_value1), threshold));

	const Vector4Type scalar_normalize3_result = scalar_normalize3<Vector4Type, FloatType>(test_value0, threshold);
	const Vector4Type vector_normalize3_result = vector_normalize3(test_value0, threshold);
	REQUIRE(scalar_near_equal(vector_get_x(vector_normalize3_result), vector_get_x(scalar_normalize3_result), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_normalize3_result), vector_get_y(scalar_normalize3_result), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_normalize3_result), vector_get_z(scalar_normalize3_result), threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_lerp(test_value10, test_value11, FloatType(0.33))), ((test_value11_flt[0] - test_value10_flt[0]) * FloatType(0.33)) + test_value10_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_lerp(test_value10, test_value11, FloatType(0.33))), ((test_value11_flt[1] - test_value10_flt[1]) * FloatType(0.33)) + test_value10_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_lerp(test_value10, test_value11, FloatType(0.33))), ((test_value11_flt[2] - test_value10_flt[2]) * FloatType(0.33)) + test_value10_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_lerp(test_value10, test_value11, FloatType(0.33))), ((test_value11_flt[3] - test_value10_flt[3]) * FloatType(0.33)) + test_value10_flt[3], threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_fraction(test_value0)), acl::fraction(test_value0_flt[0]), threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_fraction(test_value0)), acl::fraction(test_value0_flt[1]), threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_fraction(test_value0)), acl::fraction(test_value0_flt[2]), threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_fraction(test_value0)), acl::fraction(test_value0_flt[3]), threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_mul_add(test_value10, test_value11, test_value2)), (test_value10_flt[0] * test_value11_flt[0]) + test_value2_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_mul_add(test_value10, test_value11, test_value2)), (test_value10_flt[1] * test_value11_flt[1]) + test_value2_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_mul_add(test_value10, test_value11, test_value2)), (test_value10_flt[2] * test_value11_flt[2]) + test_value2_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_mul_add(test_value10, test_value11, test_value2)), (test_value10_flt[3] * test_value11_flt[3]) + test_value2_flt[3], threshold));

	REQUIRE(scalar_near_equal(vector_get_x(vector_neg_mul_sub(test_value10, test_value11, test_value2)), (test_value10_flt[0] * -test_value11_flt[0]) + test_value2_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_neg_mul_sub(test_value10, test_value11, test_value2)), (test_value10_flt[1] * -test_value11_flt[1]) + test_value2_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_neg_mul_sub(test_value10, test_value11, test_value2)), (test_value10_flt[2] * -test_value11_flt[2]) + test_value2_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_neg_mul_sub(test_value10, test_value11, test_value2)), (test_value10_flt[3] * -test_value11_flt[3]) + test_value2_flt[3], threshold));

	//////////////////////////////////////////////////////////////////////////
	// Comparisons and masking

	REQUIRE((vector_get_x(vector_less_than(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[0] < test_value1_flt[0]));
	REQUIRE((vector_get_y(vector_less_than(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[1] < test_value1_flt[1]));
	REQUIRE((vector_get_z(vector_less_than(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[2] < test_value1_flt[2]));
	REQUIRE((vector_get_w(vector_less_than(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[3] < test_value1_flt[3]));

	REQUIRE((vector_get_x(vector_greater_equal(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[0] >= test_value1_flt[0]));
	REQUIRE((vector_get_y(vector_greater_equal(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[1] >= test_value1_flt[1]));
	REQUIRE((vector_get_z(vector_greater_equal(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[2] >= test_value1_flt[2]));
	REQUIRE((vector_get_w(vector_greater_equal(test_value0, test_value1)) != FloatType(0.0)) == (test_value0_flt[3] >= test_value1_flt[3]));

	REQUIRE(vector_all_less_than(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0))) == true);
	REQUIRE(vector_all_less_than(zero, vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_than(zero, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_than(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_than(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(1.0))) == false);
	REQUIRE(vector_all_less_than(zero, zero) == false);

	REQUIRE(vector_all_less_than3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_than3(zero, vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_than3(zero, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_than3(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_than3(zero, zero) == false);

	REQUIRE(vector_any_less_than(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0))) == true);
	REQUIRE(vector_any_less_than(zero, vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_than(zero, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_than(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_than(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(1.0))) == true);
	REQUIRE(vector_any_less_than(zero, zero) == false);

	REQUIRE(vector_any_less_than3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_than3(zero, vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_than3(zero, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_than3(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_than3(zero, zero) == false);

	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0))) == true);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(1.0))) == true);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(-1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(0.0), FloatType(-1.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(-1.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_equal(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(0.0), FloatType(-1.0))) == false);
	REQUIRE(vector_all_less_equal(zero, zero) == true);

	REQUIRE(vector_all_less_equal3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_equal3(zero, vector_set(FloatType(1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_equal3(zero, vector_set(FloatType(0.0), FloatType(1.0), FloatType(0.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_equal3(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_all_less_equal3(zero, vector_set(FloatType(-1.0), FloatType(0.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_equal3(zero, vector_set(FloatType(0.0), FloatType(-1.0), FloatType(0.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_equal3(zero, vector_set(FloatType(0.0), FloatType(0.0), FloatType(-1.0), FloatType(0.0))) == false);
	REQUIRE(vector_all_less_equal3(zero, zero) == true);

	REQUIRE(vector_any_less_equal(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0))) == true);
	REQUIRE(vector_any_less_equal(zero, vector_set(FloatType(1.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0))) == true);
	REQUIRE(vector_any_less_equal(zero, vector_set(FloatType(-1.0), FloatType(1.0), FloatType(-1.0), FloatType(-1.0))) == true);
	REQUIRE(vector_any_less_equal(zero, vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(1.0), FloatType(-1.0))) == true);
	REQUIRE(vector_any_less_equal(zero, vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(1.0))) == true);
	REQUIRE(vector_any_less_equal(zero, vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0))) == false);
	REQUIRE(vector_any_less_equal(zero, zero) == true);

	REQUIRE(vector_any_less_equal3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_equal3(zero, vector_set(FloatType(1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_equal3(zero, vector_set(FloatType(-1.0), FloatType(1.0), FloatType(-1.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_equal3(zero, vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(1.0), FloatType(0.0))) == true);
	REQUIRE(vector_any_less_equal3(zero, vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0))) == false);
	REQUIRE(vector_any_less_equal3(zero, zero) == true);

	REQUIRE(vector_all_greater_equal(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0)), zero) == true);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(1.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(-1.0), FloatType(1.0), FloatType(-1.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(1.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(0.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(-1.0), FloatType(0.0), FloatType(-1.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(0.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_all_greater_equal(zero, zero) == true);

	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(-1.0), FloatType(1.0), FloatType(-1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(0.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(-1.0), FloatType(0.0), FloatType(-1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(0.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal3(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_all_greater_equal3(zero, zero) == true);

	REQUIRE(vector_any_greater_equal(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(1.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(-1.0), FloatType(1.0), FloatType(-1.0), FloatType(-1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(1.0), FloatType(-1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(0.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(-1.0), FloatType(0.0), FloatType(-1.0), FloatType(-1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(0.0), FloatType(-1.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(-1.0)), zero) == false);
	REQUIRE(vector_any_greater_equal(zero, zero) == true);

	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(-1.0), FloatType(1.0), FloatType(-1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(0.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(-1.0), FloatType(0.0), FloatType(-1.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(0.0), FloatType(0.0)), zero) == true);
	REQUIRE(vector_any_greater_equal3(vector_set(FloatType(-1.0), FloatType(-1.0), FloatType(-1.0), FloatType(0.0)), zero) == false);
	REQUIRE(vector_any_greater_equal3(zero, zero) == true);

	REQUIRE(vector_all_near_equal(zero, zero, threshold) == true);
	REQUIRE(vector_all_near_equal(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_all_near_equal(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0)), FloatType(1.0)) == true);
	REQUIRE(vector_all_near_equal(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0)), FloatType(0.9999)) == false);

	REQUIRE(vector_all_near_equal3(zero, zero, threshold) == true);
	REQUIRE(vector_all_near_equal3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(2.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_all_near_equal3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(2.0)), FloatType(1.0)) == true);
	REQUIRE(vector_all_near_equal3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(2.0)), FloatType(0.9999)) == false);

	REQUIRE(vector_any_near_equal(zero, zero, threshold) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(1.0), FloatType(2.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(2.0), FloatType(1.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(2.0), FloatType(2.0), FloatType(1.0), FloatType(2.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(2.0), FloatType(2.0), FloatType(2.0), FloatType(1.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(1.0), FloatType(2.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(2.0), FloatType(1.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(2.0), FloatType(2.0), FloatType(1.0), FloatType(2.0)), FloatType(1.0)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(2.0), FloatType(2.0), FloatType(2.0), FloatType(1.0)), FloatType(1.0)) == true);
	REQUIRE(vector_any_near_equal(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(1.0)), FloatType(0.9999)) == false);

	REQUIRE(vector_any_near_equal3(zero, zero, threshold) == true);
	REQUIRE(vector_any_near_equal3(zero, vector_set(FloatType(1.0), FloatType(2.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_any_near_equal3(zero, vector_set(FloatType(2.0), FloatType(1.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_any_near_equal3(zero, vector_set(FloatType(2.0), FloatType(2.0), FloatType(1.0), FloatType(2.0)), FloatType(1.0001)) == true);
	REQUIRE(vector_any_near_equal3(zero, vector_set(FloatType(1.0), FloatType(2.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0)) == true);
	REQUIRE(vector_any_near_equal3(zero, vector_set(FloatType(2.0), FloatType(1.0), FloatType(2.0), FloatType(2.0)), FloatType(1.0)) == true);
	REQUIRE(vector_any_near_equal3(zero, vector_set(FloatType(2.0), FloatType(2.0), FloatType(1.0), FloatType(2.0)), FloatType(1.0)) == true);
	REQUIRE(vector_any_near_equal3(zero, vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(2.0)), FloatType(0.9999)) == false);

	const FloatType inf = std::numeric_limits<FloatType>::infinity();
	const FloatType nan = std::numeric_limits<FloatType>::quiet_NaN();
	REQUIRE(vector_is_finite(zero) == true);
	REQUIRE(vector_is_finite(vector_set(inf, inf, inf, inf)) == false);
	REQUIRE(vector_is_finite(vector_set(inf, FloatType(1.0), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite(vector_set(FloatType(1.0), FloatType(inf), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite(vector_set(FloatType(1.0), FloatType(1.0), FloatType(inf), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(inf))) == false);
	REQUIRE(vector_is_finite(vector_set(nan, nan, nan, nan)) == false);
	REQUIRE(vector_is_finite(vector_set(nan, FloatType(1.0), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite(vector_set(FloatType(1.0), FloatType(nan), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite(vector_set(FloatType(1.0), FloatType(1.0), FloatType(nan), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(nan))) == false);

	REQUIRE(vector_is_finite3(zero) == true);
	REQUIRE(vector_is_finite3(vector_set(inf, inf, inf, inf)) == false);
	REQUIRE(vector_is_finite3(vector_set(inf, FloatType(1.0), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite3(vector_set(FloatType(1.0), FloatType(inf), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite3(vector_set(FloatType(1.0), FloatType(1.0), FloatType(inf), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite3(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(inf))) == true);
	REQUIRE(vector_is_finite3(vector_set(nan, nan, nan, nan)) == false);
	REQUIRE(vector_is_finite3(vector_set(nan, FloatType(1.0), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite3(vector_set(FloatType(1.0), FloatType(nan), FloatType(1.0), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite3(vector_set(FloatType(1.0), FloatType(1.0), FloatType(nan), FloatType(1.0))) == false);
	REQUIRE(vector_is_finite3(vector_set(FloatType(1.0), FloatType(1.0), FloatType(1.0), FloatType(nan))) == true);

	//////////////////////////////////////////////////////////////////////////
	// Swizzling, permutations, and mixing

	REQUIRE(scalar_near_equal(vector_get_x(vector_blend(vector_less_than(zero, vector_set(FloatType(1.0))), test_value0, test_value1)), test_value0_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_blend(vector_less_than(zero, vector_set(FloatType(1.0))), test_value0, test_value1)), test_value0_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_blend(vector_less_than(zero, vector_set(FloatType(1.0))), test_value0, test_value1)), test_value0_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_blend(vector_less_than(zero, vector_set(FloatType(1.0))), test_value0, test_value1)), test_value0_flt[3], threshold));
	REQUIRE(scalar_near_equal(vector_get_x(vector_blend(vector_less_than(vector_set(FloatType(1.0)), zero), test_value0, test_value1)), test_value1_flt[0], threshold));
	REQUIRE(scalar_near_equal(vector_get_y(vector_blend(vector_less_than(vector_set(FloatType(1.0)), zero), test_value0, test_value1)), test_value1_flt[1], threshold));
	REQUIRE(scalar_near_equal(vector_get_z(vector_blend(vector_less_than(vector_set(FloatType(1.0)), zero), test_value0, test_value1)), test_value1_flt[2], threshold));
	REQUIRE(scalar_near_equal(vector_get_w(vector_blend(vector_less_than(vector_set(FloatType(1.0)), zero), test_value0, test_value1)), test_value1_flt[3], threshold));

#define ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, comp2) \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::X] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::X>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::X>(input0, input1), threshold); \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::Y] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::Y>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::Y>(input0, input1), threshold); \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::Z] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::Z>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::Z>(input0, input1), threshold); \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::W] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::W>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::W>(input0, input1), threshold); \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::A] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::A>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::A>(input0, input1), threshold); \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::B] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::B>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::B>(input0, input1), threshold); \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::C] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::C>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::C>(input0, input1), threshold); \
	results[(int)comp0][(int)comp1][(int)comp2][(int)VectorMix::D] = vector_all_near_equal(vector_mix<comp0, comp1, comp2, VectorMix::D>(input0, input1), scalar_mix<Vector4Type, comp0, comp1, comp2, VectorMix::D>(input0, input1), threshold)

#define ACL_TEST_MIX_XY(results, input0, input1, comp0, comp1) \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::X); \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::Y); \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::Z); \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::W); \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::A); \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::B); \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::C); \
	ACL_TEST_MIX_XYZ(results, input0, input1, comp0, comp1, VectorMix::D)

#define ACL_TEST_MIX_X(results, input0, input1, comp0) \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::X); \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::Y); \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::Z); \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::W); \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::A); \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::B); \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::C); \
	ACL_TEST_MIX_XY(results, input0, input1, comp0, VectorMix::D)

#define ACL_TEST_MIX(results, input0, input1) \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::X); \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::Y); \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::Z); \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::W); \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::A); \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::B); \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::C); \
	ACL_TEST_MIX_X(results, input0, input1, VectorMix::D)

	// This generates 8*8*8*8 = 4096 unit tests... it takes a while to compile and uses a lot of stack space
	// Disabled by default to reduce the build time
	bool vector_mix_results[8][8][8][8];
	ACL_TEST_MIX(vector_mix_results, test_value0, test_value1);

	for (int comp0 = 0; comp0 < 8; ++comp0)
	{
		for (int comp1 = 0; comp1 < 8; ++comp1)
		{
			for (int comp2 = 0; comp2 < 8; ++comp2)
			{
				for (int comp3 = 0; comp3 < 8; ++comp3)
				{
					INFO("vector_mix<" << comp0 << ", " << comp1 << ", " << comp2 << ", " << comp3 << ">");
					REQUIRE(vector_mix_results[comp0][comp1][comp2][comp3] == true);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Misc

	auto scalar_sign = [](FloatType value) { return value >= FloatType(0.0) ? FloatType(1.0) : FloatType(-1.0); };
	REQUIRE(vector_get_x(vector_sign(test_value0)) == scalar_sign(test_value0_flt[0]));
	REQUIRE(vector_get_y(vector_sign(test_value0)) == scalar_sign(test_value0_flt[1]));
	REQUIRE(vector_get_z(vector_sign(test_value0)) == scalar_sign(test_value0_flt[2]));
	REQUIRE(vector_get_w(vector_sign(test_value0)) == scalar_sign(test_value0_flt[3]));
}
