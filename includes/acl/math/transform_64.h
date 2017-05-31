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

#include "acl/math/math.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_64.h"

namespace acl
{
	inline Transform_64 transform_set(const Quat_64& rotation, const Vector4_64& translation)
	{
		return Transform_64{ rotation, translation };
	}

	inline Transform_64 transform_mul(const Transform_64& lhs, const Transform_64& rhs)
	{
		Quat_64 rotation = quat_mul(rhs.rotation, lhs.rotation);
		Vector4_64 translation = vector_add(quat_rotate(rhs.rotation, lhs.translation), rhs.translation);
		return transform_set(rotation, translation);
	}

	inline Vector4_64 transform_position(const Transform_64& lhs, const Vector4_64& rhs)
	{
		return vector_add(quat_rotate(lhs.rotation, rhs), lhs.translation);
	}

	inline Transform_64 transform_inverse(const Transform_64& input)
	{
		Quat_64 rotation = quat_conjugate(input.rotation);
		Vector4_64 translation = quat_rotate(rotation, vector_neg(input.translation));
		return transform_set(rotation, translation);
	}
}
