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
#include <acl/compression/pre_process.h>
#include <acl/compression/track_array.h>
#include <acl/compression/track_error.h>
#include <acl/compression/transform_adapters.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/compression/transform_metrics.h>
#include <acl/core/ansi_allocator.h>
#include <acl/decompression/decompress.h>

#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/vqmf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

using namespace acl;

namespace
{
	constexpr uint32_t k_num_transforms = 5;
	constexpr uint32_t k_num_samples = 40;
	constexpr float k_sample_rate = 30.0F;
	constexpr float k_precision = 0.001F;

	enum class scale_kind
	{
		none,
		uniform,
		non_uniform,
	};

	track_desc_transformf make_desc(uint32_t transform_index)
	{
		track_desc_transformf desc;
		desc.output_index = transform_index;
		desc.parent_index = transform_index == 0 ? k_invalid_track_index : (transform_index - 1);
		desc.precision = k_precision;
		desc.shell_distance = 3.0F;
		return desc;
	}

	rtm::vector4f get_scale(scale_kind kind, uint32_t transform_index, float alpha)
	{
		const float offset = float(transform_index) * 0.3F;
		switch (kind)
		{
		case scale_kind::none:		return rtm::vector_set(1.0F);
		case scale_kind::uniform:	return rtm::vector_set(1.0F + 0.5F * alpha + 0.1F * offset);
		default:					return rtm::vector_set(1.0F + alpha, 1.5F - (0.5F * alpha), 0.8F + offset);
		}
	}

	// A chain of transforms with animated rotations, translations, and scales
	track_array_vqvf make_vqv_clip(iallocator& allocator, scale_kind kind)
	{
		track_array_vqvf track_list(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			track_vqvf track = track_vqvf::make_reserve(make_desc(transform_index), allocator, k_num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const float alpha = float(sample_index) / float(k_num_samples - 1);
				const float offset = float(transform_index) * 0.3F;
				track[sample_index].rotation = rtm::quat_from_euler(alpha + offset, 0.5F * alpha, 0.2F + offset);
				track[sample_index].translation = rtm::vector_set(1.0F + alpha, offset, 2.0F * alpha);
				track[sample_index].scale = get_scale(kind, transform_index, alpha);
			}

			track_list[transform_index] = std::move(track);
		}

		return track_list;
	}

	// The same clip as 'make_vqv_clip' with a uniform scale, as VQS tracks
	track_array_vqsf make_vqs_clip(iallocator& allocator)
	{
		const track_array_vqvf vqv_track_list = make_vqv_clip(allocator, scale_kind::uniform);
		track_array_vqsf track_list(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const track_vqvf& vqv_track = vqv_track_list[transform_index];
			track_vqsf track = track_vqsf::make_reserve(make_desc(transform_index), allocator, k_num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& sample = vqv_track[sample_index];
				const float scale = rtm::vector_get_x(sample.scale);
				track[sample_index] = rtm::vqs_set(sample.rotation, sample.translation, scale);
			}

			track_list[transform_index] = std::move(track);
		}

		return track_list;
	}

	// A base pose with a non-uniform scale for the additive clips
	track_array_vqvf make_base(iallocator& allocator)
	{
		track_array_vqvf base_track_list(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const float base_offset = float(transform_index) * 0.2F;
			const rtm::vqvf base_transform = rtm::vqv_set(rtm::quat_from_euler(0.2F, base_offset, 0.1F), rtm::vector_set(0.5F, 0.0F, 0.2F), rtm::vector_set(1.2F, 0.9F, 1.1F));
			base_track_list[transform_index] = track_vqvf::make_copy(make_desc(transform_index), allocator, &base_transform, 1, k_sample_rate);
		}

		return base_track_list;
	}

	compressed_tracks* compress(iallocator& allocator, const track_array& track_list, const compression_settings& settings, const track_array* base_track_list, additive_clip_format8 additive_format)
	{
		output_stats stats;
		compressed_tracks* compressed_tracks_ = nullptr;

		error_result result;
		if (base_track_list != nullptr)
			result = compress_track_list(allocator, track_list, settings, *base_track_list, additive_format, compressed_tracks_, stats);
		else
			result = compress_track_list(allocator, track_list, settings, compressed_tracks_, stats);

		REQUIRE(result.empty());
		REQUIRE(compressed_tracks_ != nullptr);
		REQUIRE(compressed_tracks_->is_valid(true).empty());
		return compressed_tracks_;
	}

	// Measures the error as compression does with these settings
	float measure_error(iallocator& allocator, const track_array& track_list, const compressed_tracks& tracks, const compression_settings& settings, const track_array* base_track_list, additive_clip_format8 additive_format)
	{
		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(tracks));

		const track_error error = base_track_list != nullptr
			? calculate_compression_error(allocator, track_list, context, settings, *base_track_list, additive_format)
			: calculate_compression_error(allocator, track_list, context, settings);

		return error.error;
	}

	// The legacy error metrics are deprecated
	ACL_IMPL_DEPRECATION_WARNING_PUSH
	itransform_error_metric* create_legacy_vqvf_metric(iallocator& allocator, additive_clip_format8 additive_format)
	{
		switch (additive_format)
		{
		case additive_clip_format8::additive1:		return allocate_type<additive_vqvf_transform_error_metric<additive_clip_format8::additive1>>(allocator);
		case additive_clip_format8::none:
		default:									return allocate_type<vqvf_transform_error_metric>(allocator);
		}
	}
	ACL_IMPL_DEPRECATION_WARNING_POP

	// A custom adapter that checks the arguments of each conversion, then forwards to the VQM adapter
	// A raw transform must be the raw sample at its transform index and at its sample index in the clip
	struct checking_adapter final : public itransform_adapter
	{
		vqmf_transform_adapter builtin_adapter;
		const track_array_vqvf* raw_tracks = nullptr;

		mutable uint32_t num_raw_conversions = 0;
		mutable uint32_t num_lossy_conversions = 0;
		mutable uint32_t num_invalid_arguments = 0;
		mutable uint32_t num_raw_mismatches = 0;
		mutable uint32_t max_sample_index = 0;

		virtual const char* get_name() const override { return "checking_adapter"; }
		virtual size_t get_transform_size() const override { return builtin_adapter.get_transform_size(); }
		virtual bool is_associative() const override { return builtin_adapter.is_associative(); }
		virtual void identity(void* out_transform) const override { builtin_adapter.identity(out_transform); }
		virtual void mul(const void* lhs, const void* rhs, void* out_transform) const override { builtin_adapter.mul(lhs, rhs, out_transform); }

		virtual void RTM_SIMD_CALL mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			const void* transform,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) const override
		{
			builtin_adapter.mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, transform, out_result_xxxx, out_result_yyyy, out_result_zzzz);
		}

		virtual void convert_transforms(const convert_transforms_args& args, void* out_transforms) const override
		{
			const uint32_t num_tracks = raw_tracks->get_num_tracks();
			const uint32_t num_samples = raw_tracks->get_num_samples_per_track();

			if (args.num_transforms != num_tracks || args.sample_index >= num_samples)
				num_invalid_arguments++;
			else
				max_sample_index = std::max(max_sample_index, args.sample_index);

			for (uint32_t dirty_transform_index = 0; dirty_transform_index < args.num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = args.dirty_transform_indices[dirty_transform_index];
				if (transform_index >= args.num_transforms || transform_index >= num_tracks || args.sample_index >= num_samples)
				{
					num_invalid_arguments++;
					continue;
				}

				if (args.is_lossy)
				{
					num_lossy_conversions++;
					continue;
				}

				num_raw_conversions++;

				const track_vqvf& raw_track = (*raw_tracks)[transform_index];
				const rtm::vqvf& expected = raw_track[args.sample_index];
				const rtm::vqvf& actual = args.transforms[transform_index];

				const bool is_same_rotation = rtm::scalar_abs(rtm::quat_dot(expected.rotation, actual.rotation)) >= 0.99999F;
				const bool is_same_translation = rtm::vector_all_near_equal3(expected.translation, actual.translation, 1.0E-4F);
				const bool is_same_scale = rtm::vector_all_near_equal3(expected.scale, actual.scale, 1.0E-4F);
				if (!is_same_rotation || !is_same_translation || !is_same_scale)
					num_raw_mismatches++;
			}

			builtin_adapter.convert_transforms(args, out_transforms);
		}
	};

	// A custom adapter: VQM math through the interface, with the default 'local_to_object_space'
	struct custom_vqm_adapter final : public itransform_adapter
	{
		virtual const char* get_name() const override { return "custom_vqm_adapter"; }
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

		virtual void identity(void* out_transform) const override { *static_cast<rtm::vqmf*>(out_transform) = rtm::vqm_identity(); }

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

	// A custom adapter that forwards each function to a built-in adapter
	// ACL compiles the built-in adapter code with its floating point settings (no FMA contraction)
	// Thus compression gives the exact same result with this adapter and with the built-in adapter
	template<class builtin_adapter_t>
	struct forwarding_adapter final : public itransform_adapter
	{
		builtin_adapter_t builtin_adapter;

		virtual const char* get_name() const override { return "forwarding_adapter"; }
		virtual size_t get_transform_size() const override { return builtin_adapter.get_transform_size(); }
		virtual bool is_associative() const override { return builtin_adapter.is_associative(); }
		virtual bool needs_conversion() const override { return builtin_adapter.needs_conversion(); }
		virtual void convert_transforms(const convert_transforms_args& args, void* out_transforms) const override { builtin_adapter.convert_transforms(args, out_transforms); }
		virtual void identity(void* out_transform) const override { builtin_adapter.identity(out_transform); }
		virtual void mul(const void* lhs, const void* rhs, void* out_transform) const override { builtin_adapter.mul(lhs, rhs, out_transform); }

		virtual void RTM_SIMD_CALL mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			const void* transform,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) const override
		{
			builtin_adapter.mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, transform, out_result_xxxx, out_result_yyyy, out_result_zzzz);
		}
	};

	// A custom adapter: VQV math through the interface, the product is not associative
	// It uses rtm::vqvf, thus it needs no conversion
	struct custom_vqv_adapter final : public itransform_adapter
	{
		virtual const char* get_name() const override { return "custom_vqv_adapter"; }
		virtual size_t get_transform_size() const override { return sizeof(rtm::vqvf); }
		virtual bool is_associative() const override { return false; }
		virtual bool needs_conversion() const override { return false; }

		virtual void identity(void* out_transform) const override { *static_cast<rtm::vqvf*>(out_transform) = rtm::vqv_identity(); }

		virtual void mul(const void* lhs, const void* rhs, void* out_transform) const override
		{
			*static_cast<rtm::vqvf*>(out_transform) = rtm::vqv_normalize(rtm::vqv_mul(*static_cast<const rtm::vqvf*>(lhs), *static_cast<const rtm::vqvf*>(rhs)));
		}

		virtual void RTM_SIMD_CALL mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			const void* transform,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) const override
		{
			acl_impl::vqv_mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, *static_cast<const rtm::vqvf*>(transform), out_result_xxxx, out_result_yyyy, out_result_zzzz);
		}
	};
}

// This test uses the deprecated legacy error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

TEST_CASE("transform runtime compression matches the legacy error metrics", "[compression][transform]")
{
	ansi_allocator allocator;

	const track_array_vqvf track_list = make_vqv_clip(allocator, scale_kind::non_uniform);
	const track_array_vqvf base_track_list = make_base(allocator);

	shell_transform_metric shell_metric;

	const additive_clip_format8 additive_formats[] = { additive_clip_format8::none, additive_clip_format8::additive1 };
	for (const additive_clip_format8 additive_format : additive_formats)
	{
		const track_array* base_track_list_ptr = additive_format != additive_clip_format8::none ? &base_track_list : nullptr;

		// VQV with the shell metric gives the same result as the legacy VQV error metric
		{
			itransform_error_metric* legacy_metric = create_legacy_vqvf_metric(allocator, additive_format);

			compression_settings legacy_settings = get_default_compression_settings();
			legacy_settings.error_metric = legacy_metric;

			compression_settings settings = get_default_compression_settings();
			settings.transform_runtime_type = transform_runtime_type_t::vqvf;
			settings.transform_metric = &shell_metric;

			compressed_tracks* legacy_tracks = compress(allocator, track_list, legacy_settings, base_track_list_ptr, additive_format);
			compressed_tracks* tracks = compress(allocator, track_list, settings, base_track_list_ptr, additive_format);

			CHECK(tracks->get_size() == legacy_tracks->get_size());
			CHECK(tracks->get_hash() == legacy_tracks->get_hash());

			allocator.deallocate(legacy_tracks, legacy_tracks->get_size());
			allocator.deallocate(tracks, tracks->get_size());
			deallocate_type(allocator, legacy_metric);
		}
	}
}

ACL_IMPL_DEPRECATION_WARNING_POP

TEST_CASE("transform runtime compression keeps the precision", "[compression][transform]")
{
	ansi_allocator allocator;

	const track_array_vqvf base_track_list = make_base(allocator);
	custom_vqm_adapter custom_adapter;
	shell_transform_metric shell_metric;

	struct test_case
	{
		scale_kind kind;
		transform_runtime_type_t type;
		itransform_adapter* custom_adapter;
		itransform_metric* metric;
	};

	const test_case test_cases[] =
	{
		// Without scale: VQ with the precise metric
		{ scale_kind::none, transform_runtime_type_t::automatic, nullptr, nullptr },
		{ scale_kind::none, transform_runtime_type_t::vqmf, nullptr, nullptr },
		// With scale: VQM, VQV, and matrices with the shell metric
		{ scale_kind::non_uniform, transform_runtime_type_t::automatic, nullptr, nullptr },
		{ scale_kind::non_uniform, transform_runtime_type_t::vqvf, nullptr, nullptr },
		{ scale_kind::non_uniform, transform_runtime_type_t::matrix3x4f, nullptr, nullptr },
		// A custom adapter uses the bit rate algorithm v1
		{ scale_kind::non_uniform, transform_runtime_type_t::custom, &custom_adapter, nullptr },
		{ scale_kind::none, transform_runtime_type_t::custom, &custom_adapter, &shell_metric },
	};

	const additive_clip_format8 additive_formats[] = { additive_clip_format8::none, additive_clip_format8::additive0 };
	for (const additive_clip_format8 additive_format : additive_formats)
	{
		const track_array* base_track_list_ptr = additive_format != additive_clip_format8::none ? &base_track_list : nullptr;

		for (const test_case& case_ : test_cases)
		{
			const track_array_vqvf track_list = make_vqv_clip(allocator, case_.kind);

			compression_settings settings = get_default_compression_settings();
			settings.transform_runtime_type = case_.type;
			settings.custom_transform_adapter = case_.custom_adapter;
			settings.transform_metric = case_.metric;

			compressed_tracks* tracks = compress(allocator, track_list, settings, base_track_list_ptr, additive_format);

			const float error = measure_error(allocator, track_list, *tracks, settings, base_track_list_ptr, additive_format);
			CHECK(error >= 0.0F);
			CHECK(error < 0.01F);

			allocator.deallocate(tracks, tracks->get_size());
		}
	}
}

TEST_CASE("transform runtime compression gives the same result with virtual functions", "[compression][transform]")
{
	ansi_allocator allocator;

	const track_array_vqvf track_list = make_vqv_clip(allocator, scale_kind::non_uniform);
	const track_array_vqvf base_track_list = make_base(allocator);

	// The custom adapters forward to the built-in adapters: the custom transform type uses the same math
	// We cannot use 'custom_vqm_adapter' here: the test code can use FMA contraction, and the result can change by one bit
	forwarding_adapter<vqmf_transform_adapter> custom_vqm;
	forwarding_adapter<vqvf_transform_adapter> custom_vqv;

	// A transform metric that ACL does not own: compression uses its virtual functions
	shell_transform_metric shell_metric;

	struct test_case
	{
		transform_runtime_type_t builtin_type;
		itransform_adapter* custom_adapter;
	};

	// A built-in type with the built-in metric uses code without virtual functions
	// A custom adapter with the same math uses the virtual functions, the result is the same
	const test_case test_cases[] =
	{
		{ transform_runtime_type_t::vqmf, &custom_vqm },
		{ transform_runtime_type_t::vqvf, &custom_vqv },
	};

	const additive_clip_format8 additive_formats[] = { additive_clip_format8::none, additive_clip_format8::additive0 };
	for (const additive_clip_format8 additive_format : additive_formats)
	{
		const track_array* base_track_list_ptr = additive_format != additive_clip_format8::none ? &base_track_list : nullptr;

		for (const test_case& case_ : test_cases)
		{
			compression_settings builtin_settings = get_default_compression_settings();
			builtin_settings.transform_runtime_type = case_.builtin_type;

			compression_settings builtin_virtual_metric_settings = builtin_settings;
			builtin_virtual_metric_settings.transform_metric = &shell_metric;

			compression_settings custom_settings = get_default_compression_settings();
			custom_settings.transform_runtime_type = transform_runtime_type_t::custom;
			custom_settings.custom_transform_adapter = case_.custom_adapter;
			custom_settings.transform_metric = &shell_metric;

			compressed_tracks* builtin_tracks = compress(allocator, track_list, builtin_settings, base_track_list_ptr, additive_format);
			compressed_tracks* builtin_virtual_metric_tracks = compress(allocator, track_list, builtin_virtual_metric_settings, base_track_list_ptr, additive_format);
			compressed_tracks* custom_tracks = compress(allocator, track_list, custom_settings, base_track_list_ptr, additive_format);

			CHECK(builtin_virtual_metric_tracks->get_hash() == builtin_tracks->get_hash());
			CHECK(custom_tracks->get_hash() == builtin_tracks->get_hash());

			allocator.deallocate(builtin_tracks, builtin_tracks->get_size());
			allocator.deallocate(builtin_virtual_metric_tracks, builtin_virtual_metric_tracks->get_size());
			allocator.deallocate(custom_tracks, custom_tracks->get_size());
		}
	}
}

TEST_CASE("transform runtime conversion arguments", "[compression][transform]")
{
	ansi_allocator allocator;

	checking_adapter adapter;
	shell_transform_metric shell_metric;

	// The clip has more than one segment: the sample index must be the index in the clip, not in the segment
	const track_array_vqvf track_list = make_vqv_clip(allocator, scale_kind::non_uniform);

	// Pre-processing converts the samples of the track list that it changes
	{
		track_array_vqvf pre_processed_track_list = make_vqv_clip(allocator, scale_kind::non_uniform);
		adapter.raw_tracks = &pre_processed_track_list;

		pre_process_settings_t pre_process_settings;
		pre_process_settings.actions = pre_process_actions::recommended;
		pre_process_settings.precision_policy = pre_process_precision_policy::lossy;
		pre_process_settings.transform_runtime_type = transform_runtime_type_t::custom;
		pre_process_settings.custom_transform_adapter = &adapter;
		pre_process_settings.transform_metric = &shell_metric;

		CHECK(pre_process_track_list(allocator, pre_process_settings, pre_processed_track_list).empty());
	}

	// Compression and the error measurement convert the samples of the raw track list
	adapter.raw_tracks = &track_list;

	compression_settings settings = get_default_compression_settings();
	settings.transform_runtime_type = transform_runtime_type_t::custom;
	settings.custom_transform_adapter = &adapter;
	settings.transform_metric = &shell_metric;

	compressed_tracks* tracks = compress(allocator, track_list, settings, nullptr, additive_clip_format8::none);

	const float error = measure_error(allocator, track_list, *tracks, settings, nullptr, additive_clip_format8::none);
	CHECK(error < 0.01F);

	allocator.deallocate(tracks, tracks->get_size());

	CHECK(adapter.num_invalid_arguments == 0);
	CHECK(adapter.num_raw_mismatches == 0);
	CHECK(adapter.num_raw_conversions != 0);
	CHECK(adapter.num_lossy_conversions != 0);
	CHECK(adapter.max_sample_index == k_num_samples - 1);
}

TEST_CASE("transform runtime compression with vqsf tracks", "[compression][transform]")
{
	ansi_allocator allocator;

	const track_array_vqsf track_list = make_vqs_clip(allocator);
	const compression_settings settings = get_default_compression_settings();

	// 'automatic' uses VQS with the precise metric
	compressed_tracks* tracks = compress(allocator, track_list, settings, nullptr, additive_clip_format8::none);

	const float error = measure_error(allocator, track_list, *tracks, settings, nullptr, additive_clip_format8::none);
	CHECK(error >= 0.0F);
	CHECK(error < 0.01F);

	// The precise metric measures the exact error: it is not below the error of the shell metric
	shell_transform_metric shell_metric;
	compression_settings shell_settings = settings;
	shell_settings.transform_metric = &shell_metric;
	CHECK(error >= measure_error(allocator, track_list, *tracks, shell_settings, nullptr, additive_clip_format8::none) - 1.0E-6F);

	allocator.deallocate(tracks, tracks->get_size());
}

// This test uses the deprecated legacy error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

TEST_CASE("transform runtime error with the legacy error metric", "[compression][transform]")
{
	ansi_allocator allocator;

	const track_array_vqvf track_list = make_vqv_clip(allocator, scale_kind::non_uniform);

	vqvf_transform_error_metric legacy_metric;
	compression_settings settings = get_default_compression_settings();
	settings.error_metric = &legacy_metric;

	compressed_tracks* tracks = compress(allocator, track_list, settings, nullptr, additive_clip_format8::none);

	decompression_context<debug_transform_decompression_settings> context;
	REQUIRE(context.initialize(*tracks));

	// With the legacy error metric in the settings, we measure with it
	const track_error settings_error = calculate_compression_error(allocator, track_list, context, settings);
	const track_error legacy_error = calculate_compression_error(allocator, track_list, context, legacy_metric);
	CHECK(settings_error.error == legacy_error.error);
	CHECK(settings_error.index == legacy_error.index);

	allocator.deallocate(tracks, tracks->get_size());
}

ACL_IMPL_DEPRECATION_WARNING_POP
