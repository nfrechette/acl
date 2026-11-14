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
#include <rtm/vector4f.h>
#include <rtm/vqf.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <utility>

using namespace acl;

namespace
{
	constexpr uint32_t k_num_transforms = 4;
	constexpr uint32_t k_num_samples = 20;
	constexpr float k_sample_rate = 30.0F;

	track_desc_transformf make_desc(uint32_t transform_index)
	{
		track_desc_transformf desc;
		desc.output_index = transform_index;
		desc.parent_index = transform_index == 0 ? k_invalid_track_index : (transform_index - 1);
		desc.precision = 0.001F;
		desc.shell_distance = 3.0F;
		return desc;
	}

	// A chain of transforms with animated rotations and translations
	rtm::vqf get_vq_sample(uint32_t transform_index, uint32_t sample_index)
	{
		const float alpha = float(sample_index) / float(k_num_samples - 1);
		const float offset = float(transform_index) * 0.3F;
		const rtm::quatf rotation = rtm::quat_from_euler(alpha + offset, 0.5F * alpha, 0.2F + offset);
		const rtm::vector4f translation = rtm::vector_set(1.0F + alpha, offset, 2.0F * alpha);
		return rtm::vq_set(rotation, translation);
	}

	// Builds the same clip as VQ tracks and as VQV tracks
	// The VQV tracks use 'scale' for each sample
	void make_clips(iallocator& allocator, float scale, track_array_vqf& out_vq_tracks, track_array_vqvf& out_vqv_tracks)
	{
		out_vq_tracks = track_array_vqf(allocator, k_num_transforms);
		out_vqv_tracks = track_array_vqvf(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const track_desc_transformf desc = make_desc(transform_index);

			// The default scale of the VQV tracks is the same as the scale of their samples
			track_desc_transformf vqv_desc = desc;
			vqv_desc.default_value.scale = rtm::vector_set(scale);

			track_vqf vq_track = track_vqf::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
			track_vqvf vqv_track = track_vqvf::make_reserve(vqv_desc, allocator, k_num_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqf sample = get_vq_sample(transform_index, sample_index);
				vq_track[sample_index] = sample;
				vqv_track[sample_index] = rtm::vqv_set(sample.rotation, sample.translation, rtm::vector_set(scale));
			}

			out_vq_tracks[transform_index] = std::move(vq_track);
			out_vqv_tracks[transform_index] = std::move(vqv_track);
		}
	}

	// A base pose with non-uniform scale for the additive clips
	track_array_vqvf make_base(iallocator& allocator)
	{
		track_array_vqvf base_tracks(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const float base_offset = float(transform_index) * 0.2F;
			const rtm::vqvf base_transform = rtm::vqv_set(rtm::quat_from_euler(0.2F, base_offset, 0.1F), rtm::vector_set(0.5F, 0.0F, 0.2F), rtm::vector_set(1.2F, 0.9F, 1.1F));
			base_tracks[transform_index] = track_vqvf::make_copy(make_desc(transform_index), allocator, &base_transform, 1, k_sample_rate);
		}

		return base_tracks;
	}

	// A track writer for a pose without scale, it counts the scale writes
	struct vq_track_writer final : public track_writer
	{
		rtm::vqf poses[k_num_transforms];
		uint32_t num_scale_writes = 0;

		void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation) { poses[track_index].rotation = rotation; }
		void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation) { poses[track_index].translation = translation; }
		void RTM_SIMD_CALL write_scale(uint32_t /*track_index*/, rtm::vector4f_arg0 /*scale*/) { num_scale_writes++; }
	};

	void compress(iallocator& allocator, const track_array& tracks, const track_array* base_tracks, additive_clip_format8 additive_format, compressed_tracks*& out_compressed_tracks)
	{
		const compression_settings settings = get_default_compression_settings();

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

	// Compresses a clip as VQ and as VQV tracks, then compares the results
	void test_vq_compression(additive_clip_format8 additive_format)
	{
		ansi_allocator allocator;

		const bool is_additive = additive_format != additive_clip_format8::none;

		// The VQV clip uses the scale that ACL uses for a VQ clip: 0.0 with additive1, 1.0 otherwise
		const float scale = additive_format == additive_clip_format8::additive1 ? 0.0F : 1.0F;

		track_array_vqf vq_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, scale, vq_tracks, vqv_tracks);

		const track_array_vqvf base_tracks = make_base(allocator);
		const track_array* base_tracks_ptr = is_additive ? &base_tracks : nullptr;

		compressed_tracks* vq_compressed = nullptr;
		compressed_tracks* vqv_compressed = nullptr;
		compress(allocator, vq_tracks, base_tracks_ptr, additive_format, vq_compressed);
		compress(allocator, vqv_tracks, base_tracks_ptr, additive_format, vqv_compressed);

		CHECK(vq_compressed->get_track_type() == track_type8::vqf);
		CHECK(vqv_compressed->get_track_type() == track_type8::vqvf);
		CHECK(vq_compressed->get_version() == compressed_tracks_version16::latest);
		CHECK(vq_compressed->get_default_scale() == int32_t(scale));

		// The streams are the same, only the track type and the hash differ
		REQUIRE(vq_compressed->get_size() == vqv_compressed->get_size());
		{
			const uint32_t size = vq_compressed->get_size();
			const uint8_t* vq_buffer = acl_impl::bit_cast<const uint8_t*>(vq_compressed);
			const uint8_t* vqv_buffer = acl_impl::bit_cast<const uint8_t*>(vqv_compressed);

			const size_t track_type_offset = size_t(acl_impl::bit_cast<const uint8_t*>(&acl_impl::get_tracks_header(*vq_compressed).track_type) - vq_buffer);

			// The raw buffer header holds the size and the hash, we skip it
			uint32_t num_different_bytes = 0;
			for (uint32_t offset = uint32_t(sizeof(acl_impl::raw_buffer_header)); offset < size; ++offset)
			{
				if (offset == track_type_offset)
					continue;

				if (vq_buffer[offset] != vqv_buffer[offset])
					num_different_bytes++;
			}

			CHECK(num_different_bytes == 0);
		}

		decompression_context<debug_transform_decompression_settings> vq_context;
		decompression_context<debug_transform_decompression_settings> vqv_context;
		REQUIRE(vq_context.initialize(*vq_compressed));
		REQUIRE(vqv_context.initialize(*vqv_compressed));

		vq_track_writer vq_writer;
		vq_track_writer vqv_writer;

		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			const float sample_time = float(sample_index) / k_sample_rate;

			vq_context.seek(sample_time, sample_rounding_policy::nearest);
			vq_context.decompress_tracks(vq_writer);

			vqv_context.seek(sample_time, sample_rounding_policy::nearest);
			vqv_context.decompress_tracks(vqv_writer);

			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				CHECK(rtm::quat_near_equal(vq_writer.poses[transform_index].rotation, vqv_writer.poses[transform_index].rotation, 0.0F));
				CHECK(rtm::vector_all_near_equal3(vq_writer.poses[transform_index].translation, vqv_writer.poses[transform_index].translation, 0.0F));
			}

			// We also decompress one track at a time
			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				vq_context.decompress_track(transform_index, vq_writer);
				vqv_context.decompress_track(transform_index, vqv_writer);
			}
		}

		// A VQ clip has no scale: decompression does not write it
		CHECK(vq_writer.num_scale_writes == 0);
		CHECK(vqv_writer.num_scale_writes != 0);

		// The error is the same with both source types
		// We measure with the VQM runtime transform type for both source types
		compression_settings error_settings = get_default_compression_settings();
		error_settings.transform_runtime_type = transform_runtime_type_t::vqmf;

		track_error vq_error;
		track_error vqv_error;
		if (is_additive)
		{
			transform_base_desc_t transform_base;
			transform_base.format = additive_format;
			transform_base.tracks = &base_tracks;
			vq_error = calculate_compression_error(allocator, vq_tracks, vq_context, error_settings, transform_base);
			vqv_error = calculate_compression_error(allocator, vqv_tracks, vqv_context, error_settings, transform_base);
		}
		else
		{
			vq_error = calculate_compression_error(allocator, vq_tracks, vq_context, error_settings);
			vqv_error = calculate_compression_error(allocator, vqv_tracks, vqv_context, error_settings);
		}

		CHECK(vq_error.error >= 0.0F);
		CHECK(vq_error.error < 0.01F);
		CHECK(vq_error.error == vqv_error.error);
		CHECK(vq_error.index == vqv_error.index);
		CHECK(vq_error.sample_time == vqv_error.sample_time);

		allocator.deallocate(vq_compressed, vq_compressed->get_size());
		allocator.deallocate(vqv_compressed, vqv_compressed->get_size());
	}
}

TEST_CASE("vqf compression", "[compression][vqf]")
{
	test_vq_compression(additive_clip_format8::none);
}

TEST_CASE("vqf compression with additive0", "[compression][vqf]")
{
	test_vq_compression(additive_clip_format8::additive0);
}

TEST_CASE("vqf compression with additive1", "[compression][vqf]")
{
	test_vq_compression(additive_clip_format8::additive1);
}

TEST_CASE("vqf raw conversion", "[compression][vqf]")
{
	ansi_allocator allocator;

	track_array_vqf vq_tracks;
	track_array_vqvf vqv_tracks;
	make_clips(allocator, 1.0F, vq_tracks, vqv_tracks);

	compressed_tracks* compressed_tracks_ = nullptr;
	REQUIRE(convert_track_list(allocator, vq_tracks, compressed_tracks_).empty());
	REQUIRE(compressed_tracks_ != nullptr);
	CHECK(compressed_tracks_->get_track_type() == track_type8::vqf);

	track_array tracks;
	REQUIRE(convert_track_list(allocator, *compressed_tracks_, tracks).empty());
	REQUIRE(tracks.get_track_type() == track_type8::vqf);
	REQUIRE(tracks.get_num_tracks() == k_num_transforms);
	REQUIRE(tracks.get_num_samples_per_track() == k_num_samples);

	const track_array_vqf& vq_tracks1 = track_array_cast<track_array_vqf>(tracks);
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		const track_vqf& track0 = vq_tracks[transform_index];
		const track_vqf& track1 = vq_tracks1[transform_index];

		CHECK(track1.get_description().parent_index == track0.get_description().parent_index);
		CHECK(track1.get_description().precision == track0.get_description().precision);
		CHECK(track1.get_description().shell_distance == track0.get_description().shell_distance);

		// The raw format keeps each value
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			CHECK(rtm::quat_near_equal(track0[sample_index].rotation, track1[sample_index].rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(track0[sample_index].translation, track1[sample_index].translation, 0.0F));
		}
	}

	allocator.deallocate(compressed_tracks_, compressed_tracks_->get_size());
}

TEST_CASE("vqf pre-processing", "[compression][vqf]")
{
	ansi_allocator allocator;

	track_array_vqf vq_tracks;
	track_array_vqvf vqv_tracks;
	make_clips(allocator, 1.0F, vq_tracks, vqv_tracks);

	// A constant rotation that differs by less than the precision
	for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
	{
		const rtm::quatf rotation = rtm::quat_from_euler(0.1F + float(sample_index) * 1.0E-7F, 0.2F, 0.3F);
		vq_tracks[0][sample_index].rotation = rotation;
		vqv_tracks[0][sample_index].rotation = rotation;
	}

	pre_process_settings_t settings;
	settings.actions = pre_process_actions::recommended;
	settings.precision_policy = pre_process_precision_policy::lossy;

	REQUIRE(pre_process_track_list(allocator, settings, vq_tracks).empty());
	REQUIRE(pre_process_track_list(allocator, settings, vqv_tracks).empty());

	// Pre-processing gives the same rotations and translations with both source types
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			CHECK(rtm::quat_near_equal(vq_tracks[transform_index][sample_index].rotation, vqv_tracks[transform_index][sample_index].rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(vq_tracks[transform_index][sample_index].translation, vqv_tracks[transform_index][sample_index].translation, 0.0F));
		}
	}

	// The constant rotation is now exactly constant
	for (uint32_t sample_index = 1; sample_index < k_num_samples; ++sample_index)
		CHECK(rtm::quat_near_equal(vq_tracks[0][sample_index].rotation, vq_tracks[0][0].rotation, 0.0F));
}

TEST_CASE("vqf pre-process transform type conversion", "[compression][vqf]")
{
	ansi_allocator allocator;

	pre_process_settings_t settings;
	settings.actions = pre_process_actions::convert_transform_type;

	// Promotion: VQ -> VQV, the scale is the identity
	{
		track_array_vqf vq_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 1.0F, vq_tracks, vqv_tracks);

		track_array tracks = std::move(vq_tracks);
		settings.transform_type = track_type8::vqvf;
		REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());
		REQUIRE(tracks.get_track_type() == track_type8::vqvf);

		const track_array_vqvf& promoted_tracks = track_array_cast<track_array_vqvf>(tracks);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			CHECK(rtm::vector_all_near_equal3(promoted_tracks[transform_index].get_description().default_value.scale, rtm::vector_set(1.0F), 0.0F));

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& sample = promoted_tracks[transform_index][sample_index];
				const rtm::vqvf& ref_sample = vqv_tracks[transform_index][sample_index];
				CHECK(rtm::quat_near_equal(sample.rotation, ref_sample.rotation, 0.0F));
				CHECK(rtm::vector_all_near_equal3(sample.translation, ref_sample.translation, 0.0F));
				CHECK(rtm::vector_all_near_equal3(sample.scale, rtm::vector_set(1.0F), 0.0F));
			}
		}
	}

	// Promotion of an additive1 clip: the identity scale is 0.0
	{
		track_array_vqf vq_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 0.0F, vq_tracks, vqv_tracks);
		const track_array_vqvf base_tracks = make_base(allocator);

		track_array tracks = std::move(vq_tracks);
		pre_process_settings_t additive_settings = settings;
		additive_settings.transform_type = track_type8::vqvf;
		additive_settings.transform_base.tracks = &base_tracks;
		additive_settings.transform_base.format = additive_clip_format8::additive1;
		REQUIRE(pre_process_track_list(allocator, additive_settings, tracks).empty());
		REQUIRE(tracks.get_track_type() == track_type8::vqvf);

		const track_array_vqvf& promoted_tracks = track_array_cast<track_array_vqvf>(tracks);
		CHECK(rtm::vector_all_near_equal3(promoted_tracks[0][0].scale, rtm::vector_set(0.0F), 0.0F));
		CHECK(rtm::vector_all_near_equal3(promoted_tracks[0].get_description().default_value.scale, rtm::vector_set(0.0F), 0.0F));
	}

	// Demotion: VQV -> VQ when the scale is exactly the identity
	{
		track_array_vqf vq_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 1.0F, vq_tracks, vqv_tracks);

		track_array tracks = std::move(vqv_tracks);
		settings.transform_type = track_type8::vqf;
		settings.precision_policy = pre_process_precision_policy::lossless;
		REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());
		REQUIRE(tracks.get_track_type() == track_type8::vqf);

		const track_array_vqf& demoted_tracks = track_array_cast<track_array_vqf>(tracks);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				CHECK(rtm::quat_near_equal(demoted_tracks[transform_index][sample_index].rotation, vq_tracks[transform_index][sample_index].rotation, 0.0F));
				CHECK(rtm::vector_all_near_equal3(demoted_tracks[transform_index][sample_index].translation, vq_tracks[transform_index][sample_index].translation, 0.0F));
			}
		}
	}

	// Demotion with a scale that is not exactly the identity
	{
		const float scales[] = { 1.0F + 1.0E-6F, 1.5F };
		for (const float scale : scales)
		{
			track_array_vqf vq_tracks;
			track_array_vqvf vqv_tracks;
			make_clips(allocator, 1.0F, vq_tracks, vqv_tracks);

			// One sample of the leaf transform has a different scale
			vqv_tracks[k_num_transforms - 1][k_num_samples / 2].scale = rtm::vector_set(scale);

			settings.transform_type = track_type8::vqf;

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

				// A tiny scale change is within the precision, a large one is not
				const track_type8 expected_type = scale < 1.1F ? track_type8::vqf : track_type8::vqvf;
				CHECK(tracks.get_track_type() == expected_type);
			}
		}
	}

	// A typed track array cannot change its type
	{
		track_array_vqf vq_tracks;
		track_array_vqvf vqv_tracks;
		make_clips(allocator, 1.0F, vq_tracks, vqv_tracks);

		settings.precision_policy = pre_process_precision_policy::lossless;

		settings.transform_type = track_type8::vqf;
		CHECK(pre_process_track_list(allocator, settings, vqv_tracks).any());
		CHECK(pre_process_track_list(allocator, settings, vq_tracks).empty());

		settings.transform_type = track_type8::vqvf;
		CHECK(pre_process_track_list(allocator, settings, vq_tracks).any());
		CHECK(pre_process_track_list(allocator, settings, vqv_tracks).empty());

		// The type must be a transform type
		track_array tracks = std::move(vqv_tracks);
		settings.transform_type = track_type8::float3f;
		CHECK(pre_process_track_list(allocator, settings, tracks).any());
	}
}
