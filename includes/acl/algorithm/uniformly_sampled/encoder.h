#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/algorithm_types.h"
#include "acl/core/bitset.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/compressed_clip.h"
#include "acl/core/error.h"
#include "acl/core/error_result.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/scope_profiler.h"
#include "acl/core/track_types.h"
#include "acl/compression/compressed_clip_impl.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/output_stats.h"
#include "acl/compression/impl/track_database.h"
#include "acl/compression/stream/clip_context.h"
#include "acl/compression/stream/track_stream.h"
#include "acl/compression/stream/convert_rotation_streams.h"
#include "acl/compression/stream/compact_constant_streams.h"
#include "acl/compression/stream/normalize_streams.h"
#include "acl/compression/stream/quantize_streams.h"
#include "acl/compression/stream/segment_streams.h"
#include "acl/compression/stream/write_segment_data.h"
#include "acl/compression/stream/write_stats.h"
#include "acl/compression/stream/write_stream_bitsets.h"
#include "acl/compression/stream/write_stream_data.h"
#include "acl/decompression/default_output_writer.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace uniformly_sampled
	{
		//////////////////////////////////////////////////////////////////////////
		// Compresses a raw clip with uniform sampling
		//
		// This compression algorithm is the simplest by far and as such it offers
		// the fastest compression and decompression. Every sample is retained and
		// every track has the same number of samples playing back at the same
		// sample rate. This means that when we sample at a particular time within
		// the clip, we can trivially calculate the offsets required to read the
		// desired data. All the data is sorted in order to ensure all reads are
		// as contiguous as possible for optimal cache locality during decompression.
		//
		//    allocator: The allocator instance to use to allocate and free memory
		//    clip: The raw clip to compress
		//    settings: The compression settings to use
		//    out_compressed_clip: The resulting compressed clip. The caller owns the returned memory and must free it
		//    out_stats: Stat output structure
		//////////////////////////////////////////////////////////////////////////
		inline ErrorResult compress_clip(IAllocator& allocator, const AnimationClip& clip, CompressionSettings settings, CompressedClip*& out_compressed_clip, OutputStats& out_stats)
		{
			using namespace impl;
			using namespace acl_impl;
			(void)out_stats;

			ErrorResult error_result = clip.is_valid();
			if (error_result.any())
				return error_result;

			error_result = settings.is_valid();
			if (error_result.any())
				return error_result;

			// Disable floating point exceptions during compression because we leverage all SIMD lanes
			// and we might intentionally divide by zero, etc.
			scope_disable_fp_exceptions fp_off;

			ScopeProfiler compression_time;

			const uint32_t num_samples = clip.get_num_samples();
			const uint32_t num_transforms = clip.get_num_bones();
			const RigidSkeleton& skeleton = clip.get_skeleton();
			const AnimationClip* additive_base_clip = clip.get_additive_base();

			const bool has_scale = clip.has_scale(settings.constant_scale_threshold);

			uint32_t num_segments;
			segment_context* segments = partition_into_segments(allocator, num_samples, num_transforms, has_scale, settings.segmenting, num_segments);

			// If we have a single segment or segmenting is disabled, disable range reduction since it won't help
			if (!settings.segmenting.enabled || num_segments == 1)
				settings.segmenting.range_reduction = RangeReductionFlags8::None;

			track_database raw_track_database(allocator, clip, skeleton, settings, segments, num_segments);
			track_database mutable_track_database(allocator, clip, skeleton, settings, segments, num_segments);

			track_database* additive_base_track_database = nullptr;
			if (additive_base_clip != nullptr)
				additive_base_track_database = allocate_type<track_database>(allocator, allocator, *additive_base_clip, skeleton, settings, segments, num_segments);

			// TODO: If our segment size is too large and doesn't fit in the L1 or L2 too comfortably, iterating per pass instead
			// of per segment might be faster since that way the code can at least remain in the L1. Doesn't matter as much if
			// we process segments in parallel but it still might if each thread processes more than 1 segment.
			// CPU decoding is often optimized for short loops.

			// Process every segment, this could be done in parallel
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				segment_context& segment = segments[segment_index];

				// Populate the database from our raw clip
				raw_track_database.populate_data(segment, clip);

				// Just copy the data from the raw database since it's now nicely formatted for us
				mutable_track_database.copy_data(segment, raw_track_database);
				if (additive_base_track_database != nullptr)
					additive_base_track_database->copy_data(segment, raw_track_database);	// TODO: Copy with streaming writes to bypass the CPU cache, won't be needed again soon

				// TODO: Should we also convert the raw databases? It seems to make sense because we always convert it when quantizing at the end anyway, does it matter?
				convert_rotations(mutable_track_database, segment, settings.rotation_format);

				// Extract segment ranges, we'll merge them after the loop
				extract_database_transform_ranges_per_segment(mutable_track_database, segment);
			}

			// Allocate and process while waiting for parallel tasks to finish
			uint16_t num_output_bones = 0;
			uint16_t* output_bone_mapping = create_output_bone_mapping(allocator, clip, num_output_bones);

			merge_database_transform_ranges_from_segments(mutable_track_database, segments, num_segments);
			detect_constant_tracks(mutable_track_database, segments, num_segments, settings.constant_rotation_threshold_angle, settings.constant_translation_threshold, settings.constant_scale_threshold);

			// Process every segment, this could be done in parallel
			acl::impl::quantization_context quant_context(allocator, mutable_track_database, raw_track_database, additive_base_track_database, settings, skeleton, segments[0]);
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				segment_context& segment = segments[segment_index];

				detect_segment_constant_tracks(mutable_track_database, segment);

				if (settings.range_reduction != RangeReductionFlags8::None)
					normalize_with_database_ranges(mutable_track_database, segment, settings.range_reduction);

				// After this point, if the rotation quat W component is dropped, it is no longer meaningful

				if (settings.segmenting.enabled && settings.segmenting.range_reduction != RangeReductionFlags8::None)
				{
					extract_segment_ranges(mutable_track_database, segment);
					normalize_with_segment_ranges(mutable_track_database, segment, settings.segmenting.range_reduction);
				}

				quantize_tracks(quant_context, segment, settings);

				segment.format_per_track_data_size = acl_impl::write_format_per_track_data(mutable_track_database, segment, output_bone_mapping, num_output_bones, nullptr);
				segment.range_data_size = acl_impl::write_segment_range_data(mutable_track_database, segment, settings.segmenting.range_reduction, output_bone_mapping, num_output_bones, nullptr);
				segment.animated_data_size = acl_impl::write_animated_track_data(mutable_track_database, segment, output_bone_mapping, num_output_bones, &segment.animated_pose_bit_size, nullptr);
			}

			const uint32_t constant_data_size = acl_impl::write_track_constant_values(mutable_track_database, segments, num_segments, output_bone_mapping, num_output_bones, nullptr);
			const uint32_t clip_range_data_size = acl_impl::write_clip_range_data(mutable_track_database, settings.range_reduction, output_bone_mapping, num_output_bones, nullptr);

			const uint32_t num_tracks_per_bone = has_scale ? 3 : 2;
			const uint32_t num_tracks = uint32_t(num_output_bones) * num_tracks_per_bone;
			const BitSetDescription bitset_desc = BitSetDescription::make_from_num_bits(num_tracks);

			// Adding an extra index at the end to delimit things, the index is always invalid: 0xFFFFFFFF
			const uint32_t segment_start_indices_size = num_segments > 1 ? (sizeof(uint32_t) * (num_segments + 1)) : 0;

			uint32_t buffer_size = 0;
			// Per clip data
			buffer_size += sizeof(CompressedClip);
			buffer_size += sizeof(ClipHeader);

			const uint32_t clip_header_size = buffer_size;

			buffer_size += segment_start_indices_size;							// Segment start indices
			buffer_size = align_to(buffer_size, 4);								// Align segment headers
			buffer_size += sizeof(SegmentHeader) * num_segments;				// Segment headers
			buffer_size = align_to(buffer_size, 4);								// Align bitsets

			const uint32_t clip_segment_header_size = buffer_size - clip_header_size;

			buffer_size += bitset_desc.get_num_bytes();							// Default tracks bitset
			buffer_size += bitset_desc.get_num_bytes();							// Constant tracks bitset
			buffer_size = align_to(buffer_size, 4);								// Align constant track data
			buffer_size += constant_data_size;									// Constant track data
			buffer_size = align_to(buffer_size, 4);								// Align range data
			buffer_size += clip_range_data_size;								// Range data

			const uint32_t clip_data_size = buffer_size - clip_segment_header_size - clip_header_size;

			// Per segment data
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				segment_context& segment = segments[segment_index];

				const uint32_t header_start = buffer_size;

				buffer_size += segment.format_per_track_data_size;				// Format per track data
				// TODO: Alignment only necessary with 16bit per component
				buffer_size = align_to(buffer_size, 2);							// Align range data
				buffer_size += segment.range_data_size;							// Range data

				const uint32_t header_end = buffer_size;

				// TODO: Variable bit rate doesn't need alignment
				buffer_size = align_to(buffer_size, 4);							// Align animated data
				buffer_size += segment.animated_data_size;						// Animated track data

				segment.total_header_size = header_end - header_start;
				segment.total_size = buffer_size - header_start;
			}

			// Ensure we have sufficient padding for unaligned 16 byte loads
			buffer_size += 15;

			uint8_t* buffer = allocate_type_array_aligned<uint8_t>(allocator, buffer_size, 16);

			CompressedClip* compressed_clip = make_compressed_clip(buffer, buffer_size, AlgorithmType8::UniformlySampled);

			ClipHeader& header = get_clip_header(*compressed_clip);
			header.num_bones = num_output_bones;
			header.num_segments = safe_static_cast<uint16_t>(num_segments);
			header.rotation_format = settings.rotation_format;
			header.translation_format = settings.translation_format;
			header.scale_format = settings.scale_format;
			header.clip_range_reduction = settings.range_reduction;
			header.segment_range_reduction = settings.segmenting.range_reduction;
			header.has_scale = has_scale ? 1 : 0;
			header.default_scale = additive_base_clip == nullptr || clip.get_additive_format() != AdditiveClipFormat8::Additive1;
			header.num_samples = num_samples;
			header.sample_rate = clip.get_sample_rate();
			header.segment_start_indices_offset = sizeof(ClipHeader);
			header.segment_headers_offset = align_to(header.segment_start_indices_offset + segment_start_indices_size, 4);
			header.default_tracks_bitset_offset = align_to(header.segment_headers_offset + (sizeof(SegmentHeader) * num_segments), 4);
			header.constant_tracks_bitset_offset = header.default_tracks_bitset_offset + bitset_desc.get_num_bytes();
			header.constant_track_data_offset = align_to(header.constant_tracks_bitset_offset + bitset_desc.get_num_bytes(), 4);
			header.clip_range_data_offset = align_to(header.constant_track_data_offset + constant_data_size, 4);

			if (num_segments > 1)
				acl_impl::write_segment_start_indices(segments, num_segments, header.get_segment_start_indices());
			else
				header.segment_start_indices_offset = InvalidPtrOffset();

			const uint32_t segment_data_start_offset = header.clip_range_data_offset + clip_range_data_size;
			acl_impl::write_segment_headers(segments, num_segments, segment_data_start_offset, header.get_segment_headers());
			acl_impl::write_default_track_bitset(mutable_track_database, output_bone_mapping, num_output_bones, header.get_default_tracks_bitset(), bitset_desc);
			acl_impl::write_constant_track_bitset(mutable_track_database, output_bone_mapping, num_output_bones, header.get_constant_tracks_bitset(), bitset_desc);

			if (constant_data_size != 0)
				acl_impl::write_track_constant_values(mutable_track_database, segments, num_segments, output_bone_mapping, num_output_bones, header.get_constant_track_data());
			else
				header.constant_track_data_offset = InvalidPtrOffset();

			if (settings.range_reduction != RangeReductionFlags8::None)
				acl_impl::write_clip_range_data(mutable_track_database, settings.range_reduction, output_bone_mapping, num_output_bones, header.get_clip_range_data());
			else
				header.clip_range_data_offset = InvalidPtrOffset();

			acl_impl::write_segment_data(mutable_track_database, segments, num_segments, settings.segmenting.range_reduction, header, output_bone_mapping, num_output_bones);

			finalize_compressed_clip(*compressed_clip);

			compression_time.stop();

#if defined(SJSON_CPP_WRITER)
			if (out_stats.logging != StatLogging::None)
				acl_impl::write_stats(allocator, clip, skeleton, settings,
					mutable_track_database, raw_track_database, additive_base_track_database,
					segments, num_segments, *compressed_clip, header, compression_time, clip_header_size, clip_data_size, out_stats);
#endif

			deallocate_type_array(allocator, output_bone_mapping, num_output_bones);
			destroy_segments(allocator, segments, num_segments);
			deallocate_type(allocator, additive_base_track_database);

			out_compressed_clip = compressed_clip;
			return ErrorResult();
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
