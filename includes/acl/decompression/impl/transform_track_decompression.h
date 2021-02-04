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

#include "acl/core/bit_manip_utils.h"
#include "acl/core/bitset.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_formats.h"
#include "acl/core/track_writer.h"
#include "acl/core/variable_bit_rates.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/database/database.h"
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

#define ACL_IMPL_USE_SEEK_PREFETCH

ACL_IMPL_FILE_PRAGMA_PUSH

#if defined(ACL_COMPILER_MSVC)
	#pragma warning(push)
	// warning C4127: conditional expression is constant
	// This is fine, the optimizer will strip the code away when it can, but it isn't always constant in practice
	#pragma warning(disable : 4127)
#endif

namespace acl
{
	namespace acl_impl
	{
#if defined(ACL_IMPL_USE_SEEK_PREFETCH)
#define ACL_IMPL_SEEK_PREFETCH(ptr) memory_prefetch(ptr)
#else
#define ACL_IMPL_SEEK_PREFETCH(ptr) (void)(ptr)
#endif

		template<class decompression_settings_type>
		constexpr bool is_database_supported_impl()
		{
			return decompression_settings_type::database_settings_type::version_supported() != compressed_tracks_version16::none;
		}

		template<class decompression_settings_type, class database_settings_type>
		inline bool initialize_v0(persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks, const database_context<database_settings_type>* database)
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
			context.db = reinterpret_cast<const database_context_v0*>(database);	// Context is always the first member and versions should always match
			context.clip_hash = tracks.get_hash();
			context.clip_duration = calculate_duration(header.num_samples, header.sample_rate);
			context.sample_time = -1.0F;
			context.default_tracks_bitset = ptr_offset32<uint32_t>(&tracks, transform_header.get_default_tracks_bitset());
			context.constant_tracks_bitset = ptr_offset32<uint32_t>(&tracks, transform_header.get_constant_tracks_bitset());
			context.constant_track_data = ptr_offset32<uint8_t>(&tracks, transform_header.get_constant_track_data());
			context.clip_range_data = ptr_offset32<uint8_t>(&tracks, transform_header.get_clip_range_data());

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
			context.has_scale = has_scale;
			context.has_segments = transform_header.has_multiple_segments();
			context.num_sub_tracks_per_track = uint8_t(num_tracks_per_bone);

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

			// Prefetch our bitsets, we'll need them soon when we start decompressing
			ACL_IMPL_SEEK_PREFETCH(context.default_tracks_bitset.add_to(context.tracks));
			ACL_IMPL_SEEK_PREFETCH(context.constant_tracks_bitset.add_to(context.tracks));

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

			const uint8_t* db_animated_track_data0 = nullptr;
			const uint8_t* db_animated_track_data1 = nullptr;

			// These two pointers are the same, the compiler should optimize one out, only here for type safety later
			const segment_header* segment_headers = transform_header.get_segment_headers();
			const segment_tier0_header* segment_tier0_headers = transform_header.get_segment_tier0_headers();

			const uint32_t num_segments = transform_header.num_segments;

			const bool is_database_supported = is_database_supported_impl<decompression_settings_type>();
			ACL_ASSERT(is_database_supported || !context.tracks->has_database(), "Cannot have a database when it isn't supported");

			const bool has_database = is_database_supported && context.tracks->has_database();
			const database_context_v0* db = context.db;

			if (num_segments == 1)
			{
				// Key frame 0 and 1 are in the only segment present
				// This is a really common case and when it happens, we don't store the segment start index (zero)

				if (is_database_supported && has_database)
				{
					const segment_tier0_header* segment_tier0_header0 = segment_tier0_headers;

					// This will cache miss
					uint32_t sample_indices0 = segment_tier0_header0->sample_indices;

					// Calculate our clip relative sample index, we'll remap it later relative to the samples we'll use
					const float sample_index = context.interpolation_alpha + float(key_frame0);

					// When we load our sample indices and offsets from the database, there can be another thread writing
					// to those memory locations at the same time (e.g. streaming in/out).
					// To ensure thread safety, we atomically load the offset and sample indices.
					uint64_t medium_importance_tier_metadata0 = 0;
					uint64_t low_importance_tier_metadata0 = 0;

					// Combine all our loaded samples into a single bit set to find which samples we need to interpolate
					if (db != nullptr)
					{
						// Possible cache miss for the clip header offset
						// Cache miss for the db clip segment headers pointer
						const tracks_database_header* tracks_db_header = transform_header.get_database_header();
						const database_runtime_clip_header* db_clip_header = tracks_db_header->get_clip_header(db->clip_segment_headers);
						const database_runtime_segment_header* db_segment_headers = db_clip_header->get_segment_headers();

						// Cache miss for the db segment headers
						const database_runtime_segment_header* db_segment_header0 = db_segment_headers;
						medium_importance_tier_metadata0 = db_segment_header0->tier_metadata[0].load(std::memory_order::memory_order_relaxed);
						low_importance_tier_metadata0 = db_segment_header0->tier_metadata[1].load(std::memory_order::memory_order_relaxed);

						sample_indices0 |= uint32_t(medium_importance_tier_metadata0);
						sample_indices0 |= uint32_t(low_importance_tier_metadata0);
					}

					// Find the closest loaded samples
					// Mask all trailing samples to find the first sample by counting trailing zeros
					const uint32_t candidate_indices0 = sample_indices0 & (0xFFFFFFFFU << (31 - key_frame0));
					key_frame0 = 31 - count_trailing_zeros(candidate_indices0);

					// Mask all leading samples to find the second sample by counting leading zeros
					const uint32_t candidate_indices1 = sample_indices0 & (0xFFFFFFFFU >> key_frame1);
					key_frame1 = count_leading_zeros(candidate_indices1);

					// Calculate our new interpolation alpha
					context.interpolation_alpha = find_linear_interpolation_alpha(sample_index, key_frame0, key_frame1, rounding_policy);

					// Find where our data lives (clip or database tier X)
					sample_indices0 = segment_tier0_header0->sample_indices;
					uint32_t sample_indices1 = sample_indices0;	// Identical

					if (db != nullptr)
					{
						const uint64_t sample_index0 = uint64_t(1) << (31 - key_frame0);
						const uint64_t sample_index1 = uint64_t(1) << (31 - key_frame1);

						const uint8_t* bulk_data_medium = db->bulk_data[0];		// Might be nullptr if we haven't streamed in yet
						const uint8_t* bulk_data_low = db->bulk_data[1];		// Might be nullptr if we haven't streamed in yet
						if ((medium_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(medium_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_medium + uint32_t(medium_importance_tier_metadata0 >> 32);
						}
						else if ((low_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(low_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_low + uint32_t(low_importance_tier_metadata0 >> 32);
						}

						// Only one segment, our metadata is the same for our second key frame
						if ((medium_importance_tier_metadata0 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(medium_importance_tier_metadata0);
							db_animated_track_data1 = bulk_data_medium + uint32_t(medium_importance_tier_metadata0 >> 32);
						}
						else if ((low_importance_tier_metadata0 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(low_importance_tier_metadata0);
							db_animated_track_data1 = bulk_data_low + uint32_t(low_importance_tier_metadata0 >> 32);
						}
					}

					// Remap our sample indices within the ones actually stored (e.g. index 3 might be the second frame stored)
					segment_key_frame0 = count_set_bits(and_not(0xFFFFFFFFU >> key_frame0, sample_indices0));
					segment_key_frame1 = count_set_bits(and_not(0xFFFFFFFFU >> key_frame1, sample_indices1));

					// Nasty but safe since they have the same layout
					segment_header0 = reinterpret_cast<const segment_header*>(segment_tier0_header0);
					segment_header1 = reinterpret_cast<const segment_header*>(segment_tier0_header0);
				}
				else
				{
					segment_header0 = segment_headers;
					segment_header1 = segment_headers;

					segment_key_frame0 = key_frame0;
					segment_key_frame1 = key_frame1;
				}
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

				segment_key_frame0 = key_frame0 - segment_start_indices[segment_index0];
				segment_key_frame1 = key_frame1 - segment_start_indices[segment_index1];

				if (is_database_supported && has_database)
				{
					const segment_tier0_header* segment_tier0_header0 = segment_tier0_headers + segment_index0;
					const segment_tier0_header* segment_tier0_header1 = segment_tier0_headers + segment_index1;

					// This will cache miss
					uint32_t sample_indices0 = segment_tier0_header0->sample_indices;
					uint32_t sample_indices1 = segment_tier0_header1->sample_indices;

					// Calculate our clip relative sample index, we'll remap it later relative to the samples we'll use
					const float sample_index = context.interpolation_alpha + float(key_frame0);

					// When we load our sample indices and offsets from the database, there can be another thread writing
					// to those memory locations at the same time (e.g. streaming in/out).
					// To ensure thread safety, we atomically load the offset and sample indices.
					uint64_t medium_importance_tier_metadata0 = 0;
					uint64_t medium_importance_tier_metadata1 = 0;
					uint64_t low_importance_tier_metadata0 = 0;
					uint64_t low_importance_tier_metadata1 = 0;

					// Combine all our loaded samples into a single bit set to find which samples we need to interpolate
					if (db != nullptr)
					{
						// Possible cache miss for the clip header offset
						// Cache miss for the db clip segment headers pointer
						const tracks_database_header* tracks_db_header = transform_header.get_database_header();
						const database_runtime_clip_header* db_clip_header = tracks_db_header->get_clip_header(db->clip_segment_headers);
						const database_runtime_segment_header* db_segment_headers = db_clip_header->get_segment_headers();

						// Cache miss for the db segment headers
						const database_runtime_segment_header* db_segment_header0 = db_segment_headers + segment_index0;
						medium_importance_tier_metadata0 = db_segment_header0->tier_metadata[0].load(std::memory_order::memory_order_relaxed);
						low_importance_tier_metadata0 = db_segment_header0->tier_metadata[1].load(std::memory_order::memory_order_relaxed);

						sample_indices0 |= uint32_t(medium_importance_tier_metadata0);
						sample_indices0 |= uint32_t(low_importance_tier_metadata0);

						const database_runtime_segment_header* db_segment_header1 = db_segment_headers + segment_index1;
						medium_importance_tier_metadata1 = db_segment_header1->tier_metadata[0].load(std::memory_order::memory_order_relaxed);
						low_importance_tier_metadata1 = db_segment_header1->tier_metadata[1].load(std::memory_order::memory_order_relaxed);

						sample_indices1 |= uint32_t(medium_importance_tier_metadata1);
						sample_indices1 |= uint32_t(low_importance_tier_metadata1);
					}

					// Find the closest loaded samples
					// Mask all trailing samples to find the first sample by counting trailing zeros
					const uint32_t candidate_indices0 = sample_indices0 & (0xFFFFFFFFU << (31 - segment_key_frame0));
					segment_key_frame0 = 31 - count_trailing_zeros(candidate_indices0);

					// Mask all leading samples to find the second sample by counting leading zeros
					const uint32_t candidate_indices1 = sample_indices1 & (0xFFFFFFFFU >> segment_key_frame1);
					segment_key_frame1 = count_leading_zeros(candidate_indices1);

					// Calculate our clip relative sample indices
					const uint32_t clip_key_frame0 = segment_start_indices[segment_index0] + segment_key_frame0;
					const uint32_t clip_key_frame1 = segment_start_indices[segment_index1] + segment_key_frame1;

					// Calculate our new interpolation alpha
					context.interpolation_alpha = find_linear_interpolation_alpha(sample_index, clip_key_frame0, clip_key_frame1, rounding_policy);

					// Find where our data lives (clip or database tier X)
					sample_indices0 = segment_tier0_header0->sample_indices;
					sample_indices1 = segment_tier0_header1->sample_indices;

					if (db != nullptr)
					{
						const uint64_t sample_index0 = uint64_t(1) << (31 - segment_key_frame0);
						const uint64_t sample_index1 = uint64_t(1) << (31 - segment_key_frame1);

						const uint8_t* bulk_data_medium = db->bulk_data[0];		// Might be nullptr if we haven't streamed in yet
						const uint8_t* bulk_data_low = db->bulk_data[1];		// Might be nullptr if we haven't streamed in yet
						if ((medium_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(medium_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_medium + uint32_t(medium_importance_tier_metadata0 >> 32);
						}
						else if ((low_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(low_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_low + uint32_t(low_importance_tier_metadata0 >> 32);
						}

						if ((medium_importance_tier_metadata1 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(medium_importance_tier_metadata1);
							db_animated_track_data1 = bulk_data_medium + uint32_t(medium_importance_tier_metadata1 >> 32);
						}
						else if ((low_importance_tier_metadata1 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(low_importance_tier_metadata1);
							db_animated_track_data1 = bulk_data_low + uint32_t(low_importance_tier_metadata1 >> 32);
						}
					}

					// Remap our sample indices within the ones actually stored (e.g. index 3 might be the second frame stored)
					segment_key_frame0 = count_set_bits(and_not(0xFFFFFFFFU >> segment_key_frame0, sample_indices0));
					segment_key_frame1 = count_set_bits(and_not(0xFFFFFFFFU >> segment_key_frame1, sample_indices1));

					// Nasty but safe since they have the same layout
					segment_header0 = reinterpret_cast<const segment_header*>(segment_tier0_header0);
					segment_header1 = reinterpret_cast<const segment_header*>(segment_tier0_header1);
				}
				else
				{
					segment_header0 = segment_headers + segment_index0;
					segment_header1 = segment_headers + segment_index1;
				}
			}

			{
				// Prefetch our constant rotation data, we'll need it soon when we start decompressing and we are about to cache miss on the segment headers
				const uint8_t* constant_data_rotations = context.constant_track_data.add_to(context.tracks);
				ACL_IMPL_SEEK_PREFETCH(constant_data_rotations);
				ACL_IMPL_SEEK_PREFETCH(constant_data_rotations + 64);
			}

			const bool uses_single_segment = segment_header0 == segment_header1;
			context.uses_single_segment = uses_single_segment;

			// Cache miss if we don't access the db data
			transform_header.get_segment_data(*segment_header0, context.format_per_track_data[0], context.segment_range_data[0], context.animated_track_data[0]);

			// More often than not the two segments are identical, when this is the case, just copy our pointers
			if (!uses_single_segment)
			{
				transform_header.get_segment_data(*segment_header1, context.format_per_track_data[1], context.segment_range_data[1], context.animated_track_data[1]);
			}
			else
			{
				context.format_per_track_data[1] = context.format_per_track_data[0];
				context.segment_range_data[1] = context.segment_range_data[0];
				context.animated_track_data[1] = context.animated_track_data[0];
			}

			if (is_database_supported && has_database)
			{
				// Update our pointers if the data lives within the database
				if (db_animated_track_data0 != nullptr)
					context.animated_track_data[0] = db_animated_track_data0;

				if (db_animated_track_data1 != nullptr)
					context.animated_track_data[1] = db_animated_track_data1;
			}

			context.key_frame_bit_offsets[0] = segment_key_frame0 * segment_header0->animated_pose_bit_size;
			context.key_frame_bit_offsets[1] = segment_key_frame1 * segment_header1->animated_pose_bit_size;

			context.segment_offsets[0] = ptr_offset32<segment_header>(context.tracks, segment_header0);
			context.segment_offsets[1] = ptr_offset32<segment_header>(context.tracks, segment_header1);
		}


		// TODO: Merge the per track format and segment range info into a single buffer? Less to prefetch and used together
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

			const rtm::quatf default_rotation = rtm::quat_identity();
			const rtm::vector4f default_translation = rtm::vector_zero();
			const rtm::vector4f default_scale = rtm::vector_set(float(header.get_default_scale()));
			const bool has_scale = context.has_scale;
			const uint32_t num_tracks = header.num_tracks;
			const uint32_t num_sub_tracks_per_track = context.num_sub_tracks_per_track;

			const uint32_t* default_tracks_bitset = context.default_tracks_bitset.add_to(context.tracks);
			const uint32_t* constant_tracks_bitset = context.constant_tracks_bitset.add_to(context.tracks);

			constant_track_cache_v0 constant_track_cache;
			constant_track_cache.initialize<decompression_settings_type>(context);

			{
				// By now, our bit sets (1-2 cache lines) constant rotations (2 cache lines) have landed in the L2
				// We prefetched them ahead in the seek(..) function call and due to cache misses when seeking,
				// their latency should be fully hidden.
				// Prefetch our 3rd constant rotation cache line to prime the hardware prefetcher and do the same for constant translations

				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_rotations + 128);
				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_translations);
				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_translations + 64);
				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_translations + 128);
			}

			animated_track_cache_v0 animated_track_cache;
			animated_track_cache.initialize<decompression_settings_type, translation_adapter>(context);

			{
				// Start prefetching the per track metadata of both segments
				// They might live in a different memory page than the clip's header and constant data
				// and we need to prime VMEM translation and the TLB

				const uint8_t* per_track_metadata0 = animated_track_cache.segment_sampling_context_rotations[0].format_per_track_data;
				const uint8_t* per_track_metadata1 = animated_track_cache.segment_sampling_context_rotations[1].format_per_track_data;
				ACL_IMPL_SEEK_PREFETCH(per_track_metadata0);
				ACL_IMPL_SEEK_PREFETCH(per_track_metadata1);
			}

			// I tried using branchless selection for the default/constant track by building a mask to select
			// the pointer and branching was often still faster due to branch prediction. It was sometimes slower
			// but an executive decision has been made to keep the branches until we can show a clear consistent win.

			// I tried caching the bitset in a 64 bit register and simply shifting the bits in but it ended up being slightly
			// slower more often than not. It seems to impact dependency chains somewhat. Maybe revisit this at some point.

			// TODO: The first time we iterate over the bit set, unpack it into our output pose as a temporary buffer
			// We can build a linked list
			// Store on the stack the first animated rot/trans/scale
			// For its rot/trans/scale, write instead the index of the next animated rot/trans/scale
			// We can even unpack it first on its own
			// Writer can expose this with something like write_rotation_index/read_rotation_index
			// The writer can then allocate a separate buffer for this or re-use the pose buffer
			// When the time comes to write our animated samples, we can unpack 4, grab the next 4 entries from the linked
			// list and write our samples. We can do this until all samples are written which should be faster than iterating a bit set
			// since it'll allow us to quickly skip entries we don't care about. The same scheme can be used for constant/default tracks.
			// When we unpack our bitset, we can also count the number of entries for each type to help iterate

			// Unpack our constant rotation sub-tracks
			for (uint32_t track_index = 0, sub_track_index = 0; track_index < num_tracks; ++track_index)
			{
				if ((track_index % 4) == 0)
				{
					// Unpack our next 4 tracks
					constant_track_cache.unpack_rotation_group<decompression_settings_type>(context);
				}

				{
					const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
					const bool is_sample_default = bitset_test(default_tracks_bitset, track_index_bit_ref);
					const bool is_sample_constant = bitset_test(constant_tracks_bitset, track_index_bit_ref);
					const bool is_sample_non_default_constant = !is_sample_default & is_sample_constant;

					rtm::quatf rotation = default_rotation;
					if (is_sample_non_default_constant)
						rotation = constant_track_cache.consume_rotation();

					// TODO: Revisit how we do the track skipping, we could skip the whole loop or the bitset stuff

					if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index))
						writer.write_rotation(track_index, rotation);
				}

				// Skip our rotation, translation, and scale sub-tracks
				sub_track_index += num_sub_tracks_per_track;
			}

			// By now, our constant translations (3 cache lines) have landed in L2 after our prefetching has completed
			// We typically will do enough work above to hide the latency
			// We do not prefetch our constant scales because scale is fairly rare
			// Instead, we prefetch our segment range and animated data
			// The second key frame of animated data might not live in the same memory page even if we use a single segment
			// so this allows us to prime the TLB as well
			{
				const uint8_t* segment_range_data0 = animated_track_cache.segment_sampling_context_rotations[0].segment_range_data;
				const uint8_t* segment_range_data1 = animated_track_cache.segment_sampling_context_rotations[1].segment_range_data;
				const uint8_t* animated_data0 = animated_track_cache.segment_sampling_context_rotations[0].animated_track_data;
				const uint8_t* animated_data1 = animated_track_cache.segment_sampling_context_rotations[1].animated_track_data;
				const uint8_t* frame_animated_data0 = animated_data0 + (animated_track_cache.segment_sampling_context_rotations[0].animated_track_data_bit_offset / 8);
				const uint8_t* frame_animated_data1 = animated_data1 + (animated_track_cache.segment_sampling_context_rotations[1].animated_track_data_bit_offset / 8);

				ACL_IMPL_SEEK_PREFETCH(segment_range_data0);
				ACL_IMPL_SEEK_PREFETCH(segment_range_data0 + 64);
				ACL_IMPL_SEEK_PREFETCH(segment_range_data1);
				ACL_IMPL_SEEK_PREFETCH(segment_range_data1 + 64);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data0);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data1);
			}

			// Unpack our constant translation/scale sub-tracks
			for (uint32_t track_index = 0, sub_track_index = 1; track_index < num_tracks; ++track_index)
			{
				{
					const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
					const bool is_sample_default = bitset_test(default_tracks_bitset, track_index_bit_ref);
					const bool is_sample_constant = bitset_test(constant_tracks_bitset, track_index_bit_ref);
					const bool is_sample_non_default_constant = !is_sample_default & is_sample_constant;

					rtm::vector4f translation = default_translation;

					if (is_sample_non_default_constant)
						translation = constant_track_cache.consume_translation();

					ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

					if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index))
						writer.write_translation(track_index, translation);
				}

				if (has_scale)
				{
					const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index + 1);
					const bool is_sample_default = bitset_test(default_tracks_bitset, track_index_bit_ref);
					const bool is_sample_constant = bitset_test(constant_tracks_bitset, track_index_bit_ref);
					const bool is_sample_non_default_constant = !is_sample_default & is_sample_constant;

					rtm::vector4f scale = default_scale;

					if (is_sample_non_default_constant)
						scale = constant_track_cache.consume_scale();

					ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

					if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index))
						writer.write_scale(track_index, scale);
				}
				else if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index))
					writer.write_scale(track_index, default_scale);

				sub_track_index += num_sub_tracks_per_track;
			}

			{
				// By now the first few cache lines of our segment data has landed in the L2
				// Prefetch ahead some more to prime the hardware prefetcher
				// We also start prefetching the clip range data since we'll need it soon and we need to prime the TLB
				// and the hardware prefetcher

				const uint8_t* per_track_metadata0 = animated_track_cache.segment_sampling_context_rotations[0].format_per_track_data;
				const uint8_t* per_track_metadata1 = animated_track_cache.segment_sampling_context_rotations[1].format_per_track_data;
				const uint8_t* animated_data0 = animated_track_cache.segment_sampling_context_rotations[0].animated_track_data;
				const uint8_t* animated_data1 = animated_track_cache.segment_sampling_context_rotations[1].animated_track_data;
				const uint8_t* frame_animated_data0 = animated_data0 + (animated_track_cache.segment_sampling_context_rotations[0].animated_track_data_bit_offset / 8);
				const uint8_t* frame_animated_data1 = animated_data1 + (animated_track_cache.segment_sampling_context_rotations[1].animated_track_data_bit_offset / 8);

				ACL_IMPL_SEEK_PREFETCH(per_track_metadata0 + 64);
				ACL_IMPL_SEEK_PREFETCH(per_track_metadata1 + 64);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data0 + 64);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data1 + 64);
				ACL_IMPL_SEEK_PREFETCH(animated_track_cache.clip_sampling_context_rotations.clip_range_data);
				ACL_IMPL_SEEK_PREFETCH(animated_track_cache.clip_sampling_context_rotations.clip_range_data + 64);

				// TODO: Can we prefetch the translation data ahead instead to prime the TLB?
			}

			// Unpack our variable sub-tracks
			// TODO: Unpack 4, then iterate over tracks to write?
			// Can we keep the rotations in registers? Does it matter?

			// Unpack rotations first
			for (uint32_t track_index = 0, sub_track_index = 0; track_index < num_tracks; ++track_index)
			{
				// Unpack our next 4 tracks
				if ((track_index % 4) == 0)
					animated_track_cache.unpack_rotation_group<decompression_settings_type>(context);

				const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
				const bool is_sample_constant = bitset_test(constant_tracks_bitset, track_index_bit_ref);

				if (!is_sample_constant)
				{
					const rtm::quatf rotation = animated_track_cache.consume_rotation();

					ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

					if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index))
						writer.write_rotation(track_index, rotation);
				}

				sub_track_index += num_sub_tracks_per_track;
			}

			// Unpack translations second
			for (uint32_t track_index = 0, sub_track_index = 1; track_index < num_tracks; ++track_index)
			{
				// Unpack our next 4 tracks
				if ((track_index % 4) == 0)
					animated_track_cache.unpack_translation_group<translation_adapter>(context);

				const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
				const bool is_sample_constant = bitset_test(constant_tracks_bitset, track_index_bit_ref);

				if (!is_sample_constant)
				{
					const rtm::vector4f translation = animated_track_cache.consume_translation();

					ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

					if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index))
						writer.write_translation(track_index, translation);
				}

				sub_track_index += num_sub_tracks_per_track;
			}

			// Unpack scales last
			if (has_scale)
			{
				for (uint32_t track_index = 0, sub_track_index = 2; track_index < num_tracks; ++track_index)
				{
					// Unpack our next 4 tracks
					if ((track_index % 4) == 0)
						animated_track_cache.unpack_scale_group<scale_adapter>(context);

					const bitset_index_ref track_index_bit_ref(context.bitset_desc, sub_track_index);
					const bool is_sample_constant = bitset_test(constant_tracks_bitset, track_index_bit_ref);

					if (!is_sample_constant)
					{
						const rtm::vector4f scale = animated_track_cache.consume_scale();

						ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index))
							writer.write_scale(track_index, scale);
					}

					sub_track_index += num_sub_tracks_per_track;
				}
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
			const bool has_scale = context.has_scale;

			const uint32_t* default_tracks_bitset = context.default_tracks_bitset.add_to(context.tracks);
			const uint32_t* constant_tracks_bitset = context.constant_tracks_bitset.add_to(context.tracks);

			// To decompress a single track, we need a few things:
			//    - if our rot/trans/scale is the default value, this is a trivial bitset lookup
			//    - constant and animated sub-tracks need to know which group they belong to so it can be unpacked

			const uint32_t num_sub_tracks_per_track = context.num_sub_tracks_per_track;
			const uint32_t sub_track_index = track_index * num_sub_tracks_per_track;

			const bitset_index_ref rotation_sub_track_index_bit_ref(context.bitset_desc, sub_track_index + 0);
			const bitset_index_ref translation_sub_track_index_bit_ref(context.bitset_desc, sub_track_index + 1);
			const bitset_index_ref scale_sub_track_index_bit_ref(context.bitset_desc, sub_track_index + 2);

			const bool is_rotation_default = bitset_test(default_tracks_bitset, rotation_sub_track_index_bit_ref);
			const bool is_translation_default = bitset_test(default_tracks_bitset, translation_sub_track_index_bit_ref);
			const bool is_scale_default = has_scale ? bitset_test(default_tracks_bitset, scale_sub_track_index_bit_ref) : true;

			if (is_rotation_default && is_translation_default && is_scale_default)
			{
				// Everything is default
				writer.write_rotation(track_index, default_rotation);
				writer.write_translation(track_index, default_translation);
				writer.write_scale(track_index, default_scale);
				return;
			}

			const bool is_rotation_constant = !is_rotation_default && bitset_test(constant_tracks_bitset, rotation_sub_track_index_bit_ref);
			const bool is_translation_constant = !is_translation_default && bitset_test(constant_tracks_bitset, translation_sub_track_index_bit_ref);
			const bool is_scale_constant = !is_scale_default && has_scale ? bitset_test(constant_tracks_bitset, scale_sub_track_index_bit_ref) : false;

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
					const uint32_t default_value = default_tracks_bitset[offset];
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
					num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

					const uint32_t constant_value = constant_tracks_bitset[offset];
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
					const uint32_t default_value = and_not(not_up_to_track_mask, default_tracks_bitset[offset]);
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
					num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

					const uint32_t constant_value = and_not(not_up_to_track_mask, constant_tracks_bitset[offset]);
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
					const uint32_t default_value = default_tracks_bitset[offset];
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

					const uint32_t constant_value = constant_tracks_bitset[offset];
					num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
					num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
				}

				const uint32_t remaining_tracks = sub_track_index % 32;
				if (remaining_tracks != 0)
				{
					const uint32_t not_up_to_track_mask = ((1 << (32 - remaining_tracks)) - 1);
					const uint32_t default_value = and_not(not_up_to_track_mask, default_tracks_bitset[offset]);
					num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
					num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

					const uint32_t constant_value = and_not(not_up_to_track_mask, constant_tracks_bitset[offset]);
					num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
					num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
				}
			}

			uint32_t rotation_group_sample_index = 0;
			uint32_t translation_group_sample_index = 0;
			uint32_t scale_group_sample_index = 0;

			constant_track_cache_v0 constant_track_cache;

			// Skip the constant track data
			if (is_rotation_constant || is_translation_constant || is_scale_constant)
			{
				// TODO: Can we init just what we need?
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

					rotation_group_sample_index = num_constant_rotations_packed % 4;
				}

				if (is_translation_constant)
				{
					const uint32_t num_constant_translations_packed = num_constant_translations - num_default_translations;
					const uint32_t num_translation_constant_groups_to_skip = num_constant_translations_packed / 4;
					if (num_translation_constant_groups_to_skip != 0)
						constant_track_cache.skip_translation_groups(num_translation_constant_groups_to_skip);

					translation_group_sample_index = num_constant_translations_packed % 4;
				}

				if (is_scale_constant)
				{
					const uint32_t num_constant_scales_packed = num_constant_scales - num_default_scales;
					const uint32_t num_scale_constant_groups_to_skip = num_constant_scales_packed / 4;
					if (num_scale_constant_groups_to_skip != 0)
						constant_track_cache.skip_scale_groups(num_scale_constant_groups_to_skip);

					scale_group_sample_index = num_constant_scales_packed % 4;
				}
			}

			animated_track_cache_v0 animated_track_cache;

			// Skip the animated track data
			if (is_rotation_animated || is_translation_animated || is_scale_animated)
			{
				// TODO: Can we init just what we need?
				animated_track_cache.initialize<decompression_settings_type, translation_adapter>(context);

				if (is_rotation_animated)
				{
					const uint32_t num_animated_rotations = track_index - num_constant_rotations;
					rotation_group_sample_index = num_animated_rotations % 4;
					const uint32_t num_groups_to_skip = num_animated_rotations / 4;
					if (num_groups_to_skip != 0)
						animated_track_cache.skip_rotation_groups<decompression_settings_type>(context, num_groups_to_skip);
				}

				if (is_translation_animated)
				{
					const uint32_t num_animated_translations = track_index - num_constant_translations;
					translation_group_sample_index = num_animated_translations % 4;
					const uint32_t num_groups_to_skip = num_animated_translations / 4;
					if (num_groups_to_skip != 0)
						animated_track_cache.skip_translation_groups<translation_adapter>(context, num_groups_to_skip);
				}

				if (is_scale_animated)
				{
					const uint32_t num_animated_scales = track_index - num_constant_scales;
					scale_group_sample_index = num_animated_scales % 4;
					const uint32_t num_groups_to_skip = num_animated_scales / 4;
					if (num_groups_to_skip != 0)
						animated_track_cache.skip_scale_groups<scale_adapter>(context, num_groups_to_skip);
				}
			}

			// Finally reached our desired track, unpack it

			{
				rtm::quatf rotation;
				if (is_rotation_default)
					rotation = default_rotation;
				else if (is_rotation_constant)
					rotation = constant_track_cache.unpack_rotation_within_group<decompression_settings_type>(context, rotation_group_sample_index);
				else
					rotation = animated_track_cache.unpack_rotation_within_group<decompression_settings_type>(context, rotation_group_sample_index);

				writer.write_rotation(track_index, rotation);
			}

			{
				rtm::vector4f translation;
				if (is_translation_default)
					translation = default_translation;
				else if (is_translation_constant)
					translation = constant_track_cache.unpack_translation_within_group(translation_group_sample_index);
				else
					translation = animated_track_cache.unpack_translation_within_group<translation_adapter>(context, translation_group_sample_index);

				writer.write_translation(track_index, translation);
			}

			{
				rtm::vector4f scale;
				if (is_scale_default)
					scale = default_scale;
				else if (is_scale_constant)
					scale = constant_track_cache.unpack_scale_within_group(scale_group_sample_index);
				else
					scale = animated_track_cache.unpack_scale_within_group<scale_adapter>(context, scale_group_sample_index);

				writer.write_scale(track_index, scale);
			}

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}
	}
}

#if defined(ACL_COMPILER_MSVC)
	#pragma warning(pop)
#endif

ACL_IMPL_FILE_PRAGMA_POP
