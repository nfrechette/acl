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
#include <acl/compression/transform_error_metrics.h>
#include <acl/compression/transform_metrics.h>

#include <rtm/matrix3x4f.h>
#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
#include <rtm/vqmf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cmath>
#include <cstring>
#include <cstdint>

using namespace acl;

namespace
{
	constexpr float k_shell_distance = 3.0F;

	// A raw transform and lossy transforms near it, with a 3D scale
	// The lossy transforms have a small error in the rotation, the translation, and the scale
	struct transform_pair
	{
		rtm::vqvf raw;
		rtm::vqvf lossy;
	};

	transform_pair make_transform_pair(uint32_t pair_index, bool is_uniform_scale, bool has_scale_error)
	{
		const float offset = float(pair_index) * 0.37F;
		const rtm::quatf raw_rotation = rtm::quat_from_euler(0.3F + offset, 1.1F - offset, -0.4F + 0.5F * offset);
		const rtm::quatf delta_rotation = rtm::quat_from_euler(0.002F * float(pair_index % 3), 0.003F, -0.001F * float(pair_index));
		const rtm::vector4f raw_translation = rtm::vector_set(1.0F + offset, -2.0F, 0.5F * offset);
		const rtm::vector4f delta_translation = rtm::vector_set(0.001F, -0.002F * float(pair_index), 0.0015F);

		const float scale = 1.5F + 0.25F * float(pair_index);
		const rtm::vector4f raw_scale = is_uniform_scale ? rtm::vector_set(scale) : rtm::vector_set(scale, 0.5F, 2.0F);
		const float scale_error = has_scale_error ? (0.004F * float(pair_index + 1)) : 0.0F;
		const rtm::vector4f lossy_scale = rtm::vector_add(raw_scale, rtm::vector_set(scale_error));

		transform_pair pair;
		pair.raw = rtm::vqv_set(raw_rotation, raw_translation, raw_scale);
		pair.lossy = rtm::vqv_set(rtm::quat_normalize(rtm::quat_mul(delta_rotation, raw_rotation)), rtm::vector_add(raw_translation, delta_translation), lossy_scale);
		return pair;
	}

	constexpr uint32_t k_num_pairs = 6;

	// Two code paths with the same math can round differently: the compiler can merge a multiply
	// and an add (FMA) in one path and not in the other. Thus we compare with a small relative threshold.
	bool is_near(float value, float expected)
	{
		return std::fabs(value - expected) <= (1.0E-5F * std::fabs(expected)) + 1.0E-9F;
	}

	// Converts a pair of rtm::vqvf transforms into the runtime type of the adapter
	template<class adapter_type>
	void convert_pair(const adapter_type& adapter, const transform_pair& pair, typename adapter_type::transform_type out_transforms[2])
	{
		const rtm::vqvf transforms[2] = { pair.raw, pair.lossy };
		const uint32_t dirty_indices[2] = { 0, 1 };

		itransform_adapter::convert_transforms_args args;
		args.dirty_transform_indices = &dirty_indices[0];
		args.num_dirty_transforms = 2;
		args.transforms = &transforms[0];
		args.num_transforms = 2;
		args.sample_index = 0;
		args.is_lossy = false;

		if (adapter.needs_conversion())
			adapter.convert_transforms(args, out_transforms);
		else
			std::memcpy(out_transforms, &transforms[0], sizeof(transforms));
	}

	template<class adapter_type>
	float calculate_error(const itransform_metric& metric, const adapter_type& adapter, const transform_pair& pair)
	{
		typename adapter_type::transform_type transforms[2];
		convert_pair(adapter, pair, transforms);

		itransform_metric::calculate_error_args args;
		args.construct_sphere_shell(k_shell_distance);
		args.adapter = &adapter;
		args.transform0 = &transforms[0];
		args.transform1 = &transforms[1];

		return rtm::scalar_cast(metric.calculate_error(args));
	}

	// Measures the error with the old (2.1) interface, on rtm::vqvf or converted transforms
	// The legacy error metrics are deprecated
	ACL_IMPL_DEPRECATION_WARNING_PUSH
	float calculate_legacy_error(const itransform_error_metric& metric, const void* raw_transform, const void* lossy_transform, bool has_scale)
	{
		itransform_error_metric::calculate_error_args args;
		args.construct_sphere_shell(k_shell_distance);
		args.transform0 = raw_transform;
		args.transform1 = lossy_transform;

		return rtm::scalar_cast(has_scale ? metric.calculate_error(args) : metric.calculate_error_no_scale(args));
	}
	ACL_IMPL_DEPRECATION_WARNING_POP

	// Returns the largest error over many points of the sphere
	// The points cover the sphere evenly (Fibonacci sphere)
	template<class mul_point3_function_type>
	float calculate_brute_force_error(mul_point3_function_type mul_point3, const transform_pair& pair)
	{
		constexpr uint32_t k_num_points = 4096;
		const float golden_angle = 2.39996323F;

		float max_error = 0.0F;
		for (uint32_t point_index = 0; point_index < k_num_points; ++point_index)
		{
			const float y = 1.0F - (2.0F * (float(point_index) + 0.5F) / float(k_num_points));
			const float radius = std::sqrt(1.0F - y * y);
			const float angle = golden_angle * float(point_index);
			const rtm::vector4f point = rtm::vector_mul(rtm::vector_set(radius * std::cos(angle), y, radius * std::sin(angle)), k_shell_distance);

			const rtm::vector4f raw_point = mul_point3(point, pair.raw);
			const rtm::vector4f lossy_point = mul_point3(point, pair.lossy);
			const float error = rtm::vector_distance3(raw_point, lossy_point);
			max_error = error > max_error ? error : max_error;
		}

		return max_error;
	}

	rtm::vector4f mul_point3_no_scale(rtm::vector4f_arg0 point, const rtm::vqvf& transform)
	{
		return rtm::vqv_mul_point3_no_scale(point, transform);
	}

	rtm::vector4f mul_point3_uniform_scale(rtm::vector4f_arg0 point, const rtm::vqvf& transform)
	{
		const float scale = rtm::vector_get_x(transform.scale);
		return rtm::vqs_mul_point3(point, rtm::vqs_set(transform.rotation, transform.translation, scale));
	}

	rtm::vector4f mul_point3_vqm(rtm::vector4f_arg0 point, const rtm::vqvf& transform)
	{
		return rtm::vqm_mul_point3(point, rtm::vqm_set(transform.rotation, transform.translation, transform.scale));
	}
}

TEST_CASE("transform metric support", "[compression][transform]")
{
	const vqf_transform_adapter vq_adapter;
	const vqsf_transform_adapter vqs_adapter;
	const vqvf_transform_adapter vqv_adapter;
	const vqmf_transform_adapter vqm_adapter;
	const matrix3x4f_transform_adapter matrix_adapter;

	const shell_transform_metric shell_metric;
	CHECK(shell_metric.is_supported(vq_adapter));
	CHECK(shell_metric.is_supported(vqs_adapter));
	CHECK(shell_metric.is_supported(vqv_adapter));
	CHECK(shell_metric.is_supported(vqm_adapter));
	CHECK(shell_metric.is_supported(matrix_adapter));

	const precise_transform_metric precise_metric;
	CHECK(precise_metric.is_supported(vq_adapter));
	CHECK(precise_metric.is_supported(vqs_adapter));
	CHECK(!precise_metric.is_supported(vqv_adapter));
	CHECK(!precise_metric.is_supported(vqm_adapter));
	CHECK(!precise_metric.is_supported(matrix_adapter));

	CHECK(shell_metric.get_hash() != precise_metric.get_hash());
}

// This test compares with the deprecated legacy error metrics
ACL_IMPL_DEPRECATION_WARNING_PUSH

TEST_CASE("shell transform metric", "[compression][transform]")
{
	const shell_transform_metric metric;

	const vqf_transform_adapter vq_adapter;
	const vqvf_transform_adapter vqv_adapter;
	const vqmf_transform_adapter vqm_adapter;
	const matrix3x4f_transform_adapter matrix_adapter;

	const vqvf_transform_error_metric legacy_vqv_metric;
	const vqvf_matrix3x4f_transform_error_metric legacy_matrix_metric;

	for (uint32_t pair_index = 0; pair_index < k_num_pairs; ++pair_index)
	{
		const transform_pair pair = make_transform_pair(pair_index, false, true);

		// The shell metric gives the same error as the legacy metric of each transform type
		CHECK(is_near(calculate_error(metric, vq_adapter, pair), calculate_legacy_error(legacy_vqv_metric, &pair.raw, &pair.lossy, false)));
		CHECK(is_near(calculate_error(metric, vqv_adapter, pair), calculate_legacy_error(legacy_vqv_metric, &pair.raw, &pair.lossy, true)));

		rtm::matrix3x4f matrix_transforms[2];
		convert_pair(matrix_adapter, pair, matrix_transforms);
		CHECK(is_near(calculate_error(metric, matrix_adapter, pair), calculate_legacy_error(legacy_matrix_metric, &matrix_transforms[0], &matrix_transforms[1], true)));

		// VQM and matrix arithmetic give the same transforms
		// The math is not the same, thus the rounding is not the same: we use an absolute threshold
		CHECK(rtm::scalar_near_equal(calculate_error(metric, vqm_adapter, pair), calculate_error(metric, matrix_adapter, pair), 1.0E-5F));

		// The shell points are on the sphere, thus the error is not above the largest error on the sphere
		CHECK(calculate_error(metric, vqm_adapter, pair) <= calculate_brute_force_error(mul_point3_vqm, pair) + 1.0E-5F);
		CHECK(calculate_error(metric, vq_adapter, pair) <= calculate_brute_force_error(mul_point3_no_scale, pair) + 1.0E-5F);
	}
}

ACL_IMPL_DEPRECATION_WARNING_POP

// This test compares with the deprecated legacy error metrics
ACL_IMPL_DEPRECATION_WARNING_PUSH

TEST_CASE("precise transform metric with vqf", "[compression][transform]")
{
	const precise_transform_metric metric;
	const vqf_transform_adapter adapter;
	const vqvf_precise_transform_error_metric legacy_metric;

	for (uint32_t pair_index = 0; pair_index < k_num_pairs; ++pair_index)
	{
		const transform_pair pair = make_transform_pair(pair_index, false, true);

		const float error = calculate_error(metric, adapter, pair);

		// The precise metric gives the same error as the legacy precise metric
		CHECK(is_near(error, calculate_legacy_error(legacy_metric, &pair.raw, &pair.lossy, false)));

		// The error is the largest error on the sphere: the points of the brute force are not above it, and they get near it
		const float brute_force_error = calculate_brute_force_error(mul_point3_no_scale, pair);
		CHECK(error >= brute_force_error - 1.0E-5F);
		CHECK(error <= brute_force_error * 1.01F + 1.0E-5F);
	}
}

ACL_IMPL_DEPRECATION_WARNING_POP

TEST_CASE("precise transform metric with vqsf", "[compression][transform]")
{
	const precise_transform_metric metric;
	const vqsf_transform_adapter adapter;

	for (uint32_t pair_index = 0; pair_index < k_num_pairs; ++pair_index)
	{
		// With equal scales, the error is exact
		{
			const transform_pair pair = make_transform_pair(pair_index, true, false);

			const float error = calculate_error(metric, adapter, pair);
			const float brute_force_error = calculate_brute_force_error(mul_point3_uniform_scale, pair);
			CHECK(error >= brute_force_error - 1.0E-5F);
			CHECK(error <= brute_force_error * 1.01F + 1.0E-5F);
		}

		// With different scales, the error is a bound: not below the largest error,
		// and not above it by more than |s1 - s0| * r
		{
			const transform_pair pair = make_transform_pair(pair_index, true, true);

			const float raw_scale = rtm::vector_get_x(pair.raw.scale);
			const float lossy_scale = rtm::vector_get_x(pair.lossy.scale);
			const float max_scale_error = std::fabs(lossy_scale - raw_scale) * k_shell_distance;

			const float error = calculate_error(metric, adapter, pair);
			const float brute_force_error = calculate_brute_force_error(mul_point3_uniform_scale, pair);
			CHECK(error >= brute_force_error - 1.0E-5F);
			CHECK(error <= brute_force_error + max_scale_error + 1.0E-5F);
		}
	}

	// Without a rotation error, the error is the translation error plus the scale error on the sphere
	{
		const rtm::vqvf raw = rtm::vqv_set(rtm::quat_from_euler(0.3F, 0.2F, 0.1F), rtm::vector_set(1.0F, 2.0F, 3.0F), rtm::vector_set(2.0F));
		const rtm::vqvf lossy = rtm::vqv_set(raw.rotation, raw.translation, rtm::vector_set(2.01F));

		transform_pair pair;
		pair.raw = raw;
		pair.lossy = lossy;

		const float error = calculate_error(metric, adapter, pair);
		CHECK(std::fabs(error - (0.01F * k_shell_distance)) < 1.0E-5F);
	}
}
