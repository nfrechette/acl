////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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

#include "catch2.impl.h"

#include <acl/math/quatf.h>

#include <rtm/macros.h>
#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

using namespace acl;

TEST_CASE("quat_mul_vector3_x4", "[math][quat]")
{
	const rtm::quatf rotations[] =
	{
		rtm::quat_identity(),
		rtm::quat_from_axis_angle(rtm::vector_set(0.0F, 0.0F, 1.0F), 1.5707963F),
		rtm::quat_from_euler(0.4F, 1.1F, -0.6F),
		rtm::quat_from_euler(-2.1F, 0.3F, 2.8F),
	};

	const rtm::vector4f vector0 = rtm::vector_set(1.0F, 0.0F, 0.0F);
	const rtm::vector4f vector1 = rtm::vector_set(0.0F, 2.0F, 0.0F);
	const rtm::vector4f vector2 = rtm::vector_set(0.0F, 0.0F, 3.0F);
	const rtm::vector4f vector3 = rtm::vector_set(4.5F, 0.6F, -0.9F);
	const rtm::vector4f vectors[4] = { vector0, vector1, vector2, vector3 };

	rtm::vector4f vectors_xxxx;
	rtm::vector4f vectors_yyyy;
	rtm::vector4f vectors_zzzz;
	RTM_MATRIXF_TRANSPOSE_4X3(vector0, vector1, vector2, vector3, vectors_xxxx, vectors_yyyy, vectors_zzzz);

	for (const rtm::quatf& rotation : rotations)
	{
		rtm::vector4f result_xxxx;
		rtm::vector4f result_yyyy;
		rtm::vector4f result_zzzz;
		acl_impl::quat_mul_vector3_x4(vectors_xxxx, vectors_yyyy, vectors_zzzz, rotation, result_xxxx, result_yyyy, result_zzzz);

		const float result_x[4] = { rtm::vector_get_x(result_xxxx), rtm::vector_get_y(result_xxxx), rtm::vector_get_z(result_xxxx), rtm::vector_get_w(result_xxxx) };
		const float result_y[4] = { rtm::vector_get_x(result_yyyy), rtm::vector_get_y(result_yyyy), rtm::vector_get_z(result_yyyy), rtm::vector_get_w(result_yyyy) };
		const float result_z[4] = { rtm::vector_get_x(result_zzzz), rtm::vector_get_y(result_zzzz), rtm::vector_get_z(result_zzzz), rtm::vector_get_w(result_zzzz) };

		for (uint32_t vector_index = 0; vector_index < 4; ++vector_index)
		{
			const rtm::vector4f expected = rtm::quat_mul_vector3(vectors[vector_index], rotation);
			CHECK(rtm::scalar_near_equal(result_x[vector_index], rtm::vector_get_x(expected), 1.0E-4F));
			CHECK(rtm::scalar_near_equal(result_y[vector_index], rtm::vector_get_y(expected), 1.0E-4F));
			CHECK(rtm::scalar_near_equal(result_z[vector_index], rtm::vector_get_z(expected), 1.0E-4F));
		}
	}
}
