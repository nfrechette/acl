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

#include "test_vector4_impl.h"

TEST_CASE("vector4 32 math", "[math][vector4]")
{
	test_vector4_impl<Vector4_32, Quat_32, float>(vector_zero_32(), quat_identity_32(), 1.0e-6f);

	const Vector4_32 src = vector_set(-2.65f, 2.996113f, 0.68123521f, -5.9182f);
	const Vector4_64 dst = vector_cast(src);
	REQUIRE(scalar_near_equal(vector_get_x(dst), -2.65, 1.0e-6));
	REQUIRE(scalar_near_equal(vector_get_y(dst), 2.996113, 1.0e-6));
	REQUIRE(scalar_near_equal(vector_get_z(dst), 0.68123521, 1.0e-6));
	REQUIRE(scalar_near_equal(vector_get_w(dst), -5.9182, 1.0e-6));
}
