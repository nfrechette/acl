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
#include <acl/compression/transform_base.h>
#include <acl/compression/transform_base_adapter.h>
#include <acl/compression/transform_space.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/compression/transform_metrics.h>
#include <acl/core/additive_utils.h>
#include <acl/core/ansi_allocator.h>
#include <acl/core/track_writer.h>
#include <acl/decompression/decompress.h>

#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/vqf.h>
#include <rtm/vqmf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <cstring>
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

	// A chain of transforms with animated rotations, translations, and scales
	track_array_vqvf make_clip(iallocator& allocator)
	{
		track_array_vqvf tracks(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			track_vqvf track = track_vqvf::make_reserve(make_desc(transform_index), allocator, k_num_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const float alpha = float(sample_index) / float(k_num_samples - 1);
				const float offset = float(transform_index) * 0.3F;
				const rtm::quatf rotation = rtm::quat_from_euler(alpha + offset, 0.5F * alpha, 0.2F + offset);
				const rtm::vector4f translation = rtm::vector_set(1.0F + alpha, offset, 2.0F * alpha);
				const rtm::vector4f scale = rtm::vector_set(1.0F + 0.1F * alpha, 1.0F, 1.0F - 0.1F * alpha);
				track[sample_index] = rtm::vqv_set(rotation, translation, scale);
			}

			tracks[transform_index] = std::move(track);
		}

		return tracks;
	}

	// A base with fewer samples than the clip
	track_array_vqvf make_base(iallocator& allocator)
	{
		constexpr uint32_t k_num_base_samples = 3;

		track_array_vqvf tracks(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			track_vqvf track = track_vqvf::make_reserve(make_desc(transform_index), allocator, k_num_base_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < k_num_base_samples; ++sample_index)
			{
				const float offset = float(transform_index) * 0.2F + float(sample_index) * 0.05F;
				track[sample_index] = rtm::vqv_set(rtm::quat_from_euler(0.2F, offset, 0.1F), rtm::vector_set(0.5F, 0.0F, 0.2F), rtm::vector_set(1.2F, 0.9F, 1.1F));
			}

			tracks[transform_index] = std::move(track);
		}

		return tracks;
	}

	compressed_tracks* compress(iallocator& allocator, const track_array& tracks, const transform_base_desc_t& transform_base)
	{
		const compression_settings settings = get_default_compression_settings();

		output_stats stats;
		compressed_tracks* compressed_tracks_ = nullptr;
		const error_result result = compress_track_list(allocator, tracks, settings, transform_base, compressed_tracks_, stats);
		REQUIRE(result.empty());
		REQUIRE(compressed_tracks_ != nullptr);
		REQUIRE(compressed_tracks_->is_valid(true).empty());
		return compressed_tracks_;
	}

	bool are_equal(const compressed_tracks& tracks0, const compressed_tracks& tracks1)
	{
		return tracks0.get_size() == tracks1.get_size() && std::memcmp(&tracks0, &tracks1, tracks0.get_size()) == 0;
	}

	// Puts a local space pose of the transform chain in mesh space: the parent of a transform comes before it
	void local_to_mesh_space(const rtm::vqvf* local_pose, rtm::vqvf* out_mesh_pose)
	{
		out_mesh_pose[0] = local_pose[0];
		for (uint32_t transform_index = 1; transform_index < k_num_transforms; ++transform_index)
			out_mesh_pose[transform_index] = rtm::vqv_normalize(rtm::vqv_mul(local_pose[transform_index], out_mesh_pose[transform_index - 1]));
	}

	// Returns a copy of the clip where each sample is in mesh space
	// The tracks keep their parent index, the copy has the mesh transform space
	track_array_vqvf make_mesh_space_clip(iallocator& allocator, const track_array_vqvf& local_clip)
	{
		track_array_vqvf mesh_clip(allocator, k_num_transforms);
		mesh_clip.set_transform_space(transform_space_t::mesh);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			mesh_clip[transform_index] = track_vqvf::make_reserve(local_clip[transform_index].get_description(), allocator, k_num_samples, k_sample_rate);

		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			rtm::vqvf local_pose[k_num_transforms];
			rtm::vqvf mesh_pose[k_num_transforms];
			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
				local_pose[transform_index] = local_clip[transform_index][sample_index];

			local_to_mesh_space(local_pose, mesh_pose);

			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
				mesh_clip[transform_index][sample_index] = mesh_pose[transform_index];
		}

		return mesh_clip;
	}

	// A track writer that keeps one pose
	// The compressed tracks of these tests have 'k_num_transforms' tracks, the writer ignores the other track indices
	struct pose_writer final : public track_writer
	{
		rtm::vqvf pose[k_num_transforms];

		void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation)
		{
			if (track_index < k_num_transforms)
				pose[track_index].rotation = rotation;
		}

		void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation)
		{
			if (track_index < k_num_transforms)
				pose[track_index].translation = translation;
		}

		void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale)
		{
			if (track_index < k_num_transforms)
				pose[track_index].scale = scale;
		}
	};

	// Returns the largest distance between the reference and the decompressed translations, after we apply the base
	// 'mesh_base_pose' is the base in mesh space, or null without a base
	// Puts the rotations of a local space pose of the transform chain in mesh space, the translations and scales stay local
	void local_to_mesh_rotation_space(const rtm::vqvf* local_pose, rtm::vqvf* out_pose)
	{
		out_pose[0] = local_pose[0];
		for (uint32_t transform_index = 1; transform_index < k_num_transforms; ++transform_index)
		{
			out_pose[transform_index] = local_pose[transform_index];
			out_pose[transform_index].rotation = rtm::quat_normalize(rtm::quat_mul(local_pose[transform_index].rotation, out_pose[transform_index - 1].rotation));
		}
	}

	// Puts a mesh rotation space pose of the transform chain in object space
	void mesh_rotation_to_object_space(const rtm::vqvf* pose, rtm::vqvf* out_object_pose)
	{
		out_object_pose[0] = pose[0];
		for (uint32_t transform_index = 1; transform_index < k_num_transforms; ++transform_index)
		{
			const rtm::vqvf& parent_object_transform = out_object_pose[transform_index - 1];

			rtm::vqvf local_transform = pose[transform_index];
			local_transform.rotation = rtm::quat_mul(local_transform.rotation, rtm::quat_conjugate(parent_object_transform.rotation));

			out_object_pose[transform_index] = rtm::vqv_normalize(rtm::vqv_mul(local_transform, parent_object_transform));
		}
	}

	// Returns the largest distance between the object space translations of the local clip and of the decompressed
	// mesh rotation space clip, both with VQM arithmetic: the product keeps the shear of a non-uniform scale
	float find_max_mesh_rotation_error_vqm(const compressed_tracks& tracks, const track_array_vqvf& local_clip)
	{
		decompression_context<default_transform_decompression_settings> context;
		REQUIRE(context.initialize(tracks));

		float max_error = 0.0F;
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			context.seek(float(sample_index) / k_sample_rate, sample_rounding_policy::nearest);

			pose_writer writer;
			context.decompress_tracks(writer);

			rtm::vqmf reference_object_pose[k_num_transforms];
			rtm::vqmf object_pose[k_num_transforms];
			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				const rtm::vqvf& local_transform = local_clip[transform_index][sample_index];
				const rtm::vqmf reference_local = rtm::vqm_set(local_transform.rotation, local_transform.translation, local_transform.scale);

				const rtm::vqvf& transform = writer.pose[transform_index];
				rtm::vqmf local = rtm::vqm_set(transform.rotation, transform.translation, transform.scale);

				if (transform_index == 0)
				{
					reference_object_pose[0] = reference_local;
					object_pose[0] = local;
				}
				else
				{
					const rtm::vqmf& parent_object_transform = object_pose[transform_index - 1];
					local.rotation = rtm::quat_mul(local.rotation, rtm::quat_conjugate(parent_object_transform.rotation));

					reference_object_pose[transform_index] = rtm::vqm_normalize(rtm::vqm_mul(reference_local, reference_object_pose[transform_index - 1]));
					object_pose[transform_index] = rtm::vqm_normalize(rtm::vqm_mul(local, parent_object_transform));
				}

				const rtm::vector4f reference_translation = rtm::vqm_get_translation(reference_object_pose[transform_index]);
				const rtm::vector4f translation = rtm::vqm_get_translation(object_pose[transform_index]);
				max_error = rtm::scalar_max(max_error, rtm::vector_distance3(translation, reference_translation));
			}
		}

		return max_error;
	}

	// Returns a copy of the clip where each sample is in mesh rotation space, the copy has the mesh rotation transform space
	track_array_vqvf make_mesh_rotation_space_clip(iallocator& allocator, const track_array_vqvf& local_clip)
	{
		track_array_vqvf clip(allocator, k_num_transforms);
		clip.set_transform_space(transform_space_t::mesh_rotation);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			clip[transform_index] = track_vqvf::make_reserve(local_clip[transform_index].get_description(), allocator, k_num_samples, k_sample_rate);

		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			rtm::vqvf local_pose[k_num_transforms];
			rtm::vqvf pose[k_num_transforms];
			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
				local_pose[transform_index] = local_clip[transform_index][sample_index];

			local_to_mesh_rotation_space(local_pose, pose);

			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
				clip[transform_index][sample_index] = pose[transform_index];
		}

		return clip;
	}

	// Returns a copy of a clip with VQS tracks: the uniform scale is the X component of the scale
	// The copy has the transform space of the clip
	track_array_vqsf make_vqs_clip(iallocator& allocator, const track_array_vqvf& vqv_clip)
	{
		track_array_vqsf clip(allocator, k_num_transforms);
		clip.set_transform_space(vqv_clip.get_transform_space());
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const track_vqvf& vqv_track = vqv_clip[transform_index];
			track_vqsf vqs_track = track_vqsf::make_reserve(vqv_track.get_description(), allocator, k_num_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& transform = vqv_track[sample_index];
				vqs_track[sample_index] = rtm::vqs_set(transform.rotation, transform.translation, rtm::vector_get_x(transform.scale));
			}

			clip[transform_index] = std::move(vqs_track);
		}

		return clip;
	}

	// Returns the largest distance between the reference object space translations and the decompressed
	// mesh rotation space clip in object space, after we apply the base
	// 'base_pose' is the base in mesh rotation space, or null without a base
	float find_max_mesh_rotation_error(const compressed_tracks& tracks, const track_array_vqvf& object_reference_clip, const rtm::vqvf* base_pose, additive_clip_format8 base_format)
	{
		decompression_context<default_transform_decompression_settings> context;
		REQUIRE(context.initialize(tracks));

		float max_error = 0.0F;
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			context.seek(float(sample_index) / k_sample_rate, sample_rounding_policy::nearest);

			pose_writer writer;
			context.decompress_tracks(writer);

			rtm::vqvf pose[k_num_transforms];
			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				pose[transform_index] = writer.pose[transform_index];
				if (base_pose != nullptr)
					pose[transform_index] = apply_additive_to_base(base_format, base_pose[transform_index], pose[transform_index]);
			}

			rtm::vqvf object_pose[k_num_transforms];
			mesh_rotation_to_object_space(pose, object_pose);

			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				const rtm::vqvf& reference = object_reference_clip[transform_index][sample_index];
				max_error = rtm::scalar_max(max_error, rtm::vector_distance3(object_pose[transform_index].translation, reference.translation));
			}
		}

		return max_error;
	}

	float find_max_translation_error(const compressed_tracks& tracks, const track_array_vqvf& reference_clip, const rtm::vqvf* mesh_base_pose, additive_clip_format8 base_format)
	{
		decompression_context<default_transform_decompression_settings> context;
		REQUIRE(context.initialize(tracks));

		float max_error = 0.0F;
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			context.seek(float(sample_index) / k_sample_rate, sample_rounding_policy::nearest);

			pose_writer writer;
			context.decompress_tracks(writer);

			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				rtm::vqvf transform = writer.pose[transform_index];
				if (mesh_base_pose != nullptr)
					transform = apply_additive_to_base(base_format, mesh_base_pose[transform_index], transform);

				const rtm::vqvf& reference = reference_clip[transform_index][sample_index];
				max_error = rtm::scalar_max(max_error, rtm::vector_distance3(transform.translation, reference.translation));
			}
		}

		return max_error;
	}
}

TEST_CASE("transform_base names", "[compression][transform_space]")
{
	CHECK(std::strcmp(get_transform_space_name(transform_space_t::local), "local") == 0);

	transform_space_t space = transform_space_t::local;
	CHECK(get_transform_space("local", space));
	CHECK(space == transform_space_t::local);
	CHECK(std::strcmp(get_transform_space_name(transform_space_t::mesh), "mesh") == 0);
	CHECK(get_transform_space("mesh", space));
	CHECK(space == transform_space_t::mesh);

	CHECK(std::strcmp(get_transform_space_name(transform_space_t::mesh_rotation), "mesh_rotation") == 0);
	CHECK(get_transform_space("mesh_rotation", space));
	CHECK(space == transform_space_t::mesh_rotation);

	// A longer name is not a match
	CHECK_FALSE(get_transform_space("mesh_space", space));
	CHECK_FALSE(get_transform_space("locals", space));

	// The name does not need a null terminator after the match
	CHECK(get_transform_space("local\" = 1", space));
	CHECK(space == transform_space_t::local);
}

TEST_CASE("track_array transform space", "[compression][transform_space]")
{
	ansi_allocator allocator;

	// A track array is in local space by default
	track_array_vqvf clip = make_clip(allocator);
	CHECK(clip.get_transform_space() == transform_space_t::local);
	CHECK(clip.is_valid().empty());

	clip.set_transform_space(transform_space_t::mesh_rotation);
	CHECK(clip.get_transform_space() == transform_space_t::mesh_rotation);
	CHECK(clip.is_valid().empty());

	// A move keeps the transform space
	track_array_vqvf moved_clip = std::move(clip);
	CHECK(moved_clip.get_transform_space() == transform_space_t::mesh_rotation);

	track_array moved_again;
	moved_again = std::move(moved_clip);
	CHECK(moved_again.get_transform_space() == transform_space_t::mesh_rotation);

	// An invalid value is not valid
	moved_again.set_transform_space(static_cast<transform_space_t>(200));
	CHECK(moved_again.is_valid().any());
}

TEST_CASE("convert a track array in each transform space", "[compression][transform_space]")
{
	ansi_allocator allocator;

	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);
	const track_array_vqvf mesh_rotation_clip = make_mesh_rotation_space_clip(allocator, local_clip);

	const track_array_vqvf* clips[] = { &local_clip, &mesh_clip, &mesh_rotation_clip };
	for (const track_array_vqvf* clip : clips)
	{
		INFO("space: " << get_transform_space_name(clip->get_transform_space()));

		// The conversion is lossless in each transform space
		compressed_tracks* tracks = nullptr;
		REQUIRE(convert_track_list(allocator, *clip, tracks).empty());
		REQUIRE(tracks != nullptr);

		track_array raw_tracks;
		REQUIRE(convert_track_list(allocator, *tracks, raw_tracks).empty());
		REQUIRE(raw_tracks.get_num_tracks() == k_num_transforms);
		REQUIRE(raw_tracks.get_num_samples_per_track() == k_num_samples);

		const track_array_vqvf& raw_tracks_vqvf = track_array_cast<track_array_vqvf>(raw_tracks);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& expected = (*clip)[transform_index][sample_index];
				const rtm::vqvf& actual = raw_tracks_vqvf[transform_index][sample_index];
				CHECK(rtm::vector_all_near_equal3(expected.translation, actual.translation, 0.0F));
				CHECK(rtm::quat_near_equal(expected.rotation, actual.rotation, 0.0001F));
			}
		}

		allocator.deallocate(tracks, tracks->get_size());
	}
}

TEST_CASE("transform_base_desc_t validation", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf base = make_base(allocator);
	const track_array_vqvf empty_base;

	{
		const transform_base_desc_t transform_base;
		CHECK_FALSE(transform_base.has_base());
		CHECK(transform_base.is_valid(transform_space_t::local).empty());
	}

	{
		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::additive1;
		transform_base.tracks = &base;
		CHECK(transform_base.has_base());
		CHECK(transform_base.is_valid(transform_space_t::local).empty());
	}

	{
		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::relative;
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// No base

		transform_base.tracks = &empty_base;
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// Empty base
	}

	{
		transform_base_desc_t transform_base;
		transform_base.tracks = &base;
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// A base without a base format
	}

	{
		transform_base_desc_t transform_base;
		transform_base.format = static_cast<additive_clip_format8>(200);
		transform_base.tracks = &base;
		CHECK(transform_base.is_valid(transform_space_t::local).any());
	}

	// The base must be in local space, for each space of the clip
	{
		track_array_vqvf mesh_base = make_base(allocator);
		mesh_base.set_transform_space(transform_space_t::mesh);

		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::additive0;
		transform_base.tracks = &mesh_base;
		CHECK(transform_base.is_valid(transform_space_t::local).any());
		CHECK(transform_base.is_valid(transform_space_t::mesh).any());
	}
}

TEST_CASE("compression with a transform space", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf clip = make_clip(allocator);
	const track_array_vqvf base = make_base(allocator);
	const compression_settings settings = get_default_compression_settings();

	// An invalid transform space returns an error
	{
		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::additive0;

		output_stats stats;
		compressed_tracks* compressed_tracks_ = nullptr;
		CHECK(compress_track_list(allocator, clip, settings, transform_base, compressed_tracks_, stats).any());
		CHECK(compressed_tracks_ == nullptr);
	}

	// Without a base, the default transform space gives the same data as the overload without a transform space
	{
		compressed_tracks* space_tracks = compress(allocator, clip, transform_base_desc_t());

		output_stats stats;
		compressed_tracks* default_tracks = nullptr;
		REQUIRE(compress_track_list(allocator, clip, settings, default_tracks, stats).empty());
		REQUIRE(default_tracks != nullptr);

		CHECK(are_equal(*space_tracks, *default_tracks));

		allocator.deallocate(space_tracks, space_tracks->get_size());
		allocator.deallocate(default_tracks, default_tracks->get_size());
	}

	const additive_clip_format8 base_formats[] = { additive_clip_format8::relative, additive_clip_format8::additive0, additive_clip_format8::additive1 };
	for (const additive_clip_format8 base_format : base_formats)
	{
		transform_base_desc_t transform_base;
		transform_base.format = base_format;
		transform_base.tracks = &base;

		compressed_tracks* space_tracks = compress(allocator, clip, transform_base);
		CHECK(space_tracks->get_default_scale() == (base_format == additive_clip_format8::additive1 ? 0 : 1));

		// The deprecated overload gives the same data
		ACL_IMPL_DEPRECATION_WARNING_PUSH
		output_stats stats;
		compressed_tracks* deprecated_tracks = nullptr;
		REQUIRE(compress_track_list(allocator, clip, settings, base, base_format, deprecated_tracks, stats).empty());
		ACL_IMPL_DEPRECATION_WARNING_POP
		REQUIRE(deprecated_tracks != nullptr);

		CHECK(are_equal(*space_tracks, *deprecated_tracks));

		// The error is measured with the base
		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*space_tracks));

		const track_error error = calculate_compression_error(allocator, clip, context, settings, transform_base);
		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);

		allocator.deallocate(space_tracks, space_tracks->get_size());
		allocator.deallocate(deprecated_tracks, deprecated_tracks->get_size());
	}

	// The deprecated overload ignores an empty base
	{
		const track_array_vqvf empty_base;
		compressed_tracks* space_tracks = compress(allocator, clip, transform_base_desc_t());

		ACL_IMPL_DEPRECATION_WARNING_PUSH
		output_stats stats;
		compressed_tracks* deprecated_tracks = nullptr;
		REQUIRE(compress_track_list(allocator, clip, settings, empty_base, additive_clip_format8::additive1, deprecated_tracks, stats).empty());
		ACL_IMPL_DEPRECATION_WARNING_POP
		REQUIRE(deprecated_tracks != nullptr);

		CHECK(are_equal(*space_tracks, *deprecated_tracks));

		allocator.deallocate(space_tracks, space_tracks->get_size());
		allocator.deallocate(deprecated_tracks, deprecated_tracks->get_size());
	}
}

TEST_CASE("pre-process with a transform space", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf base = make_base(allocator);

	pre_process_settings_t settings;
	settings.actions = pre_process_actions::recommended;
	settings.precision_policy = pre_process_precision_policy::lossy;

	// The transform space and the deprecated fields give the same result
	{
		track_array space_clip = make_clip(allocator);
		pre_process_settings_t space_settings = settings;
		space_settings.transform_base.format = additive_clip_format8::additive0;
		space_settings.transform_base.tracks = &base;
		REQUIRE(pre_process_track_list(allocator, space_settings, space_clip).empty());

		track_array deprecated_clip = make_clip(allocator);
		pre_process_settings_t deprecated_settings = settings;
		ACL_IMPL_DEPRECATION_WARNING_PUSH
		deprecated_settings.additive_base = &base;
		deprecated_settings.additive_format = additive_clip_format8::additive0;
		ACL_IMPL_DEPRECATION_WARNING_POP
		REQUIRE(pre_process_track_list(allocator, deprecated_settings, deprecated_clip).empty());

		const track_array_vqvf& space_clip_vqvf = track_array_cast<track_array_vqvf>(space_clip);
		const track_array_vqvf& deprecated_clip_vqvf = track_array_cast<track_array_vqvf>(deprecated_clip);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& space_transform = space_clip_vqvf[transform_index][sample_index];
				const rtm::vqvf& deprecated_transform = deprecated_clip_vqvf[transform_index][sample_index];
				CHECK(std::memcmp(&space_transform, &deprecated_transform, sizeof(rtm::vqvf)) == 0);
			}
		}
	}

	// The deprecated fields cannot be used with a base in the transform space
	{
		track_array clip = make_clip(allocator);
		pre_process_settings_t both_settings = settings;
		both_settings.transform_base.format = additive_clip_format8::additive0;
		both_settings.transform_base.tracks = &base;
		ACL_IMPL_DEPRECATION_WARNING_PUSH
		both_settings.additive_base = &base;
		both_settings.additive_format = additive_clip_format8::additive0;
		ACL_IMPL_DEPRECATION_WARNING_POP
		CHECK(pre_process_track_list(allocator, both_settings, clip).any());
	}

	// An invalid transform space returns an error
	{
		track_array clip = make_clip(allocator);
		pre_process_settings_t invalid_settings = settings;
		invalid_settings.transform_base.format = additive_clip_format8::additive0;
		CHECK(pre_process_track_list(allocator, invalid_settings, clip).any());
	}
}

TEST_CASE("compression in mesh space", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const compression_settings settings = get_default_compression_settings();

	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);

	const transform_base_desc_t transform_base;
	CHECK(transform_base.is_valid(transform_space_t::mesh).empty());

	compressed_tracks* tracks = compress(allocator, mesh_clip, transform_base);

	// The error of each transform is measured without its parent
	{
		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		const track_error error = calculate_compression_error(allocator, mesh_clip, context, settings, transform_base);
		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);
	}

	// Each decompressed transform is close to its mesh space reference: the error does not add up along the chain
	CHECK(find_max_translation_error(*tracks, mesh_clip, nullptr, additive_clip_format8::none) < 0.01F);

	// The overloads without a base use the transform space of the clip
	{
		output_stats stats;
		compressed_tracks* default_tracks = nullptr;
		REQUIRE(compress_track_list(allocator, mesh_clip, settings, default_tracks, stats).empty());
		REQUIRE(default_tracks != nullptr);

		CHECK(are_equal(*tracks, *default_tracks));

		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		const track_error base_error = calculate_compression_error(allocator, mesh_clip, context, settings, transform_base);
		const track_error default_error = calculate_compression_error(allocator, mesh_clip, context, settings);
		CHECK(default_error.error == base_error.error);

		allocator.deallocate(default_tracks, default_tracks->get_size());
	}

	// The same samples in local space measure the error along the chain: compression gives other data
	{
		track_array_vqvf local_space_clip = make_mesh_space_clip(allocator, local_clip);
		local_space_clip.set_transform_space(transform_space_t::local);

		compressed_tracks* local_tracks = compress(allocator, local_space_clip, transform_base);
		CHECK_FALSE(are_equal(*tracks, *local_tracks));

		allocator.deallocate(local_tracks, local_tracks->get_size());
	}

	allocator.deallocate(tracks, tracks->get_size());

	// The compressed tracks keep the parent indices of the clip
	{
		compression_settings metadata_settings = settings;
		metadata_settings.metadata.include_parent_track_indices = true;

		output_stats stats;
		compressed_tracks* metadata_tracks = nullptr;
		REQUIRE(compress_track_list(allocator, mesh_clip, metadata_settings, transform_base, metadata_tracks, stats).empty());
		REQUIRE(metadata_tracks != nullptr);

		CHECK(metadata_tracks->get_parent_track_index(0) == k_invalid_track_index);
		CHECK(metadata_tracks->get_parent_track_index(1) == 0);
		CHECK(metadata_tracks->get_parent_track_index(3) == 2);

		allocator.deallocate(metadata_tracks, metadata_tracks->get_size());
	}

	// Without a hierarchy, the local and mesh spaces give the same data
	{
		track_array_vqvf roots_clip = make_clip(allocator);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			roots_clip[transform_index].get_description().parent_index = k_invalid_track_index;

		compressed_tracks* local_tracks = compress(allocator, roots_clip, transform_base_desc_t());

		roots_clip.set_transform_space(transform_space_t::mesh);
		compressed_tracks* mesh_tracks = compress(allocator, roots_clip, transform_base);

		CHECK(are_equal(*local_tracks, *mesh_tracks));

		allocator.deallocate(local_tracks, local_tracks->get_size());
		allocator.deallocate(mesh_tracks, mesh_tracks->get_size());
	}
}

TEST_CASE("compression in mesh space with a base", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const compression_settings settings = get_default_compression_settings();

	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);

	// The base is a single local space pose with the transform hierarchy of the clip
	track_array_vqvf base(allocator, k_num_transforms);
	rtm::vqvf local_base_pose[k_num_transforms];
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		const float offset = float(transform_index) * 0.2F;
		local_base_pose[transform_index] = rtm::vqv_set(rtm::quat_from_euler(0.2F, offset, 0.1F), rtm::vector_set(0.5F, 0.0F, 0.2F), rtm::vector_set(1.0F));
		base[transform_index] = track_vqvf::make_copy(make_desc(transform_index), allocator, &local_base_pose[transform_index], 1, k_sample_rate);
	}

	rtm::vqvf mesh_base_pose[k_num_transforms];
	local_to_mesh_space(local_base_pose, mesh_base_pose);

	const additive_clip_format8 base_formats[] = { additive_clip_format8::relative, additive_clip_format8::additive0, additive_clip_format8::additive1 };
	for (const additive_clip_format8 base_format : base_formats)
	{
		// The additive clip is relative to the base in mesh space
		track_array_vqvf additive_clip(allocator, k_num_transforms);
		additive_clip.set_transform_space(transform_space_t::mesh);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			track_vqvf track = track_vqvf::make_reserve(make_desc(transform_index), allocator, k_num_samples, k_sample_rate);
			if (base_format == additive_clip_format8::additive1)
				track.get_description().default_value.scale = rtm::vector_set(0.0F);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& base_transform = mesh_base_pose[transform_index];
				const rtm::vqvf& mesh_transform = mesh_clip[transform_index][sample_index];

				rtm::vqvf additive_transform;
				if (base_format == additive_clip_format8::relative)
					additive_transform = convert_to_relative(base_transform, mesh_transform);
				else if (base_format == additive_clip_format8::additive0)
					additive_transform = convert_to_additive0(base_transform, mesh_transform);
				else
					additive_transform = convert_to_additive1(base_transform, mesh_transform);

				track[sample_index] = additive_transform;
			}

			additive_clip[transform_index] = std::move(track);
		}

		transform_base_desc_t transform_base;
		transform_base.format = base_format;
		transform_base.tracks = &base;

		compressed_tracks* tracks = compress(allocator, additive_clip, transform_base);

		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		const track_error error = calculate_compression_error(allocator, additive_clip, context, settings, transform_base);
		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);

		// Compression applies the base in mesh space: with it, we find the mesh space clip
		CHECK(find_max_translation_error(*tracks, mesh_clip, mesh_base_pose, base_format) < 0.01F);

		allocator.deallocate(tracks, tracks->get_size());
	}
}

TEST_CASE("the legacy error metric supports only the local space", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);

	const transform_base_desc_t transform_base;

	ACL_IMPL_DEPRECATION_WARNING_PUSH
	vqvf_transform_error_metric legacy_metric;

	compression_settings settings = get_default_compression_settings();
	settings.error_metric = &legacy_metric;

	output_stats stats;
	compressed_tracks* tracks = nullptr;
	CHECK(compress_track_list(allocator, mesh_clip, settings, transform_base, tracks, stats).any());
	CHECK(tracks == nullptr);

	track_array clip = make_mesh_space_clip(allocator, local_clip);
	pre_process_settings_t pre_process_settings;
	pre_process_settings.precision_policy = pre_process_precision_policy::lossy;
	pre_process_settings.error_metric = &legacy_metric;
	pre_process_settings.transform_base = transform_base;
	CHECK(pre_process_track_list(allocator, pre_process_settings, clip).any());
	ACL_IMPL_DEPRECATION_WARNING_POP
}

TEST_CASE("compression in mesh rotation space", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const compression_settings settings = get_default_compression_settings();

	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);		// The object space reference
	const track_array_vqvf clip = make_mesh_rotation_space_clip(allocator, local_clip);

	const transform_base_desc_t transform_base;
	CHECK(transform_base.is_valid(transform_space_t::mesh_rotation).empty());

	// The mesh rotation space does not support the relative base format
	{
		const track_array_vqvf base = make_base(allocator);
		transform_base_desc_t relative_base;
		relative_base.format = additive_clip_format8::relative;
		relative_base.tracks = &base;
		CHECK(relative_base.is_valid(transform_space_t::local).empty());
		CHECK(relative_base.is_valid(transform_space_t::mesh_rotation).any());

		output_stats stats;
		compressed_tracks* relative_tracks = nullptr;
		CHECK(compress_track_list(allocator, clip, settings, relative_base, relative_tracks, stats).any());
		CHECK(relative_tracks == nullptr);
	}

	compressed_tracks* tracks = compress(allocator, clip, transform_base);

	{
		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		const track_error error = calculate_compression_error(allocator, clip, context, settings, transform_base);
		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);
	}

	// With the mesh rotation product, the decompressed clip gives the object space reference
	CHECK(find_max_mesh_rotation_error(*tracks, mesh_clip, nullptr, additive_clip_format8::none) < 0.01F);

	allocator.deallocate(tracks, tracks->get_size());

	// A matrix has no rotation that we can replace: the matrix runtime transform type does not support the mesh rotation space
	{
		compression_settings matrix_settings = settings;
		matrix_settings.transform_runtime_type = transform_runtime_type_t::matrix3x4f;

		output_stats stats;
		compressed_tracks* matrix_tracks = nullptr;
		CHECK(compress_track_list(allocator, clip, matrix_settings, transform_base, matrix_tracks, stats).any());
		CHECK(matrix_tracks == nullptr);
	}

	// The VQM, VQV, VQS, and VQ runtime transform types support it
	// The VQ and VQS types use the mesh rotation cache, the VQM and VQV types use the transform chains
	// A metric that is not built-in uses the virtual functions of the transform adapter
	shell_transform_metric shell_metric;
	itransform_metric* metrics[] = { nullptr, &shell_metric };

	const track_array_vqsf vqs_clip = make_vqs_clip(allocator, clip);

	const transform_runtime_type_t types[] = { transform_runtime_type_t::vqmf, transform_runtime_type_t::vqvf, transform_runtime_type_t::vqsf, transform_runtime_type_t::vqf };
	for (itransform_metric* metric : metrics)
	{
		for (const transform_runtime_type_t type : types)
		{
			compression_settings type_settings = settings;
			type_settings.transform_runtime_type = type;
			type_settings.transform_metric = metric;

			output_stats stats;
			compressed_tracks* type_tracks = nullptr;
			// The 'vqsf' runtime type needs VQS tracks
			const track_array& type_clip = type == transform_runtime_type_t::vqsf ? static_cast<const track_array&>(vqs_clip) : static_cast<const track_array&>(clip);
			REQUIRE(compress_track_list(allocator, type_clip, type_settings, transform_base, type_tracks, stats).empty());
			REQUIRE(type_tracks != nullptr);

			{
				decompression_context<debug_transform_decompression_settings> context;
				REQUIRE(context.initialize(*type_tracks));

				const track_error error = calculate_compression_error(allocator, type_clip, context, type_settings, transform_base);
				CHECK(error.error >= 0.0F);
				CHECK(error.error < 0.01F);
			}

			// VQ and VQS arithmetic do not use the non-uniform scale: we compare with VQV and VQM arithmetic only
			if (type == transform_runtime_type_t::vqvf)
				CHECK(find_max_mesh_rotation_error(*type_tracks, mesh_clip, nullptr, additive_clip_format8::none) < 0.01F);
			else if (type == transform_runtime_type_t::vqmf)
				CHECK(find_max_mesh_rotation_error_vqm(*type_tracks, local_clip) < 0.01F);

			allocator.deallocate(type_tracks, type_tracks->get_size());
		}
	}
}

TEST_CASE("compression in mesh rotation space with a base", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const compression_settings settings = get_default_compression_settings();

	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);		// The object space reference
	const track_array_vqvf clip = make_mesh_rotation_space_clip(allocator, local_clip);

	// The base is a single local space pose with the transform hierarchy of the clip
	track_array_vqvf base(allocator, k_num_transforms);
	rtm::vqvf local_base_pose[k_num_transforms];
	for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
	{
		const float offset = float(transform_index) * 0.2F;
		local_base_pose[transform_index] = rtm::vqv_set(rtm::quat_from_euler(0.2F, offset, 0.1F), rtm::vector_set(0.5F, 0.0F, 0.2F), rtm::vector_set(1.1F, 0.9F, 1.0F));
		base[transform_index] = track_vqvf::make_copy(make_desc(transform_index), allocator, &local_base_pose[transform_index], 1, k_sample_rate);
	}

	rtm::vqvf base_pose[k_num_transforms];
	local_to_mesh_rotation_space(local_base_pose, base_pose);

	const additive_clip_format8 base_formats[] = { additive_clip_format8::additive0, additive_clip_format8::additive1 };
	for (const additive_clip_format8 base_format : base_formats)
	{
		// The additive clip is relative to the base in mesh rotation space
		track_array_vqvf additive_clip(allocator, k_num_transforms);
		additive_clip.set_transform_space(transform_space_t::mesh_rotation);
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			track_vqvf track = track_vqvf::make_reserve(make_desc(transform_index), allocator, k_num_samples, k_sample_rate);
			if (base_format == additive_clip_format8::additive1)
				track.get_description().default_value.scale = rtm::vector_set(0.0F);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& base_transform = base_pose[transform_index];
				const rtm::vqvf& transform = clip[transform_index][sample_index];

				if (base_format == additive_clip_format8::additive0)
					track[sample_index] = convert_to_additive0(base_transform, transform);
				else
					track[sample_index] = convert_to_additive1(base_transform, transform);
			}

			additive_clip[transform_index] = std::move(track);
		}

		transform_base_desc_t transform_base;
		transform_base.format = base_format;
		transform_base.tracks = &base;

		compressed_tracks* tracks = compress(allocator, additive_clip, transform_base);

		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		const track_error error = calculate_compression_error(allocator, additive_clip, context, settings, transform_base);
		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);

		// Compression applies the base in mesh rotation space: with it, we find the object space reference
		CHECK(find_max_mesh_rotation_error(*tracks, mesh_clip, base_pose, base_format) < 0.01F);

		allocator.deallocate(tracks, tracks->get_size());
	}
}

namespace
{
	// A base adapter that combines the transforms with the math of a built-in base format
	class builtin_base_adapter_t final : public itransform_base_adapter_t
	{
	public:
		explicit builtin_base_adapter_t(additive_clip_format8 base_format) : m_base_format(base_format) {}

		virtual const char* get_name() const override { return "builtin_base_adapter_t"; }
		virtual float get_default_scale() const override { return m_base_format == additive_clip_format8::additive1 ? 0.0F : 1.0F; }

		virtual rtm::vqvf apply_base(uint32_t transform_index, const rtm::vqvf& base_transform, const rtm::vqvf& transform) const override
		{
			(void)transform_index;
			return apply_additive_to_base(m_base_format, base_transform, transform);
		}

	private:
		additive_clip_format8 m_base_format;
	};

	// A base adapter that combines the transforms as 'additive0', but it multiplies the translation of
	// each transform except the root by 100. The root does not use the base.
	class scaled_base_adapter_t final : public itransform_base_adapter_t
	{
	public:
		virtual const char* get_name() const override { return "scaled_base_adapter_t"; }

		virtual rtm::vqvf apply_base(uint32_t transform_index, const rtm::vqvf& base_transform, const rtm::vqvf& transform) const override
		{
			if (transform_index == 0)
				return transform;

			const rtm::vqvf scaled_transform = rtm::vqv_set(transform.rotation, rtm::vector_mul(transform.translation, 100.0F), transform.scale);
			return transform_add0(base_transform, scaled_transform);
		}
	};

	// A base adapter with a default scale that is not valid
	class invalid_default_scale_base_adapter_t final : public itransform_base_adapter_t
	{
	public:
		virtual const char* get_name() const override { return "invalid_default_scale_base_adapter_t"; }
		virtual float get_default_scale() const override { return 0.5F; }
		virtual rtm::vqvf apply_base(uint32_t transform_index, const rtm::vqvf& base_transform, const rtm::vqvf& transform) const override
		{
			(void)transform_index;
			return transform_add0(base_transform, transform);
		}
	};

	// Returns a copy of a clip with VQ tracks: the tracks have no scale
	// The copy has the transform space of the clip
	track_array_vqf make_vq_clip(iallocator& allocator, const track_array_vqvf& vqv_clip)
	{
		track_array_vqf clip(allocator, k_num_transforms);
		clip.set_transform_space(vqv_clip.get_transform_space());
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const track_vqvf& vqv_track = vqv_clip[transform_index];
			track_vqf vq_track = track_vqf::make_reserve(vqv_track.get_description(), allocator, k_num_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const rtm::vqvf& transform = vqv_track[sample_index];
				vq_track[sample_index] = rtm::vq_set(transform.rotation, transform.translation);
			}

			clip[transform_index] = std::move(vq_track);
		}

		return clip;
	}

	// Returns a base without scale with the transform hierarchy of the clip
	track_array_vqvf make_base_without_scale(iallocator& allocator)
	{
		track_array_vqvf base = make_base(allocator);
		const uint32_t num_base_samples = base.get_num_samples_per_track();

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			for (uint32_t sample_index = 0; sample_index < num_base_samples; ++sample_index)
			{
				rtm::vqvf& transform = base[transform_index][sample_index];
				transform.scale = rtm::vector_set(1.0F);
			}
		}

		return base;
	}
}

TEST_CASE("custom base format names", "[compression][transform_space]")
{
	CHECK(std::strcmp(get_additive_clip_format_name(additive_clip_format8::custom), "custom") == 0);

	additive_clip_format8 base_format = additive_clip_format8::none;
	CHECK(get_additive_clip_format("custom", base_format));
	CHECK(base_format == additive_clip_format8::custom);
}

TEST_CASE("transform_base_desc_t validation with a base adapter", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf base = make_base(allocator);
	const builtin_base_adapter_t additive0_adapter(additive_clip_format8::additive0);
	const builtin_base_adapter_t additive1_adapter(additive_clip_format8::additive1);
	const invalid_default_scale_base_adapter_t invalid_adapter;

	{
		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::custom;
		transform_base.tracks = &base;
		CHECK(transform_base.has_base());
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// No base adapter

		transform_base.adapter = &additive0_adapter;
		CHECK(transform_base.is_valid(transform_space_t::local).empty());

		transform_base.adapter = &additive1_adapter;
		CHECK(transform_base.is_valid(transform_space_t::local).empty());

		transform_base.adapter = &invalid_adapter;
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// The default scale is not 0.0 or 1.0

		transform_base.adapter = &additive0_adapter;
		transform_base.tracks = nullptr;
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// No base
	}

	{
		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::additive0;
		transform_base.tracks = &base;
		transform_base.adapter = &additive0_adapter;
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// A base adapter without the custom base format

		transform_base.format = additive_clip_format8::none;
		transform_base.tracks = nullptr;
		CHECK(transform_base.is_valid(transform_space_t::local).any());	// A base adapter without a base
	}

	{
		// The mesh rotation space supports the custom base format
		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::custom;
		transform_base.tracks = &base;
		transform_base.adapter = &additive0_adapter;
		CHECK(transform_base.is_valid(transform_space_t::mesh_rotation).empty());
	}
}

TEST_CASE("compression with a custom base format", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const compression_settings settings = get_default_compression_settings();

	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);
	const track_array_vqvf mesh_rotation_clip = make_mesh_rotation_space_clip(allocator, local_clip);
	const track_array_vqvf base = make_base(allocator);
	const track_array_vqvf base_without_scale = make_base_without_scale(allocator);

	// With the math of a built-in base format, a base adapter gives the same compressed data and the same error
	const transform_space_t spaces[] = { transform_space_t::local, transform_space_t::mesh, transform_space_t::mesh_rotation };
	const additive_clip_format8 base_formats[] = { additive_clip_format8::relative, additive_clip_format8::additive0, additive_clip_format8::additive1 };
	for (const transform_space_t space : spaces)
	{
		const track_array_vqvf* vqv_clip_ptr = &local_clip;
		if (space == transform_space_t::mesh)
			vqv_clip_ptr = &mesh_clip;
		else if (space == transform_space_t::mesh_rotation)
			vqv_clip_ptr = &mesh_rotation_clip;

		const track_array_vqvf& vqv_clip = *vqv_clip_ptr;
		const track_array_vqf vq_clip = make_vq_clip(allocator, vqv_clip);

		for (const additive_clip_format8 base_format : base_formats)
		{
			if (space == transform_space_t::mesh_rotation && base_format == additive_clip_format8::relative)
				continue;	// Not supported

			const builtin_base_adapter_t base_adapter(base_format);

			// A clip with scale, and a clip without scale with a base without scale
			const track_array* clips[] = { &vqv_clip, &vq_clip };
			const track_array* bases[] = { &base, &base_without_scale };
			for (uint32_t clip_index = 0; clip_index < 2; ++clip_index)
			{
				INFO("space: " << get_transform_space_name(space) << ", base format: " << get_additive_clip_format_name(base_format) << ", clip: " << clip_index);

				const track_array& clip = *clips[clip_index];

				transform_base_desc_t builtin_base;
				builtin_base.format = base_format;
				builtin_base.tracks = bases[clip_index];

				transform_base_desc_t custom_base = builtin_base;
				custom_base.format = additive_clip_format8::custom;
				custom_base.adapter = &base_adapter;

				compressed_tracks* builtin_tracks = compress(allocator, clip, builtin_base);
				compressed_tracks* custom_tracks = compress(allocator, clip, custom_base);

				CHECK(are_equal(*builtin_tracks, *custom_tracks));
				CHECK(custom_tracks->get_default_scale() == (base_format == additive_clip_format8::additive1 ? 0 : 1));

				decompression_context<debug_transform_decompression_settings> context;
				REQUIRE(context.initialize(*custom_tracks));

				const track_error builtin_error = calculate_compression_error(allocator, clip, context, settings, builtin_base);
				const track_error custom_error = calculate_compression_error(allocator, clip, context, settings, custom_base);
				CHECK(custom_error.error == builtin_error.error);
				CHECK(custom_error.error < 0.01F);

				allocator.deallocate(builtin_tracks, builtin_tracks->get_size());
				allocator.deallocate(custom_tracks, custom_tracks->get_size());
			}
		}
	}

	// Compression and the error measurement use the base adapter
	{
		const scaled_base_adapter_t base_adapter;

		transform_base_desc_t builtin_base;
		builtin_base.format = additive_clip_format8::additive0;
		builtin_base.tracks = &base;

		transform_base_desc_t custom_base = builtin_base;
		custom_base.format = additive_clip_format8::custom;
		custom_base.adapter = &base_adapter;

		compressed_tracks* builtin_tracks = compress(allocator, local_clip, builtin_base);
		compressed_tracks* custom_tracks = compress(allocator, local_clip, custom_base);

		decompression_context<debug_transform_decompression_settings> builtin_context;
		REQUIRE(builtin_context.initialize(*builtin_tracks));

		decompression_context<debug_transform_decompression_settings> custom_context;
		REQUIRE(custom_context.initialize(*custom_tracks));

		// With the base adapter, the error of the clip that we compress with the base adapter is small
		const track_error custom_error = calculate_compression_error(allocator, local_clip, custom_context, settings, custom_base);
		CHECK(custom_error.error < 0.01F);

		// The clip that we compress with 'additive0' does not keep enough precision for the base adapter
		const track_error builtin_error = calculate_compression_error(allocator, local_clip, builtin_context, settings, custom_base);
		CHECK(custom_error.error < builtin_error.error);

		// The base adapter needs more precision
		CHECK(custom_tracks->get_size() > builtin_tracks->get_size());

		allocator.deallocate(builtin_tracks, builtin_tracks->get_size());
		allocator.deallocate(custom_tracks, custom_tracks->get_size());
	}
}

TEST_CASE("pre-process with a custom base format", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf base = make_base(allocator);

	pre_process_settings_t settings;
	settings.actions = pre_process_actions::recommended;
	settings.precision_policy = pre_process_precision_policy::lossy;

	// With the math of a built-in base format, a base adapter gives the same result
	const additive_clip_format8 base_formats[] = { additive_clip_format8::relative, additive_clip_format8::additive0, additive_clip_format8::additive1 };
	for (const additive_clip_format8 base_format : base_formats)
	{
		INFO("base format: " << get_additive_clip_format_name(base_format));

		const builtin_base_adapter_t base_adapter(base_format);

		track_array builtin_clip = make_clip(allocator);
		pre_process_settings_t builtin_settings = settings;
		builtin_settings.transform_base.format = base_format;
		builtin_settings.transform_base.tracks = &base;
		REQUIRE(pre_process_track_list(allocator, builtin_settings, builtin_clip).empty());

		track_array custom_clip = make_clip(allocator);
		pre_process_settings_t custom_settings = builtin_settings;
		custom_settings.transform_base.format = additive_clip_format8::custom;
		custom_settings.transform_base.adapter = &base_adapter;
		REQUIRE(pre_process_track_list(allocator, custom_settings, custom_clip).empty());

		REQUIRE(builtin_clip.get_track_type() == custom_clip.get_track_type());
		REQUIRE(builtin_clip.get_num_tracks() == custom_clip.get_num_tracks());
		REQUIRE(builtin_clip.get_num_samples_per_track() == custom_clip.get_num_samples_per_track());

		const uint32_t num_samples = builtin_clip.get_num_samples_per_track();
		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			const track& builtin_track = builtin_clip[transform_index];
			const track& custom_track = custom_clip[transform_index];
			REQUIRE(builtin_track.get_sample_size() == custom_track.get_sample_size());

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				CHECK(std::memcmp(builtin_track[sample_index], custom_track[sample_index], builtin_track.get_sample_size()) == 0);
		}
	}

	// The custom base format requires a base adapter
	{
		track_array clip = make_clip(allocator);
		pre_process_settings_t invalid_settings = settings;
		invalid_settings.transform_base.format = additive_clip_format8::custom;
		invalid_settings.transform_base.tracks = &base;
		CHECK(pre_process_track_list(allocator, invalid_settings, clip).any());
	}
}

TEST_CASE("the legacy error metric does not support the custom base format", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf clip = make_clip(allocator);
	const track_array_vqvf base = make_base(allocator);
	const builtin_base_adapter_t base_adapter(additive_clip_format8::additive0);

	transform_base_desc_t transform_base;
	transform_base.format = additive_clip_format8::custom;
	transform_base.tracks = &base;
	transform_base.adapter = &base_adapter;

	ACL_IMPL_DEPRECATION_WARNING_PUSH
	vqvf_transform_error_metric legacy_metric;

	compression_settings settings = get_default_compression_settings();
	settings.error_metric = &legacy_metric;

	output_stats stats;
	compressed_tracks* tracks = nullptr;
	CHECK(compress_track_list(allocator, clip, settings, transform_base, tracks, stats).any());
	CHECK(tracks == nullptr);

	track_array pre_process_clip = make_clip(allocator);
	pre_process_settings_t pre_process_settings;
	pre_process_settings.precision_policy = pre_process_precision_policy::lossy;
	pre_process_settings.error_metric = &legacy_metric;
	pre_process_settings.transform_base = transform_base;
	CHECK(pre_process_track_list(allocator, pre_process_settings, pre_process_clip).any());

	// The deprecated overload has no base adapter
	const compression_settings default_settings = get_default_compression_settings();
	CHECK(compress_track_list(allocator, clip, default_settings, base, additive_clip_format8::custom, tracks, stats).any());
	CHECK(tracks == nullptr);
	ACL_IMPL_DEPRECATION_WARNING_POP
}
