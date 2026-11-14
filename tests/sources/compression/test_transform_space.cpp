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
#include <acl/compression/transform_space.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/core/additive_utils.h>
#include <acl/core/ansi_allocator.h>
#include <acl/core/track_writer.h>
#include <acl/decompression/decompress.h>

#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <cstring>

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

	compressed_tracks* compress(iallocator& allocator, const track_array& tracks, const transform_space_desc_t& transform_space)
	{
		const compression_settings settings = get_default_compression_settings();

		output_stats stats;
		compressed_tracks* compressed_tracks_ = nullptr;
		const error_result result = compress_track_list(allocator, tracks, settings, transform_space, compressed_tracks_, stats);
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
	// The tracks keep their parent index
	track_array_vqvf make_mesh_space_clip(iallocator& allocator, const track_array_vqvf& local_clip)
	{
		track_array_vqvf mesh_clip(allocator, k_num_transforms);
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
	struct pose_writer final : public track_writer
	{
		rtm::vqvf pose[k_num_transforms];

		void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation) { pose[track_index].rotation = rotation; }
		void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation) { pose[track_index].translation = translation; }
		void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale) { pose[track_index].scale = scale; }
	};

	// Returns the largest distance between the reference and the decompressed translations, after we apply the base
	// 'mesh_base_pose' is the base in mesh space, or null without a base
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

TEST_CASE("transform_space names", "[compression][transform_space]")
{
	CHECK(std::strcmp(get_transform_space_name(transform_space_t::local), "local") == 0);

	transform_space_t space = transform_space_t::local;
	CHECK(get_transform_space("local", space));
	CHECK(space == transform_space_t::local);
	CHECK(std::strcmp(get_transform_space_name(transform_space_t::mesh), "mesh") == 0);
	CHECK(get_transform_space("mesh", space));
	CHECK(space == transform_space_t::mesh);

	// A longer name is not a match
	CHECK_FALSE(get_transform_space("mesh_space", space));
	CHECK_FALSE(get_transform_space("locals", space));

	// The name does not need a null terminator after the match
	CHECK(get_transform_space("local\" = 1", space));
	CHECK(space == transform_space_t::local);
}

TEST_CASE("transform_space_desc_t validation", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const track_array_vqvf base = make_base(allocator);
	const track_array_vqvf empty_base;

	{
		const transform_space_desc_t transform_space;
		CHECK(transform_space.space == transform_space_t::local);
		CHECK_FALSE(transform_space.has_base());
		CHECK(transform_space.is_valid().empty());
	}

	{
		transform_space_desc_t transform_space;
		transform_space.base_format = additive_clip_format8::additive1;
		transform_space.base = &base;
		CHECK(transform_space.has_base());
		CHECK(transform_space.is_valid().empty());
	}

	{
		transform_space_desc_t transform_space;
		transform_space.base_format = additive_clip_format8::relative;
		CHECK(transform_space.is_valid().any());	// No base

		transform_space.base = &empty_base;
		CHECK(transform_space.is_valid().any());	// Empty base
	}

	{
		transform_space_desc_t transform_space;
		transform_space.base = &base;
		CHECK(transform_space.is_valid().any());	// A base without a base format
	}

	{
		transform_space_desc_t transform_space;
		transform_space.space = static_cast<transform_space_t>(200);
		CHECK(transform_space.is_valid().any());

		transform_space.space = transform_space_t::local;
		transform_space.base_format = static_cast<additive_clip_format8>(200);
		transform_space.base = &base;
		CHECK(transform_space.is_valid().any());
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
		transform_space_desc_t transform_space;
		transform_space.base_format = additive_clip_format8::additive0;

		output_stats stats;
		compressed_tracks* compressed_tracks_ = nullptr;
		CHECK(compress_track_list(allocator, clip, settings, transform_space, compressed_tracks_, stats).any());
		CHECK(compressed_tracks_ == nullptr);
	}

	// Without a base, the default transform space gives the same data as the overload without a transform space
	{
		compressed_tracks* space_tracks = compress(allocator, clip, transform_space_desc_t());

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
		transform_space_desc_t transform_space;
		transform_space.base_format = base_format;
		transform_space.base = &base;

		compressed_tracks* space_tracks = compress(allocator, clip, transform_space);
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

		const track_error error = calculate_compression_error(allocator, clip, context, settings, transform_space);
		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);

		allocator.deallocate(space_tracks, space_tracks->get_size());
		allocator.deallocate(deprecated_tracks, deprecated_tracks->get_size());
	}

	// The deprecated overload ignores an empty base
	{
		const track_array_vqvf empty_base;
		compressed_tracks* space_tracks = compress(allocator, clip, transform_space_desc_t());

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
		space_settings.transform_space.base_format = additive_clip_format8::additive0;
		space_settings.transform_space.base = &base;
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
		both_settings.transform_space.base_format = additive_clip_format8::additive0;
		both_settings.transform_space.base = &base;
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
		invalid_settings.transform_space.base_format = additive_clip_format8::additive0;
		CHECK(pre_process_track_list(allocator, invalid_settings, clip).any());
	}
}

TEST_CASE("compression in mesh space", "[compression][transform_space]")
{
	ansi_allocator allocator;
	const compression_settings settings = get_default_compression_settings();

	const track_array_vqvf local_clip = make_clip(allocator);
	const track_array_vqvf mesh_clip = make_mesh_space_clip(allocator, local_clip);

	transform_space_desc_t transform_space;
	transform_space.space = transform_space_t::mesh;
	CHECK(transform_space.is_valid().empty());

	compressed_tracks* tracks = compress(allocator, mesh_clip, transform_space);

	// The error of each transform is measured without its parent
	{
		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		const track_error error = calculate_compression_error(allocator, mesh_clip, context, settings, transform_space);
		CHECK(error.error >= 0.0F);
		CHECK(error.error < 0.01F);
	}

	// Each decompressed transform is close to its mesh space reference: the error does not add up along the chain
	CHECK(find_max_translation_error(*tracks, mesh_clip, nullptr, additive_clip_format8::none) < 0.01F);

	allocator.deallocate(tracks, tracks->get_size());

	// The compressed tracks keep the parent indices of the clip
	{
		compression_settings metadata_settings = settings;
		metadata_settings.metadata.include_parent_track_indices = true;

		output_stats stats;
		compressed_tracks* metadata_tracks = nullptr;
		REQUIRE(compress_track_list(allocator, mesh_clip, metadata_settings, transform_space, metadata_tracks, stats).empty());
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

		compressed_tracks* local_tracks = compress(allocator, roots_clip, transform_space_desc_t());
		compressed_tracks* mesh_tracks = compress(allocator, roots_clip, transform_space);

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

		transform_space_desc_t transform_space;
		transform_space.space = transform_space_t::mesh;
		transform_space.base_format = base_format;
		transform_space.base = &base;

		compressed_tracks* tracks = compress(allocator, additive_clip, transform_space);

		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		const track_error error = calculate_compression_error(allocator, additive_clip, context, settings, transform_space);
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

	transform_space_desc_t transform_space;
	transform_space.space = transform_space_t::mesh;

	ACL_IMPL_DEPRECATION_WARNING_PUSH
	const vqvf_transform_error_metric legacy_metric;

	compression_settings settings = get_default_compression_settings();
	settings.error_metric = const_cast<vqvf_transform_error_metric*>(&legacy_metric);

	output_stats stats;
	compressed_tracks* tracks = nullptr;
	CHECK(compress_track_list(allocator, mesh_clip, settings, transform_space, tracks, stats).any());
	CHECK(tracks == nullptr);

	track_array clip = make_mesh_space_clip(allocator, local_clip);
	pre_process_settings_t pre_process_settings;
	pre_process_settings.precision_policy = pre_process_precision_policy::lossy;
	pre_process_settings.error_metric = &legacy_metric;
	pre_process_settings.transform_space = transform_space;
	CHECK(pre_process_track_list(allocator, pre_process_settings, clip).any());
	ACL_IMPL_DEPRECATION_WARNING_POP
}
