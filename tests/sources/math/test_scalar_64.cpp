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

#include <acl/math/scalar_64.h>

#include <limits>

using namespace acl;

TEST_CASE("scalar 64 math", "[math][scalar]")
{
	REQUIRE(acl::floor(0.0) == 0.0);
	REQUIRE(acl::floor(0.5) == 0.0);
	REQUIRE(acl::floor(2.5) == 2.0);
	REQUIRE(acl::floor(3.0) == 3.0);
	REQUIRE(acl::floor(-0.5) == -1.0);
	REQUIRE(acl::floor(-2.5) == -3.0);
	REQUIRE(acl::floor(-3.0) == -3.0);

	REQUIRE(acl::ceil(0.0) == 0.0);
	REQUIRE(acl::ceil(0.5) == 1.0);
	REQUIRE(acl::ceil(2.5) == 3.0);
	REQUIRE(acl::ceil(3.0) == 3.0);
	REQUIRE(acl::ceil(-0.5) == 0.0);
	REQUIRE(acl::ceil(-2.5) == -2.0);
	REQUIRE(acl::ceil(-3.0) == -3.0);

	REQUIRE(clamp(0.5, 0.0, 1.0) == 0.5);
	REQUIRE(clamp(-0.5, 0.0, 1.0) == 0.0);
	REQUIRE(clamp(1.5, 0.0, 1.0) == 1.0);

	REQUIRE(acl::abs(0.0) == 0.0);
	REQUIRE(acl::abs(2.0) == 2.0);
	REQUIRE(acl::abs(-2.0) == 2.0);

	REQUIRE(scalar_near_equal(1.0, 1.0, 0.00001) == true);
	REQUIRE(scalar_near_equal(1.0, 1.000001, 0.00001) == true);
	REQUIRE(scalar_near_equal(1.0, 0.999999, 0.00001) == true);
	REQUIRE(scalar_near_equal(1.0, 1.001, 0.00001) == false);
	REQUIRE(scalar_near_equal(1.0, 0.999, 0.00001) == false);

	const double threshold = 1.0e-9;

	REQUIRE(acl::sqrt(0.0) == 0.0);
	REQUIRE(scalar_near_equal(acl::sqrt(0.5), std::sqrt(0.5), threshold));
	REQUIRE(scalar_near_equal(acl::sqrt(32.5), std::sqrt(32.5), threshold));

	REQUIRE(scalar_near_equal(acl::sqrt_reciprocal(0.5), 1.0 / std::sqrt(0.5), threshold));
	REQUIRE(scalar_near_equal(acl::sqrt_reciprocal(32.5), 1.0 / std::sqrt(32.5), threshold));

	REQUIRE(scalar_near_equal(acl::reciprocal(0.5), 1.0 / 0.5, threshold));
	REQUIRE(scalar_near_equal(acl::reciprocal(32.5), 1.0 / 32.5, threshold));
	REQUIRE(scalar_near_equal(acl::reciprocal(-0.5), 1.0 / -0.5, threshold));
	REQUIRE(scalar_near_equal(acl::reciprocal(-32.5), 1.0 / -32.5, threshold));

	const double angles[] = { 0.0, acl::k_pi_64, -acl::k_pi_64, acl::k_pi_64 * 0.5, -acl::k_pi_64 * 0.5, 0.5, 32.5, -0.5, -32.5 };

	for (const double angle : angles)
	{
		REQUIRE(scalar_near_equal(acl::sin(angle), std::sin(angle), threshold));
		REQUIRE(scalar_near_equal(acl::cos(angle), std::cos(angle), threshold));

		double sin_result;
		double cos_result;
		acl::sincos(angle, sin_result, cos_result);
		REQUIRE(scalar_near_equal(sin_result, std::sin(angle), threshold));
		REQUIRE(scalar_near_equal(cos_result, std::cos(angle), threshold));
	}

	REQUIRE(scalar_near_equal(acl::acos(-1.0), std::acos(-1.0), threshold));
	REQUIRE(scalar_near_equal(acl::acos(-0.75), std::acos(-0.75), threshold));
	REQUIRE(scalar_near_equal(acl::acos(-0.5), std::acos(-0.5), threshold));
	REQUIRE(scalar_near_equal(acl::acos(-0.25), std::acos(-0.25), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.0), std::acos(0.0), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.25), std::acos(0.25), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.5), std::acos(0.5), threshold));
	REQUIRE(scalar_near_equal(acl::acos(0.75), std::acos(0.75), threshold));
	REQUIRE(scalar_near_equal(acl::acos(1.0), std::acos(1.0), threshold));

	REQUIRE(scalar_near_equal(acl::atan2(-2.0, -2.0), std::atan2(-2.0, -2.0), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(-1.0, -2.0), std::atan2(-1.0, -2.0), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(-2.0, -1.0), std::atan2(-2.0, -1.0), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(2.0, 2.0), std::atan2(2.0, 2.0), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(1.0, 2.0), std::atan2(1.0, 2.0), threshold));
	REQUIRE(scalar_near_equal(acl::atan2(2.0, 1.0), std::atan2(2.0, 1.0), threshold));

	REQUIRE(acl::min(-0.5, 1.0) == -0.5);
	REQUIRE(acl::min(1.0, -0.5) == -0.5);
	REQUIRE(acl::min(1.0, 1.0) == 1.0);

	REQUIRE(acl::max(-0.5, 1.0) == 1.0);
	REQUIRE(acl::max(1.0, -0.5) == 1.0);
	REQUIRE(acl::max(1.0, 1.0) == 1.0);

	REQUIRE(deg2rad(0.0) == 0.0);
	REQUIRE(scalar_near_equal(deg2rad(90.0), acl::k_pi_64 * 0.5, threshold));
	REQUIRE(scalar_near_equal(deg2rad(-90.0), -acl::k_pi_64 * 0.5, threshold));
	REQUIRE(scalar_near_equal(deg2rad(180.0), acl::k_pi_64, threshold));
	REQUIRE(scalar_near_equal(deg2rad(-180.0), -acl::k_pi_64, threshold));
	REQUIRE(scalar_near_equal(deg2rad(360.0), acl::k_pi_64 * 2.0, threshold));
	REQUIRE(scalar_near_equal(deg2rad(-360.0), -acl::k_pi_64 * 2.0, threshold));

	REQUIRE(acl::is_finite(0.0) == true);
	REQUIRE(acl::is_finite(32.0) == true);
	REQUIRE(acl::is_finite(-32.0) == true);
	REQUIRE(acl::is_finite(std::numeric_limits<double>::infinity()) == false);
	REQUIRE(acl::is_finite(std::numeric_limits<double>::quiet_NaN()) == false);
	REQUIRE(acl::is_finite(std::numeric_limits<double>::signaling_NaN()) == false);

	REQUIRE(symmetric_round(-1.75) == -2.0);
	REQUIRE(symmetric_round(-1.5) == -2.0);
	REQUIRE(symmetric_round(-1.4999) == -1.0);
	REQUIRE(symmetric_round(-0.5) == -1.0);
	REQUIRE(symmetric_round(-0.4999) == 0.0);
	REQUIRE(symmetric_round(0.0) == 0.0);
	REQUIRE(symmetric_round(0.4999) == 0.0);
	REQUIRE(symmetric_round(0.5) == 1.0);
	REQUIRE(symmetric_round(1.4999) == 1.0);
	REQUIRE(symmetric_round(1.5) == 2.0);
	REQUIRE(symmetric_round(1.75) == 2.0);

	REQUIRE(fraction(0.0) == 0.0);
	REQUIRE(fraction(1.0) == 0.0);
	REQUIRE(fraction(-1.0) == 0.0);
	REQUIRE(scalar_near_equal(fraction(0.25), 0.25, threshold));
	REQUIRE(scalar_near_equal(fraction(0.5), 0.5, threshold));
	REQUIRE(scalar_near_equal(fraction(0.75), 0.75, threshold));
}
