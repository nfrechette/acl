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
#include <acl/core/track_types.h>

#include <rtm/matrix3x4f.h>
#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/vqmf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cstdint>

using namespace acl;

namespace
{
	constexpr uint32_t k_num_transforms = 3;

	// Two code paths with the same math can round differently: the compiler can merge a multiply
	// and an add (FMA) in one path and not in the other. Thus the tests use a small threshold.
	constexpr float k_rounding_threshold = 1.0E-5F;

	// Each transform is the child of the previous one
	const uint32_t k_parent_indices[k_num_transforms] = { k_invalid_track_index, 0, 1 };
	const uint32_t k_dirty_indices[k_num_transforms] = { 0, 1, 2 };

	// Three transforms with rotation, translation, and scale
	// With a uniform scale, the VQS adapter can use them
	void make_transforms(bool is_uniform_scale, rtm::vqvf out_transforms[k_num_transforms])
	{
		const rtm::vector4f scale0 = is_uniform_scale ? rtm::vector_set(1.5F) : rtm::vector_set(1.5F, 0.5F, 2.0F);
		const rtm::vector4f scale1 = is_uniform_scale ? rtm::vector_set(0.75F) : rtm::vector_set(0.8F, 1.7F, 1.1F);
		const rtm::vector4f scale2 = is_uniform_scale ? rtm::vector_set(1.25F) : rtm::vector_set(2.2F, 0.9F, 0.6F);

		out_transforms[0] = rtm::vqv_set(rtm::quat_from_euler(0.3F, 1.1F, -0.4F), rtm::vector_set(1.0F, -2.0F, 0.5F), scale0);
		out_transforms[1] = rtm::vqv_set(rtm::quat_from_euler(-0.7F, 0.2F, 0.9F), rtm::vector_set(0.25F, 3.0F, -1.5F), scale1);
		out_transforms[2] = rtm::vqv_set(rtm::quat_from_euler(1.4F, -0.6F, 0.1F), rtm::vector_set(-0.5F, 0.75F, 2.0F), scale2);
	}

	// Four points stored transposed: (3, 0, 0), (0, 3, 0), (0, 0, 3), (1.5, -2, 0.5)
	const rtm::vector4f k_points_xxxx = rtm::vector_set(3.0F, 0.0F, 0.0F, 1.5F);
	const rtm::vector4f k_points_yyyy = rtm::vector_set(0.0F, 3.0F, 0.0F, -2.0F);
	const rtm::vector4f k_points_zzzz = rtm::vector_set(0.0F, 0.0F, 3.0F, 0.5F);

	// Returns one point from four points stored transposed
	rtm::vector4f get_point(rtm::vector4f_arg0 xxxx, rtm::vector4f_arg1 yyyy, rtm::vector4f_arg2 zzzz, uint32_t point_index)
	{
		float x[4];
		float y[4];
		float z[4];
		rtm::vector_store(xxxx, &x[0]);
		rtm::vector_store(yyyy, &y[0]);
		rtm::vector_store(zzzz, &z[0]);
		return rtm::vector_set(x[point_index], y[point_index], z[point_index]);
	}

	rtm::vector4f get_point(uint32_t point_index)
	{
		return get_point(k_points_xxxx, k_points_yyyy, k_points_zzzz, point_index);
	}

	struct points_x4
	{
		rtm::vector4f xxxx;
		rtm::vector4f yyyy;
		rtm::vector4f zzzz;

		rtm::vector4f get_point(uint32_t point_index) const
		{
			return ::get_point(xxxx, yyyy, zzzz, point_index);
		}
	};

	points_x4 transform_points(const itransform_adapter& adapter, const void* transform, const points_x4& points)
	{
		points_x4 result;
		adapter.mul_point3_x4(points.xxxx, points.yyyy, points.zzzz, transform, result.xxxx, result.yyyy, result.zzzz);
		return result;
	}

	points_x4 transform_points(const itransform_adapter& adapter, const void* transform)
	{
		points_x4 points;
		points.xxxx = k_points_xxxx;
		points.yyyy = k_points_yyyy;
		points.zzzz = k_points_zzzz;
		return transform_points(adapter, transform, points);
	}

	void check_same_points(const points_x4& points0, const points_x4& points1, float threshold)
	{
		CHECK(rtm::vector_all_near_equal(points0.xxxx, points1.xxxx, threshold));
		CHECK(rtm::vector_all_near_equal(points0.yyyy, points1.yyyy, threshold));
		CHECK(rtm::vector_all_near_equal(points0.zzzz, points1.zzzz, threshold));
	}

	// Converts the rtm::vqvf transforms into the runtime type of the adapter
	template<class adapter_type>
	void convert(const adapter_type& adapter, const rtm::vqvf transforms[k_num_transforms], typename adapter_type::transform_type out_transforms[k_num_transforms])
	{
		itransform_adapter::convert_transforms_args args;
		args.dirty_transform_indices = &k_dirty_indices[0];
		args.num_dirty_transforms = k_num_transforms;
		args.transforms = transforms;
		args.num_transforms = k_num_transforms;

		adapter.convert_transforms(args, out_transforms);
	}

	// Checks the properties and the math that all built-in adapters share
	template<class adapter_type>
	void check_builtin_adapter(const adapter_type& adapter, transform_runtime_type8 expected_type, bool expected_associative, bool is_uniform_scale)
	{
		using transform_type = typename adapter_type::transform_type;

		CHECK(adapter.get_type() == expected_type);
		CHECK(adapter.get_transform_size() == sizeof(transform_type));
		CHECK(adapter.is_associative() == expected_associative);
		CHECK(adapter.needs_conversion() == (expected_type != transform_runtime_type8::vqf && expected_type != transform_runtime_type8::vqvf));

		rtm::vqvf transforms_vqv[k_num_transforms];
		make_transforms(is_uniform_scale, transforms_vqv);

		transform_type transforms[k_num_transforms];
		convert(adapter, transforms_vqv, transforms);

		// The product with the identity does not change the transform
		transform_type identity;
		adapter.identity(&identity);

		transform_type result;
		adapter.mul(&transforms[0], &identity, &result);
		check_same_points(transform_points(adapter, &result), transform_points(adapter, &transforms[0]), 1.0E-5F);

		adapter.mul(&identity, &transforms[0], &result);
		check_same_points(transform_points(adapter, &result), transform_points(adapter, &transforms[0]), 1.0E-5F);

		// The product transforms a point as the two transforms in sequence do
		// VQV discards the shear, thus this is not true for VQV transforms with a rotation and a 3D scale
		if (expected_associative)
		{
			adapter.mul(&transforms[0], &transforms[1], &result);
			const points_x4 points_in_1 = transform_points(adapter, &transforms[0]);
			check_same_points(transform_points(adapter, &result), transform_points(adapter, &transforms[1], points_in_1), 1.0E-4F);

			// (t0 * t1) * t2 == t0 * (t1 * t2)
			transform_type product01;
			transform_type product012_left;
			adapter.mul(&transforms[0], &transforms[1], &product01);
			adapter.mul(&product01, &transforms[2], &product012_left);

			transform_type product12;
			transform_type product012_right;
			adapter.mul(&transforms[1], &transforms[2], &product12);
			adapter.mul(&transforms[0], &product12, &product012_right);

			check_same_points(transform_points(adapter, &product012_left), transform_points(adapter, &product012_right), 1.0E-4F);
		}

		// The built-in 'local_to_object_space' gives the same result as a chain of 'mul' calls
		itransform_adapter::local_to_object_space_args args;
		args.dirty_transform_indices = &k_dirty_indices[0];
		args.num_dirty_transforms = k_num_transforms;
		args.parent_transform_indices = &k_parent_indices[0];
		args.local_transforms = &transforms[0];
		args.num_transforms = k_num_transforms;

		transform_type object_transforms[k_num_transforms];
		adapter.local_to_object_space(args, &object_transforms[0]);

		transform_type expected_object_transforms[k_num_transforms];
		expected_object_transforms[0] = transforms[0];
		adapter.mul(&transforms[1], &expected_object_transforms[0], &expected_object_transforms[1]);
		adapter.mul(&transforms[2], &expected_object_transforms[1], &expected_object_transforms[2]);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			check_same_points(transform_points(adapter, &object_transforms[transform_index]), transform_points(adapter, &expected_object_transforms[transform_index]), k_rounding_threshold);
	}

	// A custom adapter with VQM math
	// It does not replace 'local_to_object_space', thus it uses the default implementation
	struct custom_vqm_transform_adapter final : public itransform_adapter
	{
		virtual const char* get_name() const override { return "custom_vqm_transform_adapter"; }
		virtual size_t get_transform_size() const override { return sizeof(rtm::vqmf); }
		virtual bool is_associative() const override { return true; }

		virtual void convert_transforms(const convert_transforms_args& args, void* out_transforms) const override
		{
			rtm::vqmf* out_transforms_ = static_cast<rtm::vqmf*>(out_transforms);
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < args.num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = args.dirty_transform_indices[dirty_transform_index];
				const rtm::vqvf& transform = args.transforms[transform_index];
				out_transforms_[transform_index] = rtm::vqm_set(transform.rotation, transform.translation, transform.scale);
			}
		}

		virtual void identity(void* out_transform) const override
		{
			*static_cast<rtm::vqmf*>(out_transform) = rtm::vqm_identity();
		}

		virtual void mul(const void* lhs, const void* rhs, void* out_transform) const override
		{
			*static_cast<rtm::vqmf*>(out_transform) = rtm::vqm_normalize(rtm::vqm_mul(*static_cast<const rtm::vqmf*>(lhs), *static_cast<const rtm::vqmf*>(rhs)));
		}

		virtual void RTM_SIMD_CALL mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			const void* transform,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) const override
		{
			acl_impl::vqm_mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, *static_cast<const rtm::vqmf*>(transform), out_result_xxxx, out_result_yyyy, out_result_zzzz);
		}
	};
}

TEST_CASE("transform adapter names", "[compression][transform]")
{
	const vqf_transform_adapter vq_adapter;
	const vqsf_transform_adapter vqs_adapter;
	const vqvf_transform_adapter vqv_adapter;
	const vqmf_transform_adapter vqm_adapter;
	const matrix3x4f_transform_adapter matrix_adapter;

	const itransform_adapter* adapters[] = { &vq_adapter, &vqs_adapter, &vqv_adapter, &vqm_adapter, &matrix_adapter };
	const uint32_t num_adapters = uint32_t(sizeof(adapters) / sizeof(adapters[0]));

	// Each built-in adapter has a different hash
	for (uint32_t adapter_index0 = 0; adapter_index0 < num_adapters; ++adapter_index0)
	{
		for (uint32_t adapter_index1 = adapter_index0 + 1; adapter_index1 < num_adapters; ++adapter_index1)
			CHECK(adapters[adapter_index0]->get_hash() != adapters[adapter_index1]->get_hash());
	}

	const custom_vqm_transform_adapter custom_adapter;
	CHECK(custom_adapter.get_type() == transform_runtime_type8::custom);
	CHECK(custom_adapter.needs_conversion());
}

TEST_CASE("vqf transform adapter", "[compression][transform]")
{
	const vqf_transform_adapter adapter;
	check_builtin_adapter(adapter, transform_runtime_type8::vqf, true, false);

	// The VQ adapter does not use the scale
	rtm::vqvf transforms[k_num_transforms];
	make_transforms(false, transforms);

	const points_x4 points = transform_points(adapter, &transforms[0]);
	for (uint32_t point_index = 0; point_index < 4; ++point_index)
	{
		const rtm::vector4f expected_point = rtm::vqv_mul_point3_no_scale(get_point(point_index), transforms[0]);
		CHECK(rtm::vector_all_near_equal3(points.get_point(point_index), expected_point, 1.0E-5F));
	}

	rtm::vqvf product;
	adapter.mul(&transforms[0], &transforms[1], &product);
	const rtm::vqvf expected_product = rtm::vqv_normalize(rtm::vqv_mul_no_scale(transforms[0], transforms[1]));
	CHECK(rtm::quat_near_equal(product.rotation, expected_product.rotation, k_rounding_threshold));
	CHECK(rtm::vector_all_near_equal3(product.translation, expected_product.translation, k_rounding_threshold));
}

TEST_CASE("vqsf transform adapter", "[compression][transform]")
{
	const vqsf_transform_adapter adapter;
	check_builtin_adapter(adapter, transform_runtime_type8::vqsf, true, true);

	rtm::vqvf transforms_vqv[k_num_transforms];
	make_transforms(true, transforms_vqv);

	rtm::vqsf transforms[k_num_transforms];
	convert(adapter, transforms_vqv, transforms);

	// The conversion keeps the uniform scale in W
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		CHECK(rtm::quat_near_equal(transforms[transform_index].rotation, transforms_vqv[transform_index].rotation, 0.0F));
		CHECK(rtm::vector_all_near_equal3(transforms[transform_index].translation_scale, transforms_vqv[transform_index].translation, 0.0F));
		CHECK(float(rtm::vector_get_w(transforms[transform_index].translation_scale)) == float(rtm::vector_get_x(transforms_vqv[transform_index].scale)));
	}

	const points_x4 points = transform_points(adapter, &transforms[0]);
	for (uint32_t point_index = 0; point_index < 4; ++point_index)
	{
		const rtm::vector4f expected_point = rtm::vqs_mul_point3(get_point(point_index), transforms[0]);
		CHECK(rtm::vector_all_near_equal3(points.get_point(point_index), expected_point, 1.0E-5F));

		// With a uniform scale, VQS and VQV give the same point
		const rtm::vector4f vqv_point = rtm::vqv_mul_point3(get_point(point_index), transforms_vqv[0]);
		CHECK(rtm::vector_all_near_equal3(points.get_point(point_index), vqv_point, 1.0E-5F));
	}
}

TEST_CASE("vqvf transform adapter", "[compression][transform]")
{
	const vqvf_transform_adapter adapter;
	check_builtin_adapter(adapter, transform_runtime_type8::vqvf, false, false);

	rtm::vqvf transforms[k_num_transforms];
	make_transforms(false, transforms);

	const points_x4 points = transform_points(adapter, &transforms[0]);
	for (uint32_t point_index = 0; point_index < 4; ++point_index)
	{
		const rtm::vector4f expected_point = rtm::vqv_mul_point3(get_point(point_index), transforms[0]);
		CHECK(rtm::vector_all_near_equal3(points.get_point(point_index), expected_point, 1.0E-5F));
	}

	// The product is the RTM product, it discards the shear
	rtm::vqvf product;
	adapter.mul(&transforms[0], &transforms[1], &product);
	const rtm::vqvf expected_product = rtm::vqv_normalize(rtm::vqv_mul(transforms[0], transforms[1]));
	CHECK(rtm::quat_near_equal(product.rotation, expected_product.rotation, k_rounding_threshold));
	CHECK(rtm::vector_all_near_equal3(product.translation, expected_product.translation, k_rounding_threshold));
	CHECK(rtm::vector_all_near_equal3(product.scale, expected_product.scale, k_rounding_threshold));
}

TEST_CASE("vqmf transform adapter", "[compression][transform]")
{
	const vqmf_transform_adapter adapter;
	check_builtin_adapter(adapter, transform_runtime_type8::vqmf, true, false);

	rtm::vqvf transforms_vqv[k_num_transforms];
	make_transforms(false, transforms_vqv);

	rtm::vqmf transforms[k_num_transforms];
	convert(adapter, transforms_vqv, transforms);

	const points_x4 points = transform_points(adapter, &transforms[0]);
	for (uint32_t point_index = 0; point_index < 4; ++point_index)
	{
		const rtm::vector4f expected_point = rtm::vqm_mul_point3(get_point(point_index), transforms[0]);
		CHECK(rtm::vector_all_near_equal3(points.get_point(point_index), expected_point, 1.0E-5F));

		// Without a parent, VQM and VQV give the same point
		const rtm::vector4f vqv_point = rtm::vqv_mul_point3(get_point(point_index), transforms_vqv[0]);
		CHECK(rtm::vector_all_near_equal3(points.get_point(point_index), vqv_point, 1.0E-5F));
	}

	// The default 'local_to_object_space' gives the same transforms as the built-in one
	const custom_vqm_transform_adapter custom_adapter;

	itransform_adapter::local_to_object_space_args args;
	args.dirty_transform_indices = &k_dirty_indices[0];
	args.num_dirty_transforms = k_num_transforms;
	args.parent_transform_indices = &k_parent_indices[0];
	args.local_transforms = &transforms[0];
	args.num_transforms = k_num_transforms;

	rtm::vqmf object_transforms[k_num_transforms];
	adapter.local_to_object_space(args, &object_transforms[0]);

	rtm::vqmf custom_object_transforms[k_num_transforms];
	custom_adapter.local_to_object_space(args, &custom_object_transforms[0]);

	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		check_same_points(transform_points(adapter, &object_transforms[transform_index]), transform_points(custom_adapter, &custom_object_transforms[transform_index]), k_rounding_threshold);
}

TEST_CASE("matrix3x4f transform adapter", "[compression][transform]")
{
	const matrix3x4f_transform_adapter adapter;
	check_builtin_adapter(adapter, transform_runtime_type8::matrix3x4f, true, false);

	rtm::vqvf transforms_vqv[k_num_transforms];
	make_transforms(false, transforms_vqv);

	rtm::matrix3x4f transforms[k_num_transforms];
	convert(adapter, transforms_vqv, transforms);

	// Each lane uses the same operations as rtm::matrix_mul_point3
	const points_x4 points = transform_points(adapter, &transforms[0]);
	for (uint32_t point_index = 0; point_index < 4; ++point_index)
	{
		const rtm::vector4f expected_point = rtm::matrix_mul_point3(get_point(point_index), transforms[0]);
		CHECK(rtm::vector_all_near_equal3(points.get_point(point_index), expected_point, k_rounding_threshold));
	}
}
