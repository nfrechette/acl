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

#include <catch2/catch.hpp>

#include <acl/core/interpolation_utils.h>
#include <rtm/scalarf.h>

using namespace acl;
using namespace rtm;

TEST_CASE("interpolation utils", "[core][utils]")
{
	const float error_threshold = 1.0E-6F;

	uint32_t key0;
	uint32_t key1;
	float alpha;
	find_linear_interpolation_samples_with_duration(31, 1.0F, 0.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 0);
	CHECK(key1 == 1);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_duration(31, 1.0F, 1.0F / 30.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 1);
	CHECK(key1 == 2);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_duration(31, 1.0F, 2.5F / 30.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 0.5F, error_threshold));

	find_linear_interpolation_samples_with_duration(31, 1.0F, 1.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 30);
	CHECK(key1 == 30);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_duration(31, 1.0F, 2.5F / 30.0F, sample_rounding_policy::floor, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_duration(31, 1.0F, 2.5F / 30.0F, sample_rounding_policy::ceil, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 1.0F, error_threshold));

	find_linear_interpolation_samples_with_duration(31, 1.0F, 2.4F / 30.0F, sample_rounding_policy::nearest, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_duration(31, 1.0F, 2.6F / 30.0F, sample_rounding_policy::nearest, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 1.0F, error_threshold));

	//////////////////////////////////////////////////////////////////////////

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 0.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 0);
	CHECK(key1 == 1);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 1.0F / 30.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 1);
	CHECK(key1 == 2);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 2.5F / 30.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 0.5F, error_threshold));

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 1.0F, sample_rounding_policy::none, key0, key1, alpha);

	CHECK(key0 == 30);
	CHECK(key1 == 30);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 2.5F / 30.0F, sample_rounding_policy::floor, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 2.5F / 30.0F, sample_rounding_policy::ceil, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 1.0F, error_threshold));

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 2.4F / 30.0F, sample_rounding_policy::nearest, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 0.0F, error_threshold));

	find_linear_interpolation_samples_with_sample_rate(31, 30.0F, 2.6F / 30.0F, sample_rounding_policy::nearest, key0, key1, alpha);

	CHECK(key0 == 2);
	CHECK(key1 == 3);
	CHECK(scalar_near_equal(alpha, 1.0F, error_threshold));

	//////////////////////////////////////////////////////////////////////////

	CHECK(scalar_near_equal(find_linear_interpolation_alpha(0.0F, 1, 1, sample_rounding_policy::none), 0.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 2, sample_rounding_policy::none), 0.5F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 2, sample_rounding_policy::none), 0.75F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 3, sample_rounding_policy::none), 0.5F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 4, sample_rounding_policy::none), 0.16666667F, error_threshold));

	CHECK(scalar_near_equal(find_linear_interpolation_alpha(0.0F, 1, 1, sample_rounding_policy::floor), 0.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 2, sample_rounding_policy::floor), 0.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 2, sample_rounding_policy::floor), 0.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 3, sample_rounding_policy::floor), 0.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 4, sample_rounding_policy::floor), 0.0F, error_threshold));

	CHECK(scalar_near_equal(find_linear_interpolation_alpha(0.0F, 1, 1, sample_rounding_policy::ceil), 1.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 2, sample_rounding_policy::ceil), 1.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 2, sample_rounding_policy::ceil), 1.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 3, sample_rounding_policy::ceil), 1.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 4, sample_rounding_policy::ceil), 1.0F, error_threshold));

	CHECK(scalar_near_equal(find_linear_interpolation_alpha(0.0F, 1, 1, sample_rounding_policy::nearest), 0.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 2, sample_rounding_policy::nearest), 1.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 2, sample_rounding_policy::nearest), 1.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 0, 3, sample_rounding_policy::nearest), 1.0F, error_threshold));
	CHECK(scalar_near_equal(find_linear_interpolation_alpha(1.5F, 1, 4, sample_rounding_policy::nearest), 0.0F, error_threshold));
}
