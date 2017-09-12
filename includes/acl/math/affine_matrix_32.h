#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/error.h"
#include "acl/math/math.h"
#include "acl/math/vector4_32.h"
#include "acl/math/quat_32.h"

namespace acl
{
	AffineMatrix_32 matrix_set(const Vector4_32& x_axis, const Vector4_32& y_axis, const Vector4_32& z_axis, const Vector4_32& w_axis)
	{
		ACL_ENSURE(vector_get_w(x_axis) == 0.0f, "X axis does not have a W component == 0.0");
		ACL_ENSURE(vector_get_w(y_axis) == 0.0f, "Y axis does not have a W component == 0.0");
		ACL_ENSURE(vector_get_w(z_axis) == 0.0f, "Z axis does not have a W component == 0.0");
		ACL_ENSURE(vector_get_w(w_axis) == 1.0f, "W axis does not have a W component == 1.0");
		return AffineMatrix_32{x_axis, y_axis, z_axis, w_axis};
	}

	AffineMatrix_32 matrix_set(const Quat_32& quat, const Vector4_32& translation, const Vector4_32& scale)
	{
		ACL_ENSURE(quat_is_normalized(quat), "Quaternion is not normalized");

		const float x2 = quat_get_x(quat) + quat_get_x(quat);
		const float y2 = quat_get_y(quat) + quat_get_y(quat);
		const float z2 = quat_get_z(quat) + quat_get_z(quat);
		const float xx = quat_get_x(quat) * x2;
		const float xy = quat_get_x(quat) * y2;
		const float xz = quat_get_x(quat) * z2;
		const float yy = quat_get_y(quat) * y2;
		const float yz = quat_get_y(quat) * z2;
		const float zz = quat_get_z(quat) * z2;
		const float wx = quat_get_w(quat) * x2;
		const float wy = quat_get_w(quat) * y2;
		const float wz = quat_get_w(quat) * z2;

		Vector4_32 x_axis = vector_mul(vector_set(1.0f - (yy + zz), xy + wz, xz - wy, 0.0f), vector_get_x(scale));
		Vector4_32 y_axis = vector_mul(vector_set(xy - wz, 1.0f - (xx + zz), yz + wx, 0.0f), vector_get_y(scale));
		Vector4_32 z_axis = vector_mul(vector_set(xz + wy, yz - wx, 1.0f - (xx + yy), 0.0f), vector_get_z(scale));
		Vector4_32 w_axis = vector_set(vector_get_x(translation), vector_get_y(translation), vector_get_z(translation), 1.0f);
		return matrix_set(x_axis, y_axis, z_axis, w_axis);
	}

	inline AffineMatrix_32 matrix_identity_32()
	{
		return matrix_set(vector_set(1.0f, 0.0f, 0.0f, 0.0f), vector_set(0.0f, 1.0f, 0.0f, 0.0f), vector_set(0.0f, 0.0f, 1.0f, 0.0f), vector_set(0.0f, 0.0f, 0.0f, 1.0f));
	}

	inline AffineMatrix_32 matrix_from_quat(const Quat_32& quat)
	{
		ACL_ENSURE(quat_is_normalized(quat), "Quaternion is not normalized");

		const float x2 = quat_get_x(quat) + quat_get_x(quat);
		const float y2 = quat_get_y(quat) + quat_get_y(quat);
		const float z2 = quat_get_z(quat) + quat_get_z(quat);
		const float xx = quat_get_x(quat) * x2;
		const float xy = quat_get_x(quat) * y2;
		const float xz = quat_get_x(quat) * z2;
		const float yy = quat_get_y(quat) * y2;
		const float yz = quat_get_y(quat) * z2;
		const float zz = quat_get_z(quat) * z2;
		const float wx = quat_get_w(quat) * x2;
		const float wy = quat_get_w(quat) * y2;
		const float wz = quat_get_w(quat) * z2;

		Vector4_32 x_axis = vector_set(1.0f - (yy + zz), xy + wz, xz - wy, 0.0f);
		Vector4_32 y_axis = vector_set(xy - wz, 1.0f - (xx + zz), yz + wx, 0.0f);
		Vector4_32 z_axis = vector_set(xz + wy, yz - wx, 1.0f - (xx + yy), 0.0f);
		Vector4_32 w_axis = vector_set(0.0f, 0.0f, 0.0f, 1.0f);
		return matrix_set(x_axis, y_axis, z_axis, w_axis);
	}

	inline AffineMatrix_32 matrix_from_translation(const Vector4_32& translation)
	{
		return matrix_set(vector_set(1.0f, 0.0f, 0.0f, 0.0f), vector_set(0.0f, 1.0f, 0.0f, 0.0f), vector_set(0.0f, 0.0f, 1.0f, 0.0f), vector_set(vector_get_x(translation), vector_get_y(translation), vector_get_z(translation), 1.0f));
	}

	inline AffineMatrix_32 matrix_from_scale(const Vector4_32& scale)
	{
		ACL_ENSURE(vector_any_near_equal3(scale, vector_zero_32()), "Scale cannot be zero");
		return matrix_set(vector_set(vector_get_x(scale), 0.0f, 0.0f, 0.0f), vector_set(0.0f, vector_get_y(scale), 0.0f, 0.0f), vector_set(0.0f, 0.0f, vector_get_z(scale), 0.0f), vector_set(0.0f, 0.0f, 0.0f, 1.0f));
	}

	// Multiplication order is as follow: local_to_world = matrix_mul(local_to_object, object_to_world)
	inline AffineMatrix_32 matrix_mul(const AffineMatrix_32& lhs, const AffineMatrix_32& rhs)
	{
		// Affine matrices have their last column always equal to [0, 0, 0, 1]
		Vector4_32 tmp = vector_mul(vector_mix_xxxx(lhs.x_axis), rhs.x_axis);
		tmp = vector_mul_add(vector_mix_yyyy(lhs.x_axis), rhs.y_axis, tmp);
		tmp = vector_mul_add(vector_mix_zzzz(lhs.x_axis), rhs.z_axis, tmp);
		Vector4_32 x_axis = tmp;

		tmp = vector_mul(vector_mix_xxxx(lhs.y_axis), rhs.x_axis);
		tmp = vector_mul_add(vector_mix_yyyy(lhs.y_axis), rhs.y_axis, tmp);
		tmp = vector_mul_add(vector_mix_zzzz(lhs.y_axis), rhs.z_axis, tmp);
		Vector4_32 y_axis = tmp;

		tmp = vector_mul(vector_mix_xxxx(lhs.z_axis), rhs.x_axis);
		tmp = vector_mul_add(vector_mix_yyyy(lhs.z_axis), rhs.y_axis, tmp);
		tmp = vector_mul_add(vector_mix_zzzz(lhs.z_axis), rhs.z_axis, tmp);
		Vector4_32 z_axis = tmp;

		tmp = vector_mul(vector_mix_xxxx(lhs.w_axis), rhs.x_axis);
		tmp = vector_mul_add(vector_mix_yyyy(lhs.w_axis), rhs.y_axis, tmp);
		tmp = vector_mul_add(vector_mix_zzzz(lhs.w_axis), rhs.z_axis, tmp);
		Vector4_32 w_axis = vector_add(rhs.w_axis, tmp);
		return matrix_set(x_axis, y_axis, z_axis, w_axis);
	}

	inline Vector4_32 matrix_mul_position(const AffineMatrix_32& lhs, const Vector4_32& rhs)
	{
		// Affine matrices have their last column always equal to [0, 0, 0, 1]
		Vector4_32 tmp0;
		Vector4_32 tmp1;

		tmp0 = vector_mul(vector_mix_xxxx(rhs), lhs.x_axis);
		tmp0 = vector_mul_add(vector_mix_yyyy(rhs), lhs.y_axis, tmp0);
		tmp1 = vector_mul_add(vector_mix_zzzz(rhs), lhs.z_axis, lhs.w_axis);

		return vector_add(tmp0, tmp1);
	}

	inline AffineMatrix_32 matrix_transpose(const AffineMatrix_32& input)
	{
		// TODO: Add unit tests for this!
		Vector4_32 tmp0 = vector_mix_xyab(input.x_axis, input.y_axis);
		Vector4_32 tmp1 = vector_mix_zwcd(input.x_axis, input.y_axis);
		Vector4_32 tmp2 = vector_mix_xyab(input.z_axis, input.w_axis);
		Vector4_32 tmp3 = vector_mix_zwcd(input.z_axis, input.w_axis);

		Vector4_32 x_axis = vector_mix_xzac(tmp0, tmp2);
		Vector4_32 y_axis = vector_mix_ywbd(tmp0, tmp2);
		Vector4_32 z_axis = vector_mix_xzac(tmp1, tmp3);
		Vector4_32 w_axis = vector_mix_ywbd(tmp1, tmp3);
		return matrix_set(x_axis, y_axis, z_axis, w_axis);
	}

	inline AffineMatrix_32 matrix_inverse(const AffineMatrix_32& input)
	{
		// TODO: Add unit tests for this!
		// TODO: This is a generic matrix inverse function, implement the affine version?
		AffineMatrix_32 input_transposed = matrix_transpose(input);

		Vector4_32 v00 = vector_mix_xxyy(input_transposed.z_axis);
		Vector4_32 v01 = vector_mix_xxyy(input_transposed.x_axis);
		Vector4_32 v02 = vector_mix_xzac(input_transposed.z_axis, input_transposed.x_axis);
		Vector4_32 v10 = vector_mix_zwzw(input_transposed.w_axis);
		Vector4_32 v11 = vector_mix_zwzw(input_transposed.y_axis);
		Vector4_32 v12 = vector_mix_ywbd(input_transposed.w_axis, input_transposed.y_axis);

		Vector4_32 d0 = vector_mul(v00, v10);
		Vector4_32 d1 = vector_mul(v01, v11);
		Vector4_32 d2 = vector_mul(v02, v12);

		v00 = vector_mix_zwzw(input_transposed.z_axis);
		v01 = vector_mix_zwzw(input_transposed.x_axis);
		v02 = vector_mix_ywbd(input_transposed.z_axis, input_transposed.x_axis);
		v10 = vector_mix_xxyy(input_transposed.w_axis);
		v11 = vector_mix_xxyy(input_transposed.y_axis);
		v12 = vector_mix_xzac(input_transposed.w_axis, input_transposed.y_axis);

		d0 = vector_neg_mul_sub(v00, v10, d0);
		d1 = vector_neg_mul_sub(v01, v11, d1);
		d2 = vector_neg_mul_sub(v02, v12, d2);

		v00 = vector_mix_yzxy(input_transposed.y_axis);
		v01 = vector_mix_zxyx(input_transposed.x_axis);
		v02 = vector_mix_yzxy(input_transposed.w_axis);
		Vector4_32 v03 = vector_mix_zxyx(input_transposed.z_axis);
		v10 = vector_mix_bywx(d0, d2);
		v11 = vector_mix_wbyz(d0, d2);
		v12 = vector_mix_dywx(d1, d2);
		Vector4_32 v13 = vector_mix_wdyz(d1, d2);

		Vector4_32 c0 = vector_mul(v00, v10);
		Vector4_32 c2 = vector_mul(v01, v11);
		Vector4_32 c4 = vector_mul(v02, v12);
		Vector4_32 c6 = vector_mul(v03, v13);

		v00 = vector_mix_zwyz(input_transposed.y_axis);
		v01 = vector_mix_wzwy(input_transposed.x_axis);
		v02 = vector_mix_zwyz(input_transposed.w_axis);
		v03 = vector_mix_wzwy(input_transposed.z_axis);
		v10 = vector_mix_wxya(d0, d2);
		v11 = vector_mix_zyax(d0, d2);
		v12 = vector_mix_wxyc(d1, d2);
		v13 = vector_mix_zycx(d1, d2);

		c0 = vector_neg_mul_sub(v00, v10, c0);
		c2 = vector_neg_mul_sub(v01, v11, c2);
		c4 = vector_neg_mul_sub(v02, v12, c4);
		c6 = vector_neg_mul_sub(v03, v13, c6);

		v00 = vector_mix_wxwx(input_transposed.y_axis);
		v01 = vector_mix_ywxz(input_transposed.x_axis);
		v02 = vector_mix_wxwx(input_transposed.w_axis);
		v03 = vector_mix_ywxz(input_transposed.z_axis);
		v10 = vector_mix_zbaz(d0, d2);
		v11 = vector_mix_bxwa(d0, d2);
		v12 = vector_mix_zdcz(d1, d2);
		v13 = vector_mix_dxwc(d1, d2);

		Vector4_32 c1 = vector_neg_mul_sub(v00, v10, c0);
		c0 = vector_mul_add(v00, v10, c0);
		Vector4_32 c3 = vector_mul_add(v01, v11, c2);
		c2 = vector_neg_mul_sub(v01, v11, c2);
		Vector4_32 c5 = vector_neg_mul_sub(v02, v12, c4);
		c4 = vector_mul_add(v02, v12, c4);
		Vector4_32 c7 = vector_mul_add(v03, v13, c6);
		c6 = vector_neg_mul_sub(v03, v13, c6);

		Vector4_32 x_axis = vector_mix_xbxb(c0, c1);
		Vector4_32 y_axis = vector_mix_xbxb(c2, c3);
		Vector4_32 z_axis = vector_mix_xbxb(c4, c5);
		Vector4_32 w_axis = vector_mix_xbxb(c6, c7);

		float det = vector_dot(x_axis, input_transposed.x_axis);
		Vector4_32 inv_det = vector_set(reciprocal(det));

		x_axis = vector_mul(x_axis, inv_det);
		y_axis = vector_mul(y_axis, inv_det);
		z_axis = vector_mul(z_axis, inv_det);
		w_axis = vector_mul(w_axis, inv_det);
		return matrix_set(x_axis, y_axis, z_axis, w_axis);
	}
}
