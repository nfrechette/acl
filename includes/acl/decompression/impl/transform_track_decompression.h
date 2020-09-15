#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/bitset.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_formats.h"
#include "acl/core/track_writer.h"
#include "acl/core/variable_bit_rates.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/impl/transform_animated_track_cache.h"
#include "acl/decompression/impl/transform_constant_track_cache.h"
#include "acl/decompression/impl/transform_decompression_context.h"
#include "acl/math/quatf.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4f.h"

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		template<class decompression_settings_type>
		inline bool initialize_v0(persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks)
		{
			ACL_ASSERT(tracks.get_algorithm_type() == algorithm_type8::uniformly_sampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(tracks.get_algorithm_type()), get_algorithm_name(algorithm_type8::uniformly_sampled));

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const tracks_header& header = get_tracks_header(tracks);
			const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

			const rotation_format8 packed_rotation_format = header.get_rotation_format();
			const vector_format8 packed_translation_format = header.get_translation_format();
			const vector_format8 packed_scale_format = header.get_scale_format();
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(packed_rotation_format);
			const vector_format8 translation_format = get_vector_format<translation_adapter>(packed_translation_format);
			const vector_format8 scale_format = get_vector_format<scale_adapter>(packed_scale_format);

			ACL_ASSERT(rotation_format == packed_rotation_format, "Statically compiled rotation format (%s) differs from the compressed rotation format (%s)!", get_rotation_format_name(rotation_format), get_rotation_format_name(packed_rotation_format));
			ACL_ASSERT(translation_format == packed_translation_format, "Statically compiled translation format (%s) differs from the compressed translation format (%s)!", get_vector_format_name(translation_format), get_vector_format_name(packed_translation_format));
			ACL_ASSERT(scale_format == packed_scale_format, "Statically compiled scale format (%s) differs from the compressed scale format (%s)!", get_vector_format_name(scale_format), get_vector_format_name(packed_scale_format));

			context.tracks = &tracks;
			context.clip_hash = tracks.get_hash();
			context.clip_duration = calculate_duration(header.num_samples, header.sample_rate);
			context.sample_time = -1.0F;
			context.default_tracks_bitset = transform_header.get_default_tracks_bitset();

			context.constant_tracks_bitset = transform_header.get_constant_tracks_bitset();
			context.constant_track_data = transform_header.get_constant_track_data();
			context.clip_range_data = transform_header.get_clip_range_data();

			for (uint32_t key_frame_index = 0; key_frame_index < 2; ++key_frame_index)
			{
				context.format_per_track_data[key_frame_index] = nullptr;
				context.segment_range_data[key_frame_index] = nullptr;
				context.animated_track_data[key_frame_index] = nullptr;
			}

			const bool has_scale = header.get_has_scale();
			const uint32_t num_tracks_per_bone = has_scale ? 3 : 2;
			context.bitset_desc = bitset_description::make_from_num_bits(header.num_tracks * num_tracks_per_bone);

			range_reduction_flags8 range_reduction = range_reduction_flags8::none;
			if (is_rotation_format_variable(rotation_format))
				range_reduction |= range_reduction_flags8::rotations;
			if (is_vector_format_variable(translation_format))
				range_reduction |= range_reduction_flags8::translations;
			if (is_vector_format_variable(scale_format))
				range_reduction |= range_reduction_flags8::scales;

			context.rotation_format = rotation_format;
			context.translation_format = translation_format;
			context.scale_format = scale_format;
			context.range_reduction = range_reduction;
			context.num_rotation_components = rotation_format == rotation_format8::quatf_full ? 4 : 3;
			context.has_segments = transform_header.num_segments > 1;

			return true;
		}

		inline bool is_dirty_v0(const persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks)
		{
			if (context.tracks != &tracks)
				return true;

			if (context.clip_hash != tracks.get_hash())
				return true;

			return false;
		}

		template<class decompression_settings_type>
		inline void seek_v0(persistent_transform_decompression_context_v0& context, float sample_time, sample_rounding_policy rounding_policy)
		{
			// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
			if (decompression_settings_type::clamp_sample_time())
				sample_time = rtm::scalar_clamp(sample_time, 0.0F, context.clip_duration);

			if (context.sample_time == sample_time)
				return;

			context.sample_time = sample_time;

			const tracks_header& header = get_tracks_header(*context.tracks);
			const transform_tracks_header& transform_header = get_transform_tracks_header(*context.tracks);

			uint32_t key_frame0;
			uint32_t key_frame1;
			find_linear_interpolation_samples_with_sample_rate(header.num_samples, header.sample_rate, sample_time, rounding_policy, key_frame0, key_frame1, context.interpolation_alpha);

			uint32_t segment_key_frame0;
			uint32_t segment_key_frame1;

			const segment_header* segment_header0;
			const segment_header* segment_header1;

			const segment_header* segment_headers = transform_header.get_segment_headers();
			const uint32_t num_segments = transform_header.num_segments;

			if (num_segments == 1)
			{
				// Key frame 0 and 1 are in the only segment present
				// This is a really common case and when it happens, we don't store the segment start index (zero)
				segment_header0 = segment_headers;
				segment_key_frame0 = key_frame0;

				segment_header1 = segment_headers;
				segment_key_frame1 = key_frame1;
			}
			else
			{
				const uint32_t* segment_start_indices = transform_header.get_segment_start_indices();

				// See segment_streams(..) for implementation details. This implementation is directly tied to it.
				const uint32_t approx_num_samples_per_segment = header.num_samples / num_segments;	// TODO: Store in header?
				const uint32_t approx_segment_index = key_frame0 / approx_num_samples_per_segment;

				uint32_t segment_index0 = 0;
				uint32_t segment_index1 = 0;

				// Our approximate segment guess is just that, a guess. The actual segments we need could be just before or after.
				// We start looking one segment earlier and up to 2 after. If we have too few segments after, we will hit the
				// sentinel value of 0xFFFFFFFF and exit the loop.
				// TODO: Can we do this with SIMD? Load all 4 values, set key_frame0, compare, move mask, count leading zeroes
				const uint32_t start_segment_index = approx_segment_index > 0 ? (approx_segment_index - 1) : 0;
				const uint32_t end_segment_index = start_segment_index + 4;

				for (uint32_t segment_index = start_segment_index; segment_index < end_segment_index; ++segment_index)
				{
					if (key_frame0 < segment_start_indices[segment_index])
					{
						// We went too far, use previous segment
						ACL_ASSERT(segment_index > 0, "Invalid segment index: %u", segment_index);
						segment_index0 = segment_index - 1;
						segment_index1 = key_frame1 < segment_start_indices[segment_index] ? segment_index0 : segment_index;
						break;
					}
				}

				segment_header0 = segment_headers + segment_index0;
				segment_header1 = segment_headers + segment_index1;

				segment_key_frame0 = key_frame0 - segment_start_indices[segment_index0];
				segment_key_frame1 = key_frame1 - segment_start_indices[segment_index1];
			}

			transform_header.get_segment_data(*segment_header0, context.format_per_track_data[0], context.segment_range_data[0], context.animated_track_data[0]);

			// More often than not the two segments are identical, when this is the case, just copy our pointers
			if (segment_header0 != segment_header1)
				transform_header.get_segment_data(*segment_header1, context.format_per_track_data[1], context.segment_range_data[1], context.animated_track_data[1]);
			else
			{
				context.format_per_track_data[1] = context.format_per_track_data[0];
				context.segment_range_data[1] = context.segment_range_data[0];
				context.animated_track_data[1] = context.animated_track_data[0];
			}

			context.key_frame_bit_offsets[0] = segment_key_frame0 * segment_header0->animated_pose_bit_size;
			context.key_frame_bit_offsets[1] = segment_key_frame1 * segment_header1->animated_pose_bit_size;
		}

		// TODO: Stage bitset decomp
		// TODO: Merge the per track format and segment range info into a single buffer? Less to prefetch and used together
		// TODO: How do we hide the cache miss after the seek to read the segment header? What work can we do while we prefetch?
		// TODO: Port vector3 decomp to use SOA
		// TODO: Unroll quat unpacking and convert to SOA
		// TODO: Use AVX where we can
		// TODO: Remove segment data alignment, no longer required?


		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_tracks_v0(const persistent_transform_decompression_context_v0& context, track_writer_type& writer)
		{
			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");
			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			const tracks_header& header = get_tracks_header(*context.tracks);

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const rtm::vector4f default_translation = rtm::vector_zero();
			const rtm::vector4f default_scale = rtm::vector_set(float(header.get_default_scale()));
			const bool has_scale = header.get_has_scale();
			const uint32_t num_tracks = header.num_tracks;

			constant_track_cache_v0 constant_track_cache;
			constant_track_cache.initialize<decompression_settings_type>(context);

			animated_track_cache_v0 animated_track_cache;
			animated_track_cache.initialize(context);

			uint32_t sub_track_index = 0;

			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				if ((track_index % 4) == 0)
				{
					// Unpack our next 4 tracks
					constant_track_cache.unpack_rotation_group<decompression_settings_type>(context);
					constant_track_cache.unpack_translation_group();

					animated_track_cache.unpack_rotation_group<decompression_settings_type>(context);
					animated_track_cache.unpack_translation_group<translation_adapter>(context);

					if (has_scale)
					{
						constant_track_cache.unpack_scale_group();
						animated_track_cache.unpack_scale_group<scale_adapter>(context);
					}
				}

				{
					const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
					const bool is_sample_default = bitset_test(context.default_tracks_bitset, track_index_bit_ref);
					rtm::quatf rotation;
					if (is_sample_default)
					{
						rotation = rtm::quat_identity();
					}
					else
					{
						const bool is_sample_constant = bitset_test(context.constant_tracks_bitset, track_index_bit_ref);
						if (is_sample_constant)
							rotation = constant_track_cache.consume_rotation();
						else
							rotation = animated_track_cache.consume_rotation();
					}

					ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

					if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index))
						writer.write_rotation(track_index, rotation);
					sub_track_index++;
				}

				{
					const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
					const bool is_sample_default = bitset_test(context.default_tracks_bitset, track_index_bit_ref);
					rtm::vector4f translation;
					if (is_sample_default)
					{
						translation = default_translation;
					}
					else
					{
						const bool is_sample_constant = bitset_test(context.constant_tracks_bitset, track_index_bit_ref);
						if (is_sample_constant)
							translation = constant_track_cache.consume_translation();
						else
							translation = animated_track_cache.consume_translation();
					}

					ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

					if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index))
						writer.write_translation(track_index, translation);
					sub_track_index++;
				}

				if (has_scale)
				{
					const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
					const bool is_sample_default = bitset_test(context.default_tracks_bitset, track_index_bit_ref);
					rtm::vector4f scale;
					if (is_sample_default)
					{
						scale = default_scale;
					}
					else
					{
						const bool is_sample_constant = bitset_test(context.constant_tracks_bitset, track_index_bit_ref);
						if (is_sample_constant)
							scale = constant_track_cache.consume_scale();
						else
							scale = animated_track_cache.consume_scale();
					}

					ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

					if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index))
						writer.write_scale(track_index, scale);
					sub_track_index++;
				}
				else if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index))
					writer.write_scale(track_index, default_scale);
			}

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_track_v0(const persistent_transform_decompression_context_v0& context, uint32_t track_index, track_writer_type& writer)
		{
			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");
			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			const tracks_header& tracks_header_ = get_tracks_header(*context.tracks);
			ACL_ASSERT(track_index < tracks_header_.num_tracks, "Invalid track index");

			if (track_index >= tracks_header_.num_tracks)
				return;	// Invalid track index

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const rtm::quatf default_rotation = rtm::quat_identity();
			const rtm::vector4f default_translation = rtm::vector_zero();
			const rtm::vector4f default_scale = rtm::vector_set(float(tracks_header_.get_default_scale()));
			const bool has_scale = tracks_header_.get_has_scale();

			// To decompress a single track, we need a few things:
			//    - if our rot/trans/scale is the default value, this is a trivial bitset lookup
			//    - constant and animated sub-tracks need to know which group they below to so it can be unpacked

			const uint32_t num_tracks_per_bone = has_scale ? 3 : 2;
			const uint32_t sub_track_index = track_index * num_tracks_per_bone;

			const bitset_index_ref rotation_sub_track_index_bit_ref(context.bitset_desc, sub_track_index + 0);
			const bitset_index_ref translation_sub_track_index_bit_ref(context.bitset_desc, sub_track_index + 1);
			const bitset_index_ref scale_sub_track_index_bit_ref(context.bitset_desc, sub_track_index + 2);

			const bool is_rotation_default = bitset_test(context.default_tracks_bitset, rotation_sub_track_index_bit_ref);
			const bool is_translation_default = bitset_test(context.default_tracks_bitset, translation_sub_track_index_bit_ref);
			const bool is_scale_default = has_scale ? bitset_test(context.default_tracks_bitset, scale_sub_track_index_bit_ref) : true;

			if (is_rotation_default && is_translation_default && is_scale_default)
			{
				// Everything is default
				writer.write_rotation(track_index, default_rotation);
				writer.write_translation(track_index, default_translation);
				writer.write_scale(track_index, default_scale);
				return;
			}

			const bool is_rotation_constant = !is_rotation_default && bitset_test(context.constant_tracks_bitset, rotation_sub_track_index_bit_ref);
			const bool is_translation_constant = !is_translation_default && bitset_test(context.constant_tracks_bitset, translation_sub_track_index_bit_ref);
			const bool is_scale_constant = !is_scale_default && has_scale ? bitset_test(context.constant_tracks_bitset, scale_sub_track_index_bit_ref) : false;

			const bool is_rotation_animated = !is_rotation_default && !is_rotation_constant;
			const bool is_translation_animated = !is_translation_default && !is_translation_constant;
			const bool is_scale_animated = !is_scale_default && !is_scale_constant;

			uint32_t num_default_rotations = 0;
			uint32_t num_default_translations = 0;
			uint32_t num_default_scales = 0;
			uint32_t num_constant_rotations = 0;
			uint32_t num_constant_translations = 0;
			uint32_t num_constant_scales = 0;

			if (has_scale)
			{
				uint32_t rotation_track_bit_mask = 0x92492492;		// b100100100..
				uint32_t translation_track_bit_mask = 0x49249249;	// b010010010..
				uint32_t scale_track_bit_mask = 0x24924924;			// b001001001..

				const uint32_t last_offset = sub_track_index / 32;
				uint32_t offset = 0;
				for (; offset < last_offset; ++offset)
				{
					const uint32_t default_value = context.default_tracks_bitset[offset];
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
					num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

					const uint32_t constant_value = context.constant_tracks_bitset[offset];
					num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
					num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
					num_constant_scales += count_set_bits(constant_value & scale_track_bit_mask);

					// Because the number of tracks in a 32 bit value isn't a multiple of the number of tracks we have (3),
					// we have to cycle the masks. There are 3 possible masks, just swap them.
					const uint32_t old_rotation_track_bit_mask = rotation_track_bit_mask;
					rotation_track_bit_mask = translation_track_bit_mask;
					translation_track_bit_mask = scale_track_bit_mask;
					scale_track_bit_mask = old_rotation_track_bit_mask;
				}

				const uint32_t remaining_tracks = sub_track_index % 32;
				if (remaining_tracks != 0)
				{
					const uint32_t not_up_to_track_mask = ((1 << (32 - remaining_tracks)) - 1);
					const uint32_t default_value = and_not(not_up_to_track_mask, context.default_tracks_bitset[offset]);
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
					num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

					const uint32_t constant_value = and_not(not_up_to_track_mask, context.constant_tracks_bitset[offset]);
					num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
					num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
					num_constant_scales += count_set_bits(constant_value & scale_track_bit_mask);
				}
			}
			else
			{
				const uint32_t rotation_track_bit_mask = 0xAAAAAAAA;		// b10101010..
				const uint32_t translation_track_bit_mask = 0x55555555;		// b01010101..

				const uint32_t last_offset = sub_track_index / 32;
				uint32_t offset = 0;
				for (; offset < last_offset; ++offset)
				{
					const uint32_t default_value = context.default_tracks_bitset[offset];
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

					const uint32_t constant_value = context.constant_tracks_bitset[offset];
					num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
					num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
				}

				const uint32_t remaining_tracks = sub_track_index % 32;
				if (remaining_tracks != 0)
				{
					const uint32_t not_up_to_track_mask = ((1 << (32 - remaining_tracks)) - 1);
					const uint32_t default_value = and_not(not_up_to_track_mask, context.default_tracks_bitset[offset]);
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

					const uint32_t constant_value = and_not(not_up_to_track_mask, context.constant_tracks_bitset[offset]);
					num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
					num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
				}
			}

			uint32_t rotation_group_index = 0;
			uint32_t translation_group_index = 0;
			uint32_t scale_group_index = 0;

			constant_track_cache_v0 constant_track_cache;

			// Skip the constant track data
			if (is_rotation_constant || is_translation_constant || is_scale_constant)
			{
				constant_track_cache.initialize<decompression_settings_type>(context);

				// Calculate how many constant groups of each sub-track type we need to skip
				// Constant groups are easy to skip since they are contiguous in memory, we can just skip N trivially
				// Tracks that are default are also constant

				// Unpack the groups we need and skip the tracks before us
				if (is_rotation_constant)
				{
					const uint32_t num_constant_rotations_packed = num_constant_rotations - num_default_rotations;
					const uint32_t num_rotation_constant_groups_to_skip = num_constant_rotations_packed / 4;
					if (num_rotation_constant_groups_to_skip != 0)
						constant_track_cache.skip_rotation_groups<decompression_settings_type>(context, num_rotation_constant_groups_to_skip);

					rotation_group_index = num_constant_rotations_packed % 4;
				}

				if (is_translation_constant)
				{
					const uint32_t num_constant_translations_packed = num_constant_translations - num_default_translations;
					const uint32_t num_translation_constant_groups_to_skip = num_constant_translations_packed / 4;
					if (num_translation_constant_groups_to_skip != 0)
						constant_track_cache.skip_translation_groups(num_translation_constant_groups_to_skip);

					translation_group_index = num_constant_translations_packed % 4;
				}

				if (is_scale_constant)
				{
					const uint32_t num_constant_scales_packed = num_constant_scales - num_default_scales;
					const uint32_t num_scale_constant_groups_to_skip = num_constant_scales_packed / 4;
					if (num_scale_constant_groups_to_skip != 0)
						constant_track_cache.skip_scale_groups(num_scale_constant_groups_to_skip);

					scale_group_index = num_constant_scales_packed % 4;
				}
			}
			else
			{
				// Fake init to avoid compiler warning...
				constant_track_cache.rotations.num_left_to_unpack = 0;
				constant_track_cache.constant_data_rotations = nullptr;
				constant_track_cache.constant_data_translations = nullptr;
				constant_track_cache.constant_data_scales = nullptr;
			}

			animated_track_cache_v0 animated_track_cache;
			animated_group_cursor_v0 rotation_group_cursor;
			animated_group_cursor_v0 translation_group_cursor;
			animated_group_cursor_v0 scale_group_cursor;

			// Skip the animated track data
			if (is_rotation_animated || is_translation_animated || is_scale_animated)
			{
				animated_track_cache.initialize(context);

				// Calculate how many animated groups of each sub-track type we need to skip
				// Skipping animated groups is a bit more complicated because they are interleaved in the order
				// they are needed

				// Tracks that are default are also constant
				const uint32_t num_animated_rotations = track_index - num_constant_rotations;

				if (is_rotation_animated)
					rotation_group_index = num_animated_rotations % 4;

				const uint32_t num_animated_translations = track_index - num_constant_translations;

				if (is_translation_animated)
					translation_group_index = num_animated_translations % 4;

				const uint32_t num_animated_scales = has_scale ? (track_index - num_constant_scales) : 0;

				if (is_scale_animated)
					scale_group_index = num_animated_scales % 4;

				uint32_t num_rotations_to_unpack = is_rotation_animated ? num_animated_rotations : ~0U;
				uint32_t num_translations_to_unpack = is_translation_animated ? num_animated_translations : ~0U;
				uint32_t num_scales_to_unpack = is_scale_animated ? num_animated_scales : ~0U;

				uint32_t num_animated_groups_to_unpack = is_rotation_animated + is_translation_animated + is_scale_animated;

				const transform_tracks_header& transform_header = get_transform_tracks_header(*context.tracks);
				const animation_track_type8* group_types = transform_header.get_animated_group_types();

				while (num_animated_groups_to_unpack != 0)
				{
					const animation_track_type8 group_type = *group_types;
					group_types++;
					ACL_ASSERT(group_type != static_cast<animation_track_type8>(0xFF), "Reached terminator");

					if (group_type == animation_track_type8::rotation)
					{
						if (num_rotations_to_unpack < 4)
						{
							// This is the group we need, cache our cursor
							animated_track_cache.get_rotation_cursor(rotation_group_cursor);
							num_animated_groups_to_unpack--;
						}

						animated_track_cache.skip_rotation_group<decompression_settings_type>(context);
						num_rotations_to_unpack -= 4;
					}
					else if (group_type == animation_track_type8::translation)
					{
						if (num_translations_to_unpack < 4)
						{
							// This is the group we need, cache our cursor
							animated_track_cache.get_translation_cursor(translation_group_cursor);
							num_animated_groups_to_unpack--;
						}

						animated_track_cache.skip_translation_group<translation_adapter>(context);
						num_translations_to_unpack -= 4;
					}
					else // scale
					{
						if (num_scales_to_unpack < 4)
						{
							// This is the group we need, cache our cursor
							animated_track_cache.get_scale_cursor(scale_group_cursor);
							num_animated_groups_to_unpack--;
						}

						animated_track_cache.skip_scale_group<scale_adapter>(context);
						num_scales_to_unpack -= 4;
					}
				}
			}

			// Finally reached our desired track, unpack it

			{
				rtm::quatf rotation;
				if (is_rotation_default)
					rotation = default_rotation;
				else if (is_rotation_constant)
					rotation = constant_track_cache.unpack_rotation_within_group<decompression_settings_type>(context, rotation_group_index);
				else
					rotation = animated_track_cache.unpack_rotation_within_group<decompression_settings_type>(context, rotation_group_cursor, rotation_group_index);

				writer.write_rotation(track_index, rotation);
			}

			{
				rtm::vector4f translation;
				if (is_translation_default)
					translation = default_translation;
				else if (is_translation_constant)
					translation = constant_track_cache.unpack_translation_within_group(translation_group_index);
				else
					translation = animated_track_cache.unpack_translation_within_group<translation_adapter>(context, translation_group_cursor, translation_group_index);

				writer.write_translation(track_index, translation);
			}

			{
				rtm::vector4f scale;
				if (is_scale_default)
					scale = default_scale;
				else if (is_scale_constant)
					scale = constant_track_cache.unpack_scale_within_group(scale_group_index);
				else
					scale = animated_track_cache.unpack_scale_within_group<scale_adapter>(context, scale_group_cursor, scale_group_index);

				writer.write_scale(track_index, scale);
			}

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
