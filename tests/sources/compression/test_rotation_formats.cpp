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
#include <acl/core/ansi_allocator.h>
#include <acl/core/compressed_tracks_version.h>
#include <acl/core/track_formats.h>
#include <acl/core/impl/debug_track_writer.h>
#include <acl/decompression/decompress.h>

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
#include <rtm/vqvf.h>

#include <cmath>
#include <cstdint>
#include <utility>

using namespace acl;

TEST_CASE("drop largest rotation format names and helpers", "[core][formats]")
{
	const rotation_format8 formats[] = { rotation_format8::quatf_drop_largest_full, rotation_format8::quatf_drop_largest_variable };
	for (const rotation_format8 format : formats)
	{
		rotation_format8 parsed_format = rotation_format8::quatf_full;
		CHECK(get_rotation_format(get_rotation_format_name(format), parsed_format));
		CHECK(parsed_format == format);
		CHECK(get_rotation_variant(format) == rotation_variant8::quat_drop_largest);
	}

	CHECK(get_highest_variant_precision(rotation_variant8::quat_drop_largest) == rotation_format8::quatf_drop_largest_full);
	CHECK(is_rotation_format_full_precision(rotation_format8::quatf_drop_largest_full));
	CHECK(!is_rotation_format_variable(rotation_format8::quatf_drop_largest_full));
	CHECK(is_rotation_format_variable(rotation_format8::quatf_drop_largest_variable));
	CHECK(!is_rotation_format_full_precision(rotation_format8::quatf_drop_largest_variable));

	// The default settings use the drop largest format
	CHECK(get_default_compression_settings().rotation_format == rotation_format8::quatf_drop_largest_variable);
	CHECK(default_transform_decompression_settings::is_rotation_format_supported(rotation_format8::quatf_drop_largest_variable));
}

namespace
{
	constexpr uint32_t k_num_samples = 64;		// More than one segment
	constexpr float k_sample_rate = 30.0F;
	constexpr float k_precision = 0.01F;

	struct rotation_test_track
	{
		uint32_t parent_index;
		rtm::vector4f translation;
		int rotation_kind;
	};

	// The rotation of a sample, each kind tests a case where W is small
	rtm::quatf get_test_rotation(int rotation_kind, uint32_t sample_index)
	{
		const float alpha = float(sample_index) / float(k_num_samples - 1);
		switch (rotation_kind)
		{
		case 0:		// Animated: a full turn around Y, W crosses 0
			return rtm::quat_from_axis_angle(rtm::vector_set(0.0F, 1.0F, 0.0F), alpha * rtm::constants::two_pi());
		case 1:		// Animated: a flip of 180 degrees in 3 samples inside a segment
		{
			const float flip_alpha = rtm::scalar_clamp((float(sample_index) - 20.0F) / 3.0F, 0.0F, 1.0F);
			return rtm::quat_from_axis_angle(rtm::vector_normalize3(rtm::vector_set(1.0F, -0.4F, 0.2F)), 0.3F + flip_alpha * rtm::constants::pi());
		}
		case 2:		// Constant: a rotation of 180 degrees, W is near 0
			return rtm::quat_normalize(rtm::quat_set(-4.3E-17F, 0.7071068F, -0.7071068F, 4.3E-17F));
		case 3:		// Constant: W is the largest component
			return rtm::quat_from_axis_angle(rtm::vector_set(0.0F, 0.0F, 1.0F), 0.4F);
		default:	// Animated: a small rotation, W is the largest component
			return rtm::quat_from_axis_angle(rtm::vector_set(1.0F, 0.0F, 0.0F), 0.2F * std::sin(alpha * 7.0F));
		}
	}

	track_array_vqvf make_rotation_test_clip(iallocator& allocator)
	{
		// The children are far from their parents: an error on a rotation becomes large
		const rotation_test_track tracks[] =
		{
			{ k_invalid_track_index, rtm::vector_zero(), 2 },
			{ 0, rtm::vector_set(0.0F, 50.0F, 0.0F), 0 },
			{ 1, rtm::vector_set(40.0F, 0.0F, 0.0F), 1 },
			{ 2, rtm::vector_set(0.0F, 0.0F, 30.0F), 3 },
			{ 3, rtm::vector_set(20.0F, 0.0F, 0.0F), 4 },
		};
		constexpr uint32_t num_tracks = sizeof(tracks) / sizeof(tracks[0]);

		track_array_vqvf track_list(allocator, num_tracks);
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			track_desc_transformf desc;
			desc.output_index = track_index;
			desc.parent_index = tracks[track_index].parent_index;
			desc.precision = k_precision;
			desc.shell_distance = 3.0F;

			track_vqvf track = track_vqvf::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
				track[sample_index] = rtm::vqv_set(get_test_rotation(tracks[track_index].rotation_kind, sample_index), tracks[track_index].translation, rtm::vector_set(1.0F));

			track_list[track_index] = std::move(track);
		}

		return track_list;
	}

	template<class decompression_settings_type>
	void decompress_rotations(const compressed_tracks& tracks, uint32_t num_tracks, bool per_track, rtm::quatf* out_rotations)
	{
		ansi_allocator allocator;
		decompression_context<decompression_settings_type> context;
		REQUIRE(context.initialize(tracks));

		acl_impl::debug_track_writer writer(allocator, track_type8::vqvf, num_tracks);
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
		{
			context.seek(float(sample_index) / k_sample_rate, sample_rounding_policy::nearest);

			if (per_track)
			{
				for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
					context.decompress_track(track_index, writer);
			}
			else
				context.decompress_tracks(writer);

			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
				out_rotations[(sample_index * num_tracks) + track_index] = writer.read_vqv(track_index).rotation;
		}
	}

	// Returns the largest difference between the components of two rotations, q and -q are the same rotation
	float get_largest_difference(const rtm::quatf* rotations0, const rtm::quatf* rotations1, uint32_t num_rotations)
	{
		float largest_difference = 0.0F;
		for (uint32_t index = 0; index < num_rotations; ++index)
		{
			const rtm::vector4f rotation0 = rtm::quat_to_vector(rotations0[index]);
			rtm::vector4f rotation1 = rtm::quat_to_vector(rotations1[index]);
			if (float(rtm::vector_dot(rotation0, rotation1)) < 0.0F)
				rotation1 = rtm::vector_neg(rotation1);

			const rtm::vector4f difference = rtm::vector_abs(rtm::vector_sub(rotation0, rotation1));
			largest_difference = rtm::scalar_max(largest_difference, float(rtm::vector_get_max_component(difference)));
		}

		return largest_difference;
	}
}

TEST_CASE("drop largest rotation formats compression", "[compression][formats]")
{
	ansi_allocator allocator;

	const track_array_vqvf track_list = make_rotation_test_clip(allocator);
	const uint32_t num_tracks = track_list.get_num_tracks();

	const rotation_format8 formats[] =
	{
		rotation_format8::quatf_drop_w_full,
		rotation_format8::quatf_drop_w_variable,
		rotation_format8::quatf_drop_largest_full,
		rotation_format8::quatf_drop_largest_variable,
	};

	float errors[4] = { 0.0F, 0.0F, 0.0F, 0.0F };
	for (uint32_t format_index = 0; format_index < 4; ++format_index)
	{
		const rotation_format8 format = formats[format_index];
		INFO("rotation format: " << get_rotation_format_name(format));

		compression_settings settings = get_default_compression_settings();
		settings.rotation_format = format;

		output_stats stats;
		compressed_tracks* tracks = nullptr;
		REQUIRE(compress_track_list(allocator, track_list, settings, tracks, stats).empty());
		REQUIRE(tracks != nullptr);
		CHECK(tracks->is_valid(true).empty());
		CHECK(tracks->get_version() == compressed_tracks_version16::latest);

		decompression_context<debug_transform_decompression_settings> context;
		REQUIRE(context.initialize(*tracks));

		errors[format_index] = calculate_compression_error(allocator, track_list, context, settings).error;

		// Each track and all the tracks give the same rotations, within the rounding of the code paths
		rtm::quatf* rotations_all = allocate_type_array<rtm::quatf>(allocator, size_t(k_num_samples) * num_tracks);
		rtm::quatf* rotations_single = allocate_type_array<rtm::quatf>(allocator, size_t(k_num_samples) * num_tracks);
		decompress_rotations<debug_transform_decompression_settings>(*tracks, num_tracks, false, rotations_all);
		decompress_rotations<debug_transform_decompression_settings>(*tracks, num_tracks, true, rotations_single);
		CHECK(get_largest_difference(rotations_all, rotations_single, k_num_samples * num_tracks) < 1.0E-5F);

		// The default decompression settings support the default rotation format
		if (format == rotation_format8::quatf_drop_largest_variable)
		{
			rtm::quatf* rotations_default = allocate_type_array<rtm::quatf>(allocator, size_t(k_num_samples) * num_tracks);
			decompress_rotations<default_transform_decompression_settings>(*tracks, num_tracks, false, rotations_default);
			CHECK(get_largest_difference(rotations_all, rotations_default, k_num_samples * num_tracks) < 1.0E-5F);
			deallocate_type_array(allocator, rotations_default, size_t(k_num_samples) * num_tracks);
		}

		deallocate_type_array(allocator, rotations_all, size_t(k_num_samples) * num_tracks);
		deallocate_type_array(allocator, rotations_single, size_t(k_num_samples) * num_tracks);
		allocator.deallocate(tracks, tracks->get_size());
	}

	INFO("errors: drop W full " << errors[0] << ", drop W variable " << errors[1] << ", drop largest full " << errors[2] << ", drop largest variable " << errors[3]);

	// With W near 0, the drop W formats cannot reach the precision
	CHECK(errors[0] > k_precision);

	// The drop largest formats keep the precision
	CHECK(errors[2] <= k_precision);
	CHECK(errors[3] <= k_precision);
}
