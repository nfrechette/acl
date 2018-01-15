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

#include <acl/math/scalar_32.h>

#include <limits>

using namespace acl;

TEST_CASE("scalar 32 math", "[math][scalar]")
{
	REQUIRE(acl::floor(0.0f) == 0.0f);
	REQUIRE(acl::floor(0.5f) == 0.0f);
	REQUIRE(acl::floor(2.5f) == 2.0f);
	REQUIRE(acl::floor(3.0f) == 3.0f);
	REQUIRE(acl::floor(-0.5f) == -1.0f);
	REQUIRE(acl::floor(-2.5f) == -3.0f);
	REQUIRE(acl::floor(-3.0f) == -3.0f);

	REQUIRE(acl::ceil(0.0f) == 0.0f);
	REQUIRE(acl::ceil(0.5f) == 1.0f);
	REQUIRE(acl::ceil(2.5f) == 3.0f);
	REQUIRE(acl::ceil(3.0f) == 3.0f);
	REQUIRE(acl::ceil(-0.5f) == 0.0f);
	REQUIRE(acl::ceil(-2.5f) == -2.0f);
	REQUIRE(acl::ceil(-3.0f) == -3.0f);

	REQUIRE(clamp(0.5f, 0.0f, 1.0f) == 0.5f);
	REQUIRE(clamp(-0.5f, 0.0f, 1.0f) == 0.0f);
	REQUIRE(clamp(1.5f, 0.0f, 1.0f) == 1.0f);

	REQUIRE(acl::abs(0.0f) == 0.0f);
	REQUIRE(acl::abs(2.0f) == 2.0f);
	REQUIRE(acl::abs(-2.0f) == 2.0f);

	REQUIRE(scalar_near_equal(1.0f, 1.0f, 0.00001f) == true);
	REQUIRE(scalar_near_equal(1.0f, 1.000001f, 0.00001f) == true);
	REQUIRE(scalar_near_equal(1.0f, 0.999999f, 0.00001f) == true);
	REQUIRE(scalar_near_equal(1.0f, 1.001f, 0.00001f) == false);
	REQUIRE(scalar_near_equal(1.0f, 0.999f, 0.00001f) == false);

	const float threshold = 1.0e-6f;

	REQUIRE(acl::sqrt(0.0f) == 0.0f);
	REQUIRE(scalar_near_equal(acl::sqrt(0.5f), std::sqrt(0.5f), threshold));
	REQUIRE(scalar_near_equal(acl::sqrt(32.5f), std::sqrt(32.5f), threshold));

	REQUIRE(scalar_near_equal(acl::sqrt_reciprocal(0.5f), 1.0f / std::sqrt(0.5f), threshold));
	REQUIRE(scalar_near_equal(acl::sqrt_reciprocal(32.5f), 1.0f / std::sqrt(32.5f), threshold));

	REQUIRE(scalar_near_equal(acl::reciprocal(0.5f), 1.0f / 0.5f, threshold));
	REQUIRE(scalar_near_equal(acl::reciprocal(32.5f), 1.0f / 32.5f, threshold));
	REQUIRE(scalar_near_equal(acl::reciprocal(-0.5f), 1.0f / -0.5f, threshold));
	REQUIRE(scalar_near_equal(acl::reciprocal(-32.5f), 1.0f / -32.5f, threshold));

	const float angles[] = { 0.0f, acl::k_pi_32, -acl::k_pi_32, acl::k_pi_32 * 0.5f, -acl::k_pi_32 * 0.5f, 0.5f, 32.5f, -0.5f, -32.5f };

	for (const float angle : angles)
	{
		REQUIRE(scalar_near_equal(acl::sin(angle), std::sin(angle), threshold));
		REQUIRE(scalar_near_equal(acl::cos(angle), std::cos(angle), threshold));

		float sin_result;
		float cos_result;
		acl::sincos(angle, sin_result, cos_result);
		REQUIRE(scalar_near_equal(sin_result, std::sin(angle), threshold));
		REQUIRE(scalar_near_equal(cos_result, std::cos(angle), threshold));
	}

	REQUIRE(scalar_near_equal(acl::acos(-1.0f), std::acos(-1.0f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(-0.75f), std::acos(-0.75f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(-0.5f), std::acos(-0.5f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(-0.25f), std::acos(-0.25f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.0f), std::acos(0.0f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.25f), std::acos(0.25f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.5f), std::acos(0.5f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.75f), std::acos(0.75f), threshold));
	REQUIRE(scalar_near_equal(acl::acos(1.0f), std::acos(1.0f), threshold));

	REQUIRE(scalar_near_equal(acl::atan2(-2.0f, -2.0f), std::atan2(-2.0f, -2.0f), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(-1.0f, -2.0f), std::atan2(-1.0f, -2.0f), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(-2.0f, -1.0f), std::atan2(-2.0f, -1.0f), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(2.0f, 2.0f), std::atan2(2.0f, 2.0f), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(1.0f, 2.0f), std::atan2(1.0f, 2.0f), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(2.0f, 1.0f), std::atan2(2.0f, 1.0f), threshold));

	REQUIRE(acl::min(-0.5f, 1.0f) == -0.5f);
	REQUIRE(acl::min(1.0f, -0.5f) == -0.5f);
	REQUIRE(acl::min(1.0f, 1.0f) == 1.0f);

	REQUIRE(acl::max(-0.5f, 1.0f) == 1.0f);
	REQUIRE(acl::max(1.0f, -0.5f) == 1.0f);
	REQUIRE(acl::max(1.0f, 1.0f) == 1.0f);

	REQUIRE(deg2rad(0.0f) == 0.0f);
	REQUIRE(scalar_near_equal(deg2rad(90.0f), acl::k_pi_32 * 0.5f, threshold));
	REQUIRE(scalar_near_equal(deg2rad(-90.0f), -acl::k_pi_32 * 0.5f, threshold));
	REQUIRE(scalar_near_equal(deg2rad(180.0f), acl::k_pi_32, threshold));
	REQUIRE(scalar_near_equal(deg2rad(-180.0f), -acl::k_pi_32, threshold));
	REQUIRE(scalar_near_equal(deg2rad(360.0f), acl::k_pi_32 * 2.0f, threshold));
	REQUIRE(scalar_near_equal(deg2rad(-360.0f), -acl::k_pi_32 * 2.0f, threshold));

	REQUIRE(acl::is_finite(0.0f) == true);
	REQUIRE(acl::is_finite(32.0f) == true);
	REQUIRE(acl::is_finite(-32.0f) == true);
	REQUIRE(acl::is_finite(std::numeric_limits<float>::infinity()) == false);
	REQUIRE(acl::is_finite(std::numeric_limits<float>::quiet_NaN()) == false);
	REQUIRE(acl::is_finite(std::numeric_limits<float>::signaling_NaN()) == false);

	REQUIRE(symmetric_round(-1.75f) == -2.0f);
	REQUIRE(symmetric_round(-1.5f) == -2.0f);
	REQUIRE(symmetric_round(-1.4999f) == -1.0f);
	REQUIRE(symmetric_round(-0.5f) == -1.0f);
	REQUIRE(symmetric_round(-0.4999f) == 0.0f);
	REQUIRE(symmetric_round(0.0f) == 0.0f);
	REQUIRE(symmetric_round(0.4999f) == 0.0f);
	REQUIRE(symmetric_round(0.5f) == 1.0f);
	REQUIRE(symmetric_round(1.4999f) == 1.0f);
	REQUIRE(symmetric_round(1.5f) == 2.0f);
	REQUIRE(symmetric_round(1.75f) == 2.0f);

	REQUIRE(fraction(0.0f) == 0.0f);
	REQUIRE(fraction(1.0f) == 0.0f);
	REQUIRE(fraction(-1.0f) == 0.0f);
	REQUIRE(scalar_near_equal(fraction(0.25f), 0.25f, threshold));
	REQUIRE(scalar_near_equal(fraction(0.5f), 0.5f, threshold));
	REQUIRE(scalar_near_equal(fraction(0.75f), 0.75f, threshold));
}
