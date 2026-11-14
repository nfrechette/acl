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

#include <acl/compression/compress.h>
#include <acl/compression/track_array.h>
#include <acl/compression/track_error.h>
#include <acl/compression/transform_adapters.h>
#include <acl/compression/transform_metrics.h>
#include <acl/core/ansi_allocator.h>
#include <acl/decompression/decompress.h>
#include <acl/math/vqmf.h>

#include <rtm/matrix3x4f.h>
#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/vqmf.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <utility>

using namespace acl;

namespace
{
	constexpr uint32_t k_num_transforms = 3;

	// A chain of three transforms with rotation and non-uniform scale
	// The product of these transforms contains shear
	void get_local_transforms(rtm::vqvf* out_raw_transforms, rtm::vqvf* out_lossy_transforms)
	{
		out_raw_transforms[0] = rtm::vqv_set(rtm::quat_from_euler(0.1F, 0.5F, 1.2F), rtm::vector_set(0.0F, 0.6F, 2.3F), rtm::vector_set(1.4F, 2.1F, 0.6F));
		out_raw_transforms[1] = rtm::vqv_set(rtm::quat_from_euler(1.1F, 0.4F, 0.7F), rtm::vector_set(1.2F, 0.3F, -0.5F), rtm::vector_set(0.5F, 1.7F, 1.1F));
		out_raw_transforms[2] = rtm::vqv_set(rtm::quat_from_euler(0.3F, 1.3F, 0.2F), rtm::vector_set(-0.4F, 2.0F, 0.8F), rtm::vector_set(2.2F, 0.9F, 1.3F));

		// The lossy transforms have a small error on each part
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const rtm::vqvf& raw_transform = out_raw_transforms[transform_index];
			const rtm::quatf rotation = rtm::quat_normalize(rtm::quat_mul(raw_transform.rotation, rtm::quat_from_euler(0.002F, -0.001F, 0.003F)));
			const rtm::vector4f translation = rtm::vector_add(raw_transform.translation, rtm::vector_set(0.001F, -0.002F, 0.0015F));
			const rtm::vector4f scale = rtm::vector_mul(raw_transform.scale, rtm::vector_set(1.001F, 0.998F, 1.002F));
			out_lossy_transforms[transform_index] = rtm::vqv_set(rotation, translation, scale);
		}
	}

	// Measures the object space error of each transform with a transform adapter and a transform metric
	void calculate_object_space_errors(const itransform_adapter& adapter, const itransform_metric& metric, float* out_errors)
	{
		rtm::vqvf raw_local_transforms[k_num_transforms];
		rtm::vqvf lossy_local_transforms[k_num_transforms];
		get_local_transforms(raw_local_transforms, lossy_local_transforms);

		const uint32_t dirty_transform_indices[k_num_transforms] = { 0, 1, 2 };
		const uint32_t parent_transform_indices[k_num_transforms] = { k_invalid_track_index, 0, 1 };

		// Big enough for 3 transforms of any built-in transform adapter
		// We zero the buffers: GCC cannot prove that a parent transform is written before its children read it
		alignas(64) uint8_t raw_local_converted[1024] = { 0 };
		alignas(64) uint8_t lossy_local_converted[1024] = { 0 };
		alignas(64) uint8_t raw_object_transforms[1024] = { 0 };
		alignas(64) uint8_t lossy_object_transforms[1024] = { 0 };

		const size_t transform_size = adapter.get_transform_size();
		REQUIRE(transform_size * k_num_transforms <= sizeof(raw_local_converted));

		const void* raw_local_transforms_ = raw_local_transforms;
		const void* lossy_local_transforms_ = lossy_local_transforms;

		if (adapter.needs_conversion())
		{
			itransform_adapter::convert_transforms_args convert_transforms_args;
			convert_transforms_args.dirty_transform_indices = dirty_transform_indices;
			convert_transforms_args.num_dirty_transforms = k_num_transforms;
			convert_transforms_args.transforms = raw_local_transforms;
			convert_transforms_args.num_transforms = k_num_transforms;
			convert_transforms_args.sample_index = 0;
			convert_transforms_args.is_lossy = false;
			adapter.convert_transforms(convert_transforms_args, raw_local_converted);

			convert_transforms_args.transforms = lossy_local_transforms;
			adapter.convert_transforms(convert_transforms_args, lossy_local_converted);

			raw_local_transforms_ = raw_local_converted;
			lossy_local_transforms_ = lossy_local_converted;
		}

		itransform_adapter::local_to_object_space_args local_to_object_space_args;
		local_to_object_space_args.dirty_transform_indices = dirty_transform_indices;
		local_to_object_space_args.num_dirty_transforms = k_num_transforms;
		local_to_object_space_args.parent_transform_indices = parent_transform_indices;
		local_to_object_space_args.local_transforms = raw_local_transforms_;
		local_to_object_space_args.num_transforms = k_num_transforms;
		adapter.local_to_object_space(local_to_object_space_args, raw_object_transforms);

		local_to_object_space_args.local_transforms = lossy_local_transforms_;
		adapter.local_to_object_space(local_to_object_space_args, lossy_object_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			itransform_metric::calculate_error_args calculate_error_args;
			calculate_error_args.construct_sphere_shell(3.0F);
			calculate_error_args.adapter = &adapter;
			calculate_error_args.transform0 = raw_object_transforms + (transform_index * transform_size);
			calculate_error_args.transform1 = lossy_object_transforms + (transform_index * transform_size);

			out_errors[transform_index] = rtm::scalar_cast(metric.calculate_error(calculate_error_args));
		}
	}
}

TEST_CASE("transform adapters keep the shear in object space", "[compression][transform]")
{
	const vqmf_transform_adapter vqm_adapter;
	const matrix3x4f_transform_adapter matrix_adapter;
	const vqvf_transform_adapter vqv_adapter;
	const shell_transform_metric metric;

	float vqm_errors[k_num_transforms];
	float matrix_errors[k_num_transforms];
	float vqv_errors[k_num_transforms];
	calculate_object_space_errors(vqm_adapter, metric, vqm_errors);
	calculate_object_space_errors(matrix_adapter, metric, matrix_errors);
	calculate_object_space_errors(vqv_adapter, metric, vqv_errors);

	// Both VQM and matrix arithmetic keep the shear, they must agree
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		CHECK(vqm_errors[transform_index] > 0.0F);
		CHECK(rtm::scalar_near_equal(vqm_errors[transform_index], matrix_errors[transform_index], 1.0E-4F));
	}

	// The root has no parent, there is no shear and all three must agree
	CHECK(rtm::scalar_near_equal(vqm_errors[0], vqv_errors[0], 1.0E-4F));
}

TEST_CASE("vqm_mul_point3_x4", "[math][vqm]")
{
	const rtm::quatf rotation = rtm::quat_from_euler(0.4F, 1.1F, -0.6F);
	const rtm::vector4f translation = rtm::vector_set(1.0F, -2.0F, 3.0F);

	// A VQM transform with shear
	const rtm::matrix3x3f scale_shear = rtm::matrix_set(
		rtm::vector_set(1.5F, 0.2F, -0.3F),
		rtm::vector_set(0.1F, 0.8F, 0.4F),
		rtm::vector_set(-0.2F, 0.3F, 2.1F));

	// The second transform has no scale: it holds the identity flag and uses the fast path
	const rtm::vqmf transforms[2] =
	{
		rtm::vqm_set(rotation, translation, scale_shear),
		rtm::vqm_set(rotation, translation, rtm::vector_set(1.0F)),
	};

	const rtm::vector4f point0 = rtm::vector_set(3.0F, 0.0F, 0.0F);
	const rtm::vector4f point1 = rtm::vector_set(0.0F, 3.0F, 0.0F);
	const rtm::vector4f point2 = rtm::vector_set(0.0F, 0.0F, 3.0F);
	const rtm::vector4f point3 = rtm::vector_set(1.2F, -0.7F, 2.5F);
	const rtm::vector4f points[4] = { point0, point1, point2, point3 };

	rtm::vector4f points_xxxx;
	rtm::vector4f points_yyyy;
	rtm::vector4f points_zzzz;
	RTM_MATRIXF_TRANSPOSE_4X3(point0, point1, point2, point3, points_xxxx, points_yyyy, points_zzzz);

	for (const rtm::vqmf& transform : transforms)
	{
		rtm::vector4f result_xxxx;
		rtm::vector4f result_yyyy;
		rtm::vector4f result_zzzz;
		acl_impl::vqm_mul_point3_x4(points_xxxx, points_yyyy, points_zzzz, transform, result_xxxx, result_yyyy, result_zzzz);

		const float result_x[4] = { rtm::vector_get_x(result_xxxx), rtm::vector_get_y(result_xxxx), rtm::vector_get_z(result_xxxx), rtm::vector_get_w(result_xxxx) };
		const float result_y[4] = { rtm::vector_get_x(result_yyyy), rtm::vector_get_y(result_yyyy), rtm::vector_get_z(result_yyyy), rtm::vector_get_w(result_yyyy) };
		const float result_z[4] = { rtm::vector_get_x(result_zzzz), rtm::vector_get_y(result_zzzz), rtm::vector_get_z(result_zzzz), rtm::vector_get_w(result_zzzz) };

		for (uint32_t point_index = 0; point_index < 4; ++point_index)
		{
			const rtm::vector4f expected = rtm::vqm_mul_point3(points[point_index], transform);
			CHECK(rtm::scalar_near_equal(result_x[point_index], rtm::vector_get_x(expected), 1.0E-4F));
			CHECK(rtm::scalar_near_equal(result_y[point_index], rtm::vector_get_y(expected), 1.0E-4F));
			CHECK(rtm::scalar_near_equal(result_z[point_index], rtm::vector_get_z(expected), 1.0E-4F));
		}
	}
}

// This test uses the deprecated legacy error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

TEST_CASE("compression with the default error metric", "[compression][error_metric]")
{
	ansi_allocator allocator;

	const uint32_t num_samples = 20;
	const float sample_rate = 30.0F;

	track_array_vqvf track_list(allocator, k_num_transforms);
	track_array_vqvf base_track_list(allocator, k_num_transforms);

	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		track_desc_transformf desc;
		desc.output_index = transform_index;
		desc.parent_index = transform_index == 0 ? k_invalid_track_index : (transform_index - 1);
		desc.precision = 0.001F;
		desc.shell_distance = 3.0F;

		// Animated rotation, translation, and non-uniform scale
		track_vqvf track = track_vqvf::make_reserve(desc, allocator, num_samples, sample_rate);
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const float alpha = float(sample_index) / float(num_samples - 1);
			const float offset = float(transform_index) * 0.3F;
			track[sample_index].rotation = rtm::quat_from_euler(alpha + offset, 0.5F * alpha, 0.2F + offset);
			track[sample_index].translation = rtm::vector_set(1.0F + alpha, offset, 2.0F * alpha);
			track[sample_index].scale = rtm::vector_set(1.0F + alpha, 1.5F - (0.5F * alpha), 0.8F + offset);
		}
		track_list[transform_index] = std::move(track);

		// A base pose with scale for the additive clip
		const float base_offset = float(transform_index) * 0.2F;
		const rtm::vqvf base_transform = rtm::vqv_set(rtm::quat_from_euler(0.2F, base_offset, 0.1F), rtm::vector_set(0.5F, 0.0F, 0.2F), rtm::vector_set(1.2F, 0.9F, 1.1F));
		base_track_list[transform_index] = track_vqvf::make_copy(desc, allocator, &base_transform, 1, sample_rate);
	}

	const additive_clip_format8 additive_formats[] = { additive_clip_format8::none, additive_clip_format8::additive1 };
	for (const additive_clip_format8 additive_format : additive_formats)
	{
		compression_settings settings = get_default_compression_settings();
		REQUIRE(settings.error_metric == nullptr);
		REQUIRE(settings.is_valid().empty());

		const bool is_additive = additive_format != additive_clip_format8::none;

		transform_base_desc_t transform_base;
		if (is_additive)
		{
			transform_base.format = additive_format;
			transform_base.tracks = &base_track_list;
		}

		output_stats stats;
		compressed_tracks* compressed_tracks_ = nullptr;
		error_result result;
		if (is_additive)
			result = compress_track_list(allocator, track_list, settings, transform_base, compressed_tracks_, stats);
		else
			result = compress_track_list(allocator, track_list, settings, compressed_tracks_, stats);

		REQUIRE(result.empty());
		REQUIRE(compressed_tracks_ != nullptr);
		CHECK(compressed_tracks_->is_valid(true).empty());

		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*compressed_tracks_));

		// We measure the error as compression does with these settings
		track_error error;
		if (is_additive)
			error = calculate_compression_error(allocator, track_list, context, settings, transform_base);
		else
			error = calculate_compression_error(allocator, track_list, context, settings);

		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);

		allocator.deallocate(compressed_tracks_, compressed_tracks_->get_size());
	}
}

ACL_IMPL_DEPRECATION_WARNING_POP
