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
#include <acl/compression/convert.h>
#include <acl/compression/pre_process.h>
#include <acl/compression/track_array.h>
#include <acl/compression/track_error.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/core/ansi_allocator.h>
#include <acl/core/track_writer.h>
#include <acl/decompression/decompress.h>

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
#include <rtm/vqf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cmath>
#include <cstdint>
#include <utility>

using namespace acl;

namespace
{
	// 40 transforms give 10 groups of 4 scale sub-tracks
	// 100 samples give more than one segment
	constexpr uint32_t k_num_transforms = 40;
	constexpr uint32_t k_num_samples = 100;
	constexpr float k_sample_rate = 30.0F;

	track_desc_transformf make_desc(uint32_t transform_index, float default_scale)
	{
		track_desc_transformf desc;
		desc.output_index = transform_index;
		desc.parent_index = transform_index == 0 ? k_invalid_track_index : ((transform_index - 1) / 2);
		desc.precision = 0.001F;
		desc.shell_distance = 3.0F;
		desc.default_value.scale = rtm::vector_set(default_scale);
		return desc;
	}

	// A tree of transforms with animated rotations and translations
	// The uniform scale of each transform depends on its index:
	//    - 0: animated
	//    - 1: constant, not the default value
	//    - 2: the default value
	//    - 3: animated with a large range
	float get_uniform_scale(uint32_t transform_index, uint32_t sample_index, float default_scale)
	{
		const float alpha = float(sample_index) / float(k_num_samples - 1);
		switch (transform_index % 4)
		{
		case 0:		return default_scale + 0.2F + 0.3F * std::sin(alpha * 6.0F + float(transform_index));
		case 1:		return default_scale + 0.3F;
		case 2:		return default_scale;
		default:	return default_scale + 0.5F + 2.0F * alpha * alpha;
		}
	}

	rtm::vqsf get_vqs_sample(uint32_t transform_index, uint32_t sample_index, float default_scale)
	{
		const float alpha = float(sample_index) / float(k_num_samples - 1);
		const float offset = float(transform_index) * 0.1F;
		const rtm::quatf rotation = rtm::quat_from_euler(alpha + offset, 0.5F * alpha, 0.2F + offset);
		const rtm::vector4f translation = rtm::vector_set(1.0F + alpha, offset, 2.0F * alpha);
		return rtm::vqs_set(rotation, translation, get_uniform_scale(transform_index, sample_index, default_scale));
	}

	// Builds the same clip as VQS tracks and as VQV tracks
	// The VQV tracks have the uniform scale in [xyz]
	void make_clips(iallocator& allocator, float default_scale, track_array_vqsf& out_vqs_tracks, track_array_vqvf& out_vqv_tracks)
	{
		out_vqs_tracks = track_array_vqsf(allocator, k_num_transforms);
		out_vqv_tracks = track_array_vqvf(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const track_desc_transformf desc = make_desc(transform_index, default_scale);

			track_vqsf vqs_track = track_vqsf::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
			track_vqvf vqv_track = track_vqvf::make_reserve(desc, allocator, k_num_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqsf sample = get_vqs_sample(transform_index, sample_index, default_scale);
				vqs_track[sample_index] = sample;
				vqv_track[sample_index] = rtm::vqv_set(sample.rotation, rtm::vector_set_w(sample.translation_scale, 0.0F), rtm::vector_dup_w(sample.translation_scale));
			}

			out_vqs_tracks[transform_index] = std::move(vqs_track);
			out_vqv_tracks[transform_index] = std::move(vqv_track);
		}
	}

	// A base pose with non-uniform scale for the additive clips
	track_array_vqvf make_base(iallocator& allocator)
	{
		track_array_vqvf base_tracks(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const float base_offset = float(transform_index) * 0.05F;
			const rtm::vqvf base_transform = rtm::vqv_set(rtm::quat_from_euler(0.2F, base_offset, 0.1F), rtm::vector_set(0.5F, 0.0F, 0.2F), rtm::vector_set(1.2F, 0.9F, 1.1F));
			base_tracks[transform_index] = track_vqvf::make_copy(make_desc(transform_index, 1.0F), allocator, &base_transform, 1, k_sample_rate);
		}

		return base_tracks;
	}

	// A track writer with the uniform scale overload, it counts the scale writes of each kind
	struct vqs_track_writer final : public track_writer
	{
		rtm::vqvf poses[k_num_transforms];
		uint32_t num_uniform_scale_writes = 0;
		uint32_t num_vector_scale_writes = 0;

		void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation) { poses[track_index].rotation = rotation; }
		void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation) { poses[track_index].translation = translation; }
		void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale) { poses[track_index].scale = scale; num_vector_scale_writes++; }
		void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::scalarf_arg0 scale) { poses[track_index].scale = rtm::vector_set(scale); num_uniform_scale_writes++; }
	};

	// A track writer without the uniform scale overload
	struct vector_scale_track_writer final : public track_writer
	{
		rtm::vqvf poses[k_num_transforms];
		uint32_t num_vector_scale_writes = 0;

		void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation) { poses[track_index].rotation = rotation; }
		void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation) { poses[track_index].translation = translation; }
		void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale) { poses[track_index].scale = scale; num_vector_scale_writes++; }
	};

	static_assert(acl_impl::has_uniform_scale_writer<vqs_track_writer>::value, "Expected the uniform scale overload");
	static_assert(!acl_impl::has_uniform_scale_writer<vector_scale_track_writer>::value, "Expected no uniform scale overload");
	static_assert(!acl_impl::has_uniform_scale_writer<track_writer>::value, "Expected no uniform scale overload");

	void compress(iallocator& allocator, const track_array& tracks, const compression_settings& settings, const track_array* base_tracks, additive_clip_format8 additive_format, compressed_tracks*& out_compressed_tracks)
	{
		output_stats stats;
		error_result result;
		if (base_tracks != nullptr)
		{
			transform_base_desc_t transform_base;
			transform_base.format = additive_format;
			transform_base.tracks = base_tracks;
			result = compress_track_list(allocator, tracks, settings, transform_base, out_compressed_tracks, stats);
		}
		else
			result = compress_track_list(allocator, tracks, settings, out_compressed_tracks, stats);

		REQUIRE(result.empty());
		REQUIRE(out_compressed_tracks != nullptr);
		REQUIRE(out_compressed_tracks->is_valid(true).empty());
	}

	void check_same_pose(const rtm::vqvf* vqs_pose, const rtm::vqvf* vqv_pose)
	{
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const rtm::vqvf& vqs_transform = vqs_pose[transform_index];
			const rtm::vqvf& vqv_transform = vqv_pose[transform_index];

			CHECK(rtm::quat_near_equal(vqs_transform.rotation, vqv_transform.rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(vqs_transform.translation, vqv_transform.translation, 0.0F));

			// The VQV clip has a uniform scale and the VQS clip has the same value
			// A VQS clip unpacks one scale value, a VQV clip unpacks each component. On x86 with x87 math
			// (for example without SIMD), a component can keep more precision than a stored float.
			// Thus the values can be different by 1 ULP. The largest scale is 3.5: 1 ULP is 2.4E-7.
			const float scale_threshold = 1.0E-6F;
			CHECK(rtm::vector_all_near_equal3(vqs_transform.scale, vqv_transform.scale, scale_threshold));
			CHECK(rtm::vector_all_near_equal3(vqv_transform.scale, rtm::vector_dup_x(vqv_transform.scale), scale_threshold));
		}
	}

	// Compresses a clip as VQS and as VQV tracks, then compares the results
	void test_vqs_compression(const compression_settings& settings, additive_clip_format8 additive_format)
	{
		ansi_allocator allocator;

		const bool is_additive = additive_format != additive_clip_format8::none;
		const float default_scale = additive_format == additive_clip_format8::additive1 ? 0.0F : 1.0F;

		track_array_vqsf vqs_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, default_scale, vqs_tracks, vqv_tracks);

		const track_array_vqvf base_tracks = make_base(allocator);
		const track_array* base_tracks_ptr = is_additive ? &base_tracks : nullptr;

		// We compare the storage of VQS and VQV tracks: both use the same runtime transform type
		// Thus compression makes the same decisions for both. With 'automatic', VQS tracks would use 'vqsf'
		compression_settings vqm_settings = settings;
		vqm_settings.transform_runtime_type = transform_runtime_type_t::vqmf;

		compressed_tracks* vqs_compressed = nullptr;
		compressed_tracks* vqv_compressed = nullptr;
		compress(allocator, vqs_tracks, vqm_settings, base_tracks_ptr, additive_format, vqs_compressed);
		compress(allocator, vqv_tracks, vqm_settings, base_tracks_ptr, additive_format, vqv_compressed);

		CHECK(vqs_compressed->get_track_type() == track_type8::vqsf);
		CHECK(vqv_compressed->get_track_type() == track_type8::vqvf);
		CHECK(vqs_compressed->get_version() == compressed_tracks_version16::latest);
		CHECK(vqs_compressed->get_num_tracks() == k_num_transforms);

		// A uniform scale stores one component instead of three
		CHECK(vqs_compressed->get_size() < vqv_compressed->get_size());

		decompression_context<debug_transform_decompression_settings> vqs_context;
		decompression_context<debug_transform_decompression_settings> vqv_context;
		REQUIRE(vqs_context.initialize(*vqs_compressed));
		REQUIRE(vqv_context.initialize(*vqv_compressed));

		vqs_track_writer vqs_writer;
		vqs_track_writer vqv_writer;
		vector_scale_track_writer vqs_vector_writer;

		const sample_rounding_policy rounding_policies[] = { sample_rounding_policy::none, sample_rounding_policy::nearest };

		// We sample between the samples as well
		const uint32_t num_sample_times = k_num_samples * 2;
		for (uint32_t sample_time_index = 0; sample_time_index < num_sample_times; ++sample_time_index)
		{
			const float sample_time = float(sample_time_index) / (k_sample_rate * 2.0F);

			for (const sample_rounding_policy rounding_policy : rounding_policies)
			{
				vqs_context.seek(sample_time, rounding_policy);
				vqs_context.decompress_tracks(vqs_writer);
				vqs_context.decompress_tracks(vqs_vector_writer);

				vqv_context.seek(sample_time, rounding_policy);
				vqv_context.decompress_tracks(vqv_writer);

				check_same_pose(vqs_writer.poses, vqv_writer.poses);
				check_same_pose(vqs_vector_writer.poses, vqv_writer.poses);

				// We also decompress one track at a time
				for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
				{
					vqs_context.decompress_track(transform_index, vqs_writer);
					vqv_context.decompress_track(transform_index, vqv_writer);
				}

				check_same_pose(vqs_writer.poses, vqv_writer.poses);
			}
		}

		// A VQS clip writes its scale with the uniform scale overload when the track writer has it
		CHECK(vqs_writer.num_uniform_scale_writes != 0);
		CHECK(vqs_writer.num_vector_scale_writes == 0);
		CHECK(vqs_vector_writer.num_vector_scale_writes != 0);
		CHECK(vqv_writer.num_uniform_scale_writes == 0);
		CHECK(vqv_writer.num_vector_scale_writes != 0);

		// The error is the same with both source types
		// We measure with the VQM runtime transform type for both source types
		compression_settings error_settings = get_default_compression_settings();
		error_settings.transform_runtime_type = transform_runtime_type_t::vqmf;

		track_error vqs_error;
		track_error vqv_error;
		if (is_additive)
		{
			transform_base_desc_t transform_base;
			transform_base.format = additive_format;
			transform_base.tracks = &base_tracks;
			vqs_error = calculate_compression_error(allocator, vqs_tracks, vqs_context, error_settings, transform_base);
			vqv_error = calculate_compression_error(allocator, vqv_tracks, vqv_context, error_settings, transform_base);
		}
		else
		{
			vqs_error = calculate_compression_error(allocator, vqs_tracks, vqs_context, error_settings);
			vqv_error = calculate_compression_error(allocator, vqv_tracks, vqv_context, error_settings);
		}

		CHECK(vqs_error.error >= 0.0F);
		CHECK(vqs_error.error < 0.01F);

		// With x87 math, the scales can be different by 1 ULP (see 'check_same_pose'), thus the errors can be different too.
		// When two samples have almost the same largest error, such a difference can change which sample has the largest error.
		// Thus we compare only the value of the largest error, with a threshold 10x smaller than the precision of the tracks.
		CHECK(rtm::scalar_near_equal(vqs_error.error, vqv_error.error, 1.0E-4F));

		allocator.deallocate(vqs_compressed, vqs_compressed->get_size());
		allocator.deallocate(vqv_compressed, vqv_compressed->get_size());
	}
}

TEST_CASE("vqsf compression", "[compression][vqsf]")
{
	test_vqs_compression(get_default_compression_settings(), additive_clip_format8::none);
}

TEST_CASE("vqsf compression with additive0", "[compression][vqsf]")
{
	test_vqs_compression(get_default_compression_settings(), additive_clip_format8::additive0);
}

TEST_CASE("vqsf compression with additive1", "[compression][vqsf]")
{
	test_vqs_compression(get_default_compression_settings(), additive_clip_format8::additive1);
}

TEST_CASE("vqsf compression with full precision scale", "[compression][vqsf]")
{
	// Variable rotations and translations, full precision scales
	compression_settings settings = get_default_compression_settings();
	settings.scale_format = vector_format8::vector3f_full;
	test_vqs_compression(settings, additive_clip_format8::none);
}

TEST_CASE("vqsf raw conversion", "[compression][vqsf]")
{
	ansi_allocator allocator;

	track_array_vqsf vqs_tracks;
	track_array_vqvf vqv_tracks;
	make_clips(allocator, 1.0F, vqs_tracks, vqv_tracks);

	compressed_tracks* compressed_tracks_ = nullptr;
	REQUIRE(convert_track_list(allocator, vqs_tracks, compressed_tracks_).empty());
	REQUIRE(compressed_tracks_ != nullptr);
	CHECK(compressed_tracks_->get_track_type() == track_type8::vqsf);

	track_array tracks;
	REQUIRE(convert_track_list(allocator, *compressed_tracks_, tracks).empty());
	REQUIRE(tracks.get_track_type() == track_type8::vqsf);
	REQUIRE(tracks.get_num_tracks() == k_num_transforms);
	REQUIRE(tracks.get_num_samples_per_track() == k_num_samples);

	const track_array_vqsf& vqs_tracks1 = track_array_cast<track_array_vqsf>(tracks);
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		const track_vqsf& track0 = vqs_tracks[transform_index];
		const track_vqsf& track1 = vqs_tracks1[transform_index];

		CHECK(track1.get_description().parent_index == track0.get_description().parent_index);
		CHECK(track1.get_description().precision == track0.get_description().precision);
		CHECK(track1.get_description().shell_distance == track0.get_description().shell_distance);
		CHECK(rtm::vector_all_near_equal3(track1.get_description().default_value.scale, track0.get_description().default_value.scale, 0.0F));

		// The raw format keeps each value
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			CHECK(rtm::quat_near_equal(track0[sample_index].rotation, track1[sample_index].rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal(track0[sample_index].translation_scale, track1[sample_index].translation_scale, 0.0F));
		}
	}

	allocator.deallocate(compressed_tracks_, compressed_tracks_->get_size());
}

TEST_CASE("vqsf pre-processing", "[compression][vqsf]")
{
	ansi_allocator allocator;

	track_array_vqsf vqs_tracks;
	track_array_vqvf vqv_tracks;
	make_clips(allocator, 1.0F, vqs_tracks, vqv_tracks);

	// A leaf transform has a constant translation and a constant scale that differ by less than the precision
	// The scale of a parent transform also scales its children, we use a leaf transform
	const uint32_t k_leaf_index = k_num_transforms - 1;
	for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
	{
		const rtm::vector4f translation = rtm::vector_set(0.1F + float(sample_index) * 1.0E-8F, 0.2F, 0.3F);
		const float scale = 1.25F + float(sample_index) * 1.0E-8F;
		vqs_tracks[k_leaf_index][sample_index].translation_scale = rtm::vector_set_w(translation, scale);
		vqv_tracks[k_leaf_index][sample_index].translation = translation;
		vqv_tracks[k_leaf_index][sample_index].scale = rtm::vector_set(scale);
	}

	pre_process_settings_t settings;
	settings.actions = pre_process_actions::recommended;
	settings.precision_policy = pre_process_precision_policy::lossy;

	REQUIRE(pre_process_track_list(allocator, settings, vqs_tracks).empty());
	REQUIRE(pre_process_track_list(allocator, settings, vqv_tracks).empty());

	// Pre-processing gives the same transforms with both source types
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			const rtm::vqsf& vqs_sample = vqs_tracks[transform_index][sample_index];
			const rtm::vqvf& vqv_sample = vqv_tracks[transform_index][sample_index];
			CHECK(rtm::quat_near_equal(vqs_sample.rotation, vqv_sample.rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(vqs_sample.translation_scale, vqv_sample.translation, 0.0F));
			CHECK(rtm::vector_get_w(vqs_sample.translation_scale) == rtm::vector_get_x(vqv_sample.scale));
		}
	}

	// The constant translation and scale are now exactly constant
	for (uint32_t sample_index = 1; sample_index < k_num_samples; ++sample_index)
		CHECK(rtm::vector_all_near_equal(vqs_tracks[k_leaf_index][sample_index].translation_scale, vqs_tracks[k_leaf_index][0].translation_scale, 0.0F));
}

TEST_CASE("vqsf pre-process transform type conversion", "[compression][vqsf]")
{
	ansi_allocator allocator;

	pre_process_settings_t settings;
	settings.actions = pre_process_actions::convert_transform_type;
	settings.precision_policy = pre_process_precision_policy::lossless;

	// Promotion: VQS -> VQV, the scale is in [xyz]
	{
		track_array_vqsf vqs_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 1.0F, vqs_tracks, vqv_tracks);

		track_array tracks = std::move(vqs_tracks);
		settings.transform_type = track_type8::vqvf;
		REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());
		REQUIRE(tracks.get_track_type() == track_type8::vqvf);

		const track_array_vqvf& promoted_tracks = track_array_cast<track_array_vqvf>(tracks);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& sample = promoted_tracks[transform_index][sample_index];
				const rtm::vqvf& ref_sample = vqv_tracks[transform_index][sample_index];
				CHECK(rtm::quat_near_equal(sample.rotation, ref_sample.rotation, 0.0F));
				CHECK(rtm::vector_all_near_equal3(sample.translation, ref_sample.translation, 0.0F));
				CHECK(rtm::vector_all_near_equal3(sample.scale, ref_sample.scale, 0.0F));
			}
		}
	}

	// Promotion: VQ -> VQS, the scale is the identity of the additive format
	{
		const additive_clip_format8 additive_formats[] = { additive_clip_format8::none, additive_clip_format8::additive1 };
		for (const additive_clip_format8 additive_format : additive_formats)
		{
			const float identity_scale = additive_format == additive_clip_format8::additive1 ? 0.0F : 1.0F;
			const track_array_vqvf base_tracks = make_base(allocator);

			track_array_vqf vq_tracks(allocator, k_num_transforms);
			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				track_vqf vq_track = track_vqf::make_reserve(make_desc(transform_index, 1.0F), allocator, k_num_samples, k_sample_rate);
				for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
				{
					const rtm::vqsf sample = get_vqs_sample(transform_index, sample_index, 1.0F);
					vq_track[sample_index] = rtm::vq_set(sample.rotation, sample.translation_scale);
				}

				vq_tracks[transform_index] = std::move(vq_track);
			}

			track_array tracks = std::move(vq_tracks);
			pre_process_settings_t vq_settings = settings;
			vq_settings.transform_type = track_type8::vqsf;
			if (additive_format != additive_clip_format8::none)
			{
				vq_settings.transform_base.tracks = &base_tracks;
				vq_settings.transform_base.format = additive_format;
			}

			REQUIRE(pre_process_track_list(allocator, vq_settings, tracks).empty());
			REQUIRE(tracks.get_track_type() == track_type8::vqsf);

			const track_array_vqsf& promoted_tracks = track_array_cast<track_array_vqsf>(tracks);
			CHECK(rtm::vector_get_w(promoted_tracks[0][0].translation_scale) == identity_scale);
			CHECK(rtm::vector_all_near_equal3(promoted_tracks[0].get_description().default_value.scale, rtm::vector_set(identity_scale), 0.0F));
		}
	}

	// Demotion: VQV -> VQS when each scale is exactly uniform
	{
		track_array_vqsf vqs_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 1.0F, vqs_tracks, vqv_tracks);

		track_array tracks = std::move(vqv_tracks);
		settings.transform_type = track_type8::vqsf;
		REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());
		REQUIRE(tracks.get_track_type() == track_type8::vqsf);

		const track_array_vqsf& demoted_tracks = track_array_cast<track_array_vqsf>(tracks);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqsf& sample = demoted_tracks[transform_index][sample_index];
				const rtm::vqsf& ref_sample = vqs_tracks[transform_index][sample_index];
				CHECK(rtm::quat_near_equal(sample.rotation, ref_sample.rotation, 0.0F));
				CHECK(rtm::vector_all_near_equal(sample.translation_scale, ref_sample.translation_scale, 0.0F));
			}
		}
	}

	// Demotion with a scale that is not exactly uniform
	{
		const float deltas[] = { 1.0E-6F, 0.5F };
		for (const float delta : deltas)
		{
			track_array_vqsf vqs_tracks;
			track_array_vqvf vqv_tracks;
			make_clips(allocator, 1.0F, vqs_tracks, vqv_tracks);

			// One sample of the leaf transform has a non-uniform scale
			rtm::vqvf& sample = vqv_tracks[k_num_transforms - 1][k_num_samples / 2];
			sample.scale = rtm::vector_add(sample.scale, rtm::vector_set(delta, 0.0F, -delta));

			settings.transform_type = track_type8::vqsf;

			// With the lossless policy, the scale stays
			{
				track_array tracks(allocator, k_num_transforms);
				for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
					tracks[transform_index] = vqv_tracks[transform_index].get_copy(allocator);

				settings.precision_policy = pre_process_precision_policy::lossless;
				REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());
				CHECK(tracks.get_track_type() == track_type8::vqvf);
			}

			// With the lossy policy, the error metric decides
			{
				track_array tracks(allocator, k_num_transforms);
				for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
					tracks[transform_index] = vqv_tracks[transform_index].get_copy(allocator);

				settings.precision_policy = pre_process_precision_policy::lossy;
				REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());

				// A tiny difference is within the precision, a large one is not
				const track_type8 expected_type = delta < 0.1F ? track_type8::vqsf : track_type8::vqvf;
				CHECK(tracks.get_track_type() == expected_type);

				if (expected_type == track_type8::vqsf)
				{
					// The new uniform scale is the mean of [xyz]
					const track_array_vqsf& demoted_tracks = track_array_cast<track_array_vqsf>(tracks);
					const float expected_scale = (rtm::vector_get_x(sample.scale) + rtm::vector_get_y(sample.scale) + rtm::vector_get_z(sample.scale)) * (1.0F / 3.0F);
					CHECK(rtm::vector_get_w(demoted_tracks[k_num_transforms - 1][k_num_samples / 2].translation_scale) == expected_scale);
				}
			}
		}
	}

	// Demotion: VQS -> VQ only when each scale is the identity
	{
		track_array_vqsf vqs_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 1.0F, vqs_tracks, vqv_tracks);

		settings.transform_type = track_type8::vqf;
		settings.precision_policy = pre_process_precision_policy::lossless;

		// The clip has a scale, it stays
		{
			track_array tracks(allocator, k_num_transforms);
			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
				tracks[transform_index] = vqs_tracks[transform_index].get_copy(allocator);

			REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());
			CHECK(tracks.get_track_type() == track_type8::vqsf);
		}

		// Without scale, the clip becomes a VQ clip
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
				vqs_tracks[transform_index][sample_index].translation_scale = rtm::vector_set_w(vqs_tracks[transform_index][sample_index].translation_scale, 1.0F);
		}

		track_array tracks = std::move(vqs_tracks);
		REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());
		CHECK(tracks.get_track_type() == track_type8::vqf);
	}

	// A typed track array cannot change its type
	{
		track_array_vqsf vqs_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 1.0F, vqs_tracks, vqv_tracks);

		settings.precision_policy = pre_process_precision_policy::lossless;

		settings.transform_type = track_type8::vqvf;
		CHECK(pre_process_track_list(allocator, settings, vqs_tracks).any());

		settings.transform_type = track_type8::vqsf;
		CHECK(pre_process_track_list(allocator, settings, vqs_tracks).empty());
		CHECK(pre_process_track_list(allocator, settings, vqv_tracks).any());
	}
}
