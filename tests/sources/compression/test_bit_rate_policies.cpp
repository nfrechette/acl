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

#include <acl/compression/transform_adapters.h>
#include <acl/compression/impl/bit_rate_policies.transform.h>

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cstdint>

using namespace acl;
using namespace acl_impl;

namespace
{
	// The transform values of a chain: F -> X -> A -> B -> D (parent first)
	// F is the object transform of the parent of X, the others are the values in mesh rotation space
	struct chain_values_t
	{
		rtm::quatf rotations[5];
		rtm::vector4f translations[5];
		float scales[5];
	};

	chain_values_t make_chain_values(bool has_scale)
	{
		chain_values_t values;
		values.rotations[0] = rtm::quat_from_euler(rtm::scalar_deg_to_rad(10.0F), rtm::scalar_deg_to_rad(-35.0F), rtm::scalar_deg_to_rad(70.0F));
		values.rotations[1] = rtm::quat_from_euler(rtm::scalar_deg_to_rad(-60.0F), rtm::scalar_deg_to_rad(25.0F), rtm::scalar_deg_to_rad(5.0F));
		values.rotations[2] = rtm::quat_from_euler(rtm::scalar_deg_to_rad(45.0F), rtm::scalar_deg_to_rad(80.0F), rtm::scalar_deg_to_rad(-20.0F));
		values.rotations[3] = rtm::quat_from_euler(rtm::scalar_deg_to_rad(-15.0F), rtm::scalar_deg_to_rad(-50.0F), rtm::scalar_deg_to_rad(130.0F));
		values.rotations[4] = rtm::quat_from_euler(rtm::scalar_deg_to_rad(90.0F), rtm::scalar_deg_to_rad(15.0F), rtm::scalar_deg_to_rad(-75.0F));

		values.translations[0] = rtm::vector_set(1.0F, -2.0F, 3.0F);
		values.translations[1] = rtm::vector_set(0.0F, 4.0F, -1.5F);
		values.translations[2] = rtm::vector_set(2.5F, 0.5F, 1.0F);
		values.translations[3] = rtm::vector_set(-1.0F, 3.0F, 0.25F);
		values.translations[4] = rtm::vector_set(0.75F, -0.5F, 2.0F);

		const float scales[5] = { 1.5F, 0.5F, 2.0F, 1.25F, 0.8F };
		for (uint32_t index = 0; index < 5; ++index)
			values.scales[index] = has_scale ? scales[index] : 1.0F;

		return values;
	}

	rtm::vqsf make_vqs(const chain_values_t& values, uint32_t index)
	{
		return rtm::vqs_set(values.rotations[index], values.translations[index], values.scales[index]);
	}

	rtm::vqvf make_vq(const chain_values_t& values, uint32_t index)
	{
		return rtm::vqv_set(values.rotations[index], values.translations[index], rtm::vector_set(1.0F));
	}

	// Simulates the search on the chain X -> A -> B -> D: it optimizes D, B, and A, then it measures each
	// transform when it optimizes X. It compares the result with the mesh rotation product.
	template<class transform_math_t, class make_transform_t>
	void test_mesh_rotation_cache(const chain_values_t& values, make_transform_t make_transform)
	{
		using cache = mesh_rotation_cache_t<transform_math_t>;
		using transform_type = typename transform_math_t::transform_type;

		const transform_type f_object = make_transform(values, 0);
		const transform_type x_local = make_transform(values, 1);
		const transform_type a_local = make_transform(values, 2);
		const transform_type b_local = make_transform(values, 3);
		const transform_type d_local = make_transform(values, 4);

		// The object transforms with the mesh rotation product
		const transform_type x_object = transform_math_t::mul_mesh_rotation(x_local, f_object);
		const transform_type a_object = transform_math_t::mul_mesh_rotation(a_local, x_object);
		const transform_type b_object = transform_math_t::mul_mesh_rotation(b_local, a_object);
		const transform_type d_object = transform_math_t::mul_mesh_rotation(d_local, b_object);

		// Optimize D: its cached transform holds its local transform
		transform_type d_cached = d_local;

		// Optimize B: D is its chain child
		d_cached = cache::update(b_local, d_cached, d_cached, true);
		transform_type b_cached = b_local;

		// Optimize A: B is the chain child of D and B
		d_cached = cache::update(a_local, b_cached, d_cached, false);
		b_cached = cache::update(a_local, b_cached, b_cached, true);
		const transform_type a_cached = a_local;

		// Optimize X: A is the chain child of D, B, and A
		const transform_type a_measured = cache::measure(x_object, a_cached, a_cached, true);
		const transform_type b_measured = cache::measure(x_object, a_cached, b_cached, false);
		const transform_type d_measured = cache::measure(x_object, a_cached, d_cached, false);

		const float threshold = 1.0E-4F;

		// We compare points: the X, Y, and Z lanes hold the X, Y, and Z unit axes, the W lane holds the origin
		const rtm::vector4f points_xxxx = rtm::vector_set(1.0F, 0.0F, 0.0F, 0.0F);
		const rtm::vector4f points_yyyy = rtm::vector_set(0.0F, 1.0F, 0.0F, 0.0F);
		const rtm::vector4f points_zzzz = rtm::vector_set(0.0F, 0.0F, 1.0F, 0.0F);

		const transform_type* expected_transforms[3] = { &a_object, &b_object, &d_object };
		const transform_type* measured_transforms[3] = { &a_measured, &b_measured, &d_measured };
		for (uint32_t transform_index = 0; transform_index < 3; ++transform_index)
		{
			const transform_type& expected = *expected_transforms[transform_index];
			const transform_type& measured = *measured_transforms[transform_index];

			CHECK(rtm::quat_near_equal(expected.rotation, measured.rotation, threshold));

			rtm::vector4f expected_xxxx;
			rtm::vector4f expected_yyyy;
			rtm::vector4f expected_zzzz;
			transform_math_t::mul_point3_x4(points_xxxx, points_yyyy, points_zzzz, expected, expected_xxxx, expected_yyyy, expected_zzzz);

			rtm::vector4f measured_xxxx;
			rtm::vector4f measured_yyyy;
			rtm::vector4f measured_zzzz;
			transform_math_t::mul_point3_x4(points_xxxx, points_yyyy, points_zzzz, measured, measured_xxxx, measured_yyyy, measured_zzzz);

			CHECK(rtm::vector_all_near_equal(expected_xxxx, measured_xxxx, threshold));
			CHECK(rtm::vector_all_near_equal(expected_yyyy, measured_yyyy, threshold));
			CHECK(rtm::vector_all_near_equal(expected_zzzz, measured_zzzz, threshold));
		}
	}
}

TEST_CASE("mesh rotation cache", "[compression][transform]")
{
	CHECK(mesh_rotation_cache_t<vq_transform_math>::is_supported());
	CHECK(mesh_rotation_cache_t<vqs_transform_math>::is_supported());
	CHECK(!mesh_rotation_cache_t<vqv_transform_math>::is_supported());
	CHECK(!mesh_rotation_cache_t<vqm_transform_math>::is_supported());

	// VQ transforms do not have a scale
	test_mesh_rotation_cache<vq_transform_math>(make_chain_values(false), make_vq);

	// VQS transforms, with and without a scale
	test_mesh_rotation_cache<vqs_transform_math>(make_chain_values(false), make_vqs);
	test_mesh_rotation_cache<vqs_transform_math>(make_chain_values(true), make_vqs);
}
