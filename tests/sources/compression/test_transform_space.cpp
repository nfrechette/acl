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
#include <acl/core/ansi_allocator.h>
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
}

TEST_CASE("transform_space names", "[compression][transform_space]")
{
	CHECK(std::strcmp(get_transform_space_name(transform_space_t::local), "local") == 0);

	transform_space_t space = transform_space_t::local;
	CHECK(get_transform_space("local", space));
	CHECK(space == transform_space_t::local);
	CHECK_FALSE(get_transform_space("mesh_space", space));
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
