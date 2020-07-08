#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/buffer_tag.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/error.h"
#include "acl/core/error_result.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/core/scope_profiler.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/output_stats.h"
#include "acl/compression/track_array.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/constant_track_impl.h"
#include "acl/compression/impl/normalize_track_impl.h"
#include "acl/compression/impl/quantize_track_impl.h"
#include "acl/compression/impl/track_list_context.h"
#include "acl/compression/impl/track_range_impl.h"
#include "acl/compression/impl/write_compression_stats_impl.h"
#include "acl/compression/impl/write_track_data_impl.h"

#include "acl/compression/impl/track_stream.h"
#include "acl/compression/impl/convert_rotation_streams.h"
#include "acl/compression/impl/compact_constant_streams.h"
#include "acl/compression/impl/normalize_streams.h"
#include "acl/compression/impl/quantize_streams.h"
#include "acl/compression/impl/segment_streams.h"
#include "acl/compression/impl/write_segment_data.h"
#include "acl/compression/impl/write_stats.h"
#include "acl/compression/impl/write_stream_bitsets.h"
#include "acl/compression/impl/write_stream_data.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline ErrorResult compress_scalar_track_list(IAllocator& allocator, const track_array& track_list, compressed_tracks*& out_compressed_tracks, OutputStats& out_stats)
		{
#if defined(SJSON_CPP_WRITER)
			ScopeProfiler compression_time;
#endif

			track_list_context context;
			initialize_context(allocator, track_list, context);

			extract_track_ranges(context);
			extract_constant_tracks(context);
			normalize_tracks(context);
			quantize_tracks(context);

			// Done transforming our input tracks, time to pack them into their final form
			const uint32_t per_track_metadata_size = write_track_metadata(context, nullptr);
			const uint32_t constant_values_size = write_track_constant_values(context, nullptr);
			const uint32_t range_values_size = write_track_range_values(context, nullptr);
			const uint32_t animated_num_bits = write_track_animated_values(context, nullptr);
			const uint32_t animated_values_size = (animated_num_bits + 7) / 8;		// Round up to nearest byte
			const uint32_t num_bits_per_frame = context.num_samples != 0 ? (animated_num_bits / context.num_samples) : 0;

			uint32_t buffer_size = 0;
			buffer_size += sizeof(compressed_tracks);								// Headers
			buffer_size += sizeof(scalar_tracks_header);							// Header
			ACL_ASSERT(is_aligned_to(buffer_size, alignof(track_metadata)), "Invalid alignment");
			buffer_size += per_track_metadata_size;									// Per track metadata
			buffer_size = align_to(buffer_size, 4);									// Align constant values
			buffer_size += constant_values_size;									// Constant values
			ACL_ASSERT(is_aligned_to(buffer_size, 4), "Invalid alignment");
			buffer_size += range_values_size;										// Range values
			ACL_ASSERT(is_aligned_to(buffer_size, 4), "Invalid alignment");
			buffer_size += animated_values_size;									// Animated values
			buffer_size += 15;														// Ensure we have sufficient padding for unaligned 16 byte loads

			uint8_t* buffer = allocate_type_array_aligned<uint8_t>(allocator, buffer_size, alignof(compressed_tracks));
			std::memset(buffer, 0, buffer_size);

			const uint8_t* buffer_start = buffer;
			out_compressed_tracks = reinterpret_cast<compressed_tracks*>(buffer);

			raw_buffer_header* buffer_header = safe_ptr_cast<raw_buffer_header>(buffer);
			buffer += sizeof(raw_buffer_header);

			tracks_header* header = safe_ptr_cast<tracks_header>(buffer);
			buffer += sizeof(tracks_header);

			// Write our primary header
			header->tag = static_cast<uint32_t>(buffer_tag32::compressed_tracks);
			header->version = get_algorithm_version(algorithm_type8::uniformly_sampled);
			header->algorithm_type = algorithm_type8::uniformly_sampled;
			header->track_type = track_list.get_track_type();
			header->num_tracks = context.num_tracks;
			header->num_samples = context.num_samples;
			header->sample_rate = context.sample_rate;

			// Write our scalar tracks header
			scalar_tracks_header* scalars_header = safe_ptr_cast<scalar_tracks_header>(buffer);
			buffer += sizeof(scalar_tracks_header);

			scalars_header->num_bits_per_frame = num_bits_per_frame;

			const uint8_t* packed_data_start_offset = buffer - sizeof(scalar_tracks_header);	// Relative to our header
			scalars_header->metadata_per_track = buffer - packed_data_start_offset;
			buffer += per_track_metadata_size;
			buffer = align_to(buffer, 4);
			scalars_header->track_constant_values = buffer - packed_data_start_offset;
			buffer += constant_values_size;
			scalars_header->track_range_values = buffer - packed_data_start_offset;
			buffer += range_values_size;
			scalars_header->track_animated_values = buffer - packed_data_start_offset;
			buffer += animated_values_size;
			buffer += 15;

			(void)buffer_start;	// Avoid VS2017 bug, it falsely reports this variable as unused even when asserts are enabled
			ACL_ASSERT((buffer_start + buffer_size) == buffer, "Buffer size and pointer mismatch");

			// Write our compressed data
			track_metadata* per_track_metadata = scalars_header->get_track_metadata();
			write_track_metadata(context, per_track_metadata);

			float* constant_values = scalars_header->get_track_constant_values();
			write_track_constant_values(context, constant_values);

			float* range_values = scalars_header->get_track_range_values();
			write_track_range_values(context, range_values);

			uint8_t* animated_values = scalars_header->get_track_animated_values();
			write_track_animated_values(context, animated_values);

			// Finish the raw buffer header
			buffer_header->size = buffer_size;
			buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(header), buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

#if defined(ACL_HAS_ASSERT_CHECKS)
			for (const uint8_t* padding = buffer - 15; padding < buffer; ++padding)
				ACL_ASSERT(*padding == 0, "Padding was overwritten");
#endif

#if defined(SJSON_CPP_WRITER)
			compression_time.stop();

			if (out_stats.logging != StatLogging::None)
				write_compression_stats(context, *out_compressed_tracks, compression_time, out_stats);
#endif

			return ErrorResult();
		}

		inline ErrorResult compress_transform_track_list(IAllocator& allocator, const track_array_qvvf& track_list, compression_settings settings, const track_array_qvvf* additive_base_track_list, additive_clip_format8 additive_format,
			compressed_tracks*& out_compressed_tracks, OutputStats& out_stats)
		{
			ErrorResult error_result = settings.is_valid();
			if (error_result.any())
				return error_result;

#if defined(SJSON_CPP_WRITER)
			ScopeProfiler compression_time;
#endif

			// If every track is retains full precision, we disable segmenting since it provides no benefit
			if (!is_rotation_format_variable(settings.rotation_format) && !is_vector_format_variable(settings.translation_format) && !is_vector_format_variable(settings.scale_format))
			{
				settings.segmenting.ideal_num_samples = 0xFFFF;
				settings.segmenting.max_num_samples = 0xFFFF;
			}

			// Variable bit rate tracks need range reduction
			// Full precision tracks do not need range reduction since samples are stored raw
			range_reduction_flags8 range_reduction = range_reduction_flags8::none;
			if (is_rotation_format_variable(settings.rotation_format))
				range_reduction |= range_reduction_flags8::rotations;

			if (is_vector_format_variable(settings.translation_format))
				range_reduction |= range_reduction_flags8::translations;

			if (is_vector_format_variable(settings.scale_format))
				range_reduction |= range_reduction_flags8::scales;

			ClipContext raw_clip_context;
			initialize_clip_context(allocator, track_list, additive_format, raw_clip_context);

			ClipContext clip_context;
			initialize_clip_context(allocator, track_list, additive_format, clip_context);

			const bool is_additive = additive_base_track_list != nullptr && additive_format != additive_clip_format8::none;
			ClipContext additive_base_clip_context;
			if (is_additive)
				initialize_clip_context(allocator, *additive_base_track_list, additive_format, additive_base_clip_context);

			convert_rotation_streams(allocator, clip_context, settings.rotation_format);

			// Extract our clip ranges now, we need it for compacting the constant streams
			extract_clip_bone_ranges(allocator, clip_context);

			// Compact and collapse the constant streams
			compact_constant_streams(allocator, clip_context, track_list);

			uint32_t clip_range_data_size = 0;
			if (range_reduction != range_reduction_flags8::none)
			{
				normalize_clip_streams(clip_context, range_reduction);
				clip_range_data_size = get_stream_range_data_size(clip_context, range_reduction, settings.rotation_format);
			}

			segment_streams(allocator, clip_context, settings.segmenting);

			// If we have a single segment, skip segment range reduction since it won't help
			if (range_reduction != range_reduction_flags8::none && clip_context.num_segments > 1)
			{
				extract_segment_bone_ranges(allocator, clip_context);
				normalize_segment_streams(clip_context, range_reduction);
			}

			quantize_streams(allocator, clip_context, settings, raw_clip_context, additive_base_clip_context, out_stats);

			uint32_t num_output_bones = 0;
			uint32_t* output_bone_mapping = create_output_track_mapping(allocator, track_list, num_output_bones);

			const uint32_t constant_data_size = get_constant_data_size(clip_context, output_bone_mapping, num_output_bones);

			calculate_animated_data_size(clip_context, output_bone_mapping, num_output_bones);

			const uint32_t format_per_track_data_size = get_format_per_track_data_size(clip_context, settings.rotation_format, settings.translation_format, settings.scale_format);

			const uint32_t num_tracks_per_bone = clip_context.has_scale ? 3 : 2;
			const uint32_t num_tracks = uint32_t(num_output_bones) * num_tracks_per_bone;
			const BitSetDescription bitset_desc = BitSetDescription::make_from_num_bits(num_tracks);

			// Adding an extra index at the end to delimit things, the index is always invalid: 0xFFFFFFFF
			const uint32_t segment_start_indices_size = clip_context.num_segments > 1 ? (sizeof(uint32_t) * (clip_context.num_segments + 1)) : 0;
			const uint32_t segment_headers_size = sizeof(segment_header) * clip_context.num_segments;

			uint32_t buffer_size = 0;
			// Per clip data
			buffer_size += sizeof(compressed_tracks);							// Headers
			buffer_size += sizeof(transform_tracks_header);						// Header

			const uint32_t clip_header_size = buffer_size;

			buffer_size += segment_start_indices_size;							// Segment start indices
			buffer_size = align_to(buffer_size, 4);								// Align segment headers
			buffer_size += segment_headers_size;								// Segment headers
			buffer_size = align_to(buffer_size, 4);								// Align bitsets

			const uint32_t clip_segment_header_size = buffer_size - clip_header_size;

			buffer_size += bitset_desc.get_num_bytes();							// Default tracks bitset
			buffer_size += bitset_desc.get_num_bytes();							// Constant tracks bitset
			buffer_size = align_to(buffer_size, 4);								// Align constant track data
			buffer_size += constant_data_size;									// Constant track data
			buffer_size = align_to(buffer_size, 4);								// Align range data
			buffer_size += clip_range_data_size;								// Range data

			const uint32_t clip_data_size = buffer_size - clip_segment_header_size - clip_header_size;

			if (are_all_enum_flags_set(out_stats.logging, StatLogging::Detailed))
			{
				constexpr uint32_t k_cache_line_byte_size = 64;
				clip_context.decomp_touched_bytes = clip_header_size + clip_data_size;
				clip_context.decomp_touched_bytes += sizeof(uint32_t) * 4;			// We touch at most 4 segment start indices
				clip_context.decomp_touched_bytes += sizeof(segment_header) * 2;	// We touch at most 2 segment headers
				clip_context.decomp_touched_cache_lines = align_to(clip_header_size, k_cache_line_byte_size) / k_cache_line_byte_size;
				clip_context.decomp_touched_cache_lines += align_to(clip_data_size, k_cache_line_byte_size) / k_cache_line_byte_size;
				clip_context.decomp_touched_cache_lines += 1;						// All 4 segment start indices should fit in a cache line
				clip_context.decomp_touched_cache_lines += 1;						// Both segment headers should fit in a cache line
			}

			// Per segment data
			for (SegmentContext& segment : clip_context.segment_iterator())
			{
				const uint32_t header_start = buffer_size;

				buffer_size += format_per_track_data_size;						// Format per track data
				// TODO: Alignment only necessary with 16bit per component (segment constant tracks), need to fix scalar decoding path
				buffer_size = align_to(buffer_size, 2);							// Align range data
				buffer_size += segment.range_data_size;							// Range data

				const uint32_t header_end = buffer_size;

				// TODO: Variable bit rate doesn't need alignment
				buffer_size = align_to(buffer_size, 4);							// Align animated data
				buffer_size += segment.animated_data_size;						// Animated track data

				segment.total_header_size = header_end - header_start;
			}

			// Ensure we have sufficient padding for unaligned 16 byte loads
			buffer_size += 15;

			uint8_t* buffer = allocate_type_array_aligned<uint8_t>(allocator, buffer_size, alignof(compressed_tracks));
			std::memset(buffer, 0, buffer_size);

			const uint8_t* buffer_start = buffer;
			out_compressed_tracks = reinterpret_cast<compressed_tracks*>(buffer);

			raw_buffer_header* buffer_header = safe_ptr_cast<raw_buffer_header>(buffer);
			buffer += sizeof(raw_buffer_header);

			tracks_header* header = safe_ptr_cast<tracks_header>(buffer);
			buffer += sizeof(tracks_header);

			// Write our primary header
			header->tag = static_cast<uint32_t>(buffer_tag32::compressed_tracks);
			header->version = get_algorithm_version(algorithm_type8::uniformly_sampled);
			header->algorithm_type = algorithm_type8::uniformly_sampled;
			header->track_type = track_list.get_track_type();
			header->num_tracks = num_output_bones;
			header->num_samples = track_list.get_num_samples_per_track();
			header->sample_rate = track_list.get_sample_rate();

			// Write our transform tracks header
			transform_tracks_header* transforms_header = safe_ptr_cast<transform_tracks_header>(buffer);
			buffer += sizeof(transform_tracks_header);

			transforms_header->num_segments = clip_context.num_segments;
			transforms_header->rotation_format = settings.rotation_format;
			transforms_header->translation_format = settings.translation_format;
			transforms_header->scale_format = settings.scale_format;
			transforms_header->has_scale = clip_context.has_scale ? 1 : 0;
			// Our default scale is 1.0 if we have no additive base or if we don't use 'additive1', otherwise it is 0.0
			transforms_header->default_scale = !is_additive || additive_format != additive_clip_format8::additive1 ? 1 : 0;
			transforms_header->segment_start_indices_offset = sizeof(transform_tracks_header);	// Relative to the start of our header
			transforms_header->segment_headers_offset = align_to(transforms_header->segment_start_indices_offset + segment_start_indices_size, 4);
			transforms_header->default_tracks_bitset_offset = align_to(transforms_header->segment_headers_offset + segment_headers_size, 4);
			transforms_header->constant_tracks_bitset_offset = transforms_header->default_tracks_bitset_offset + bitset_desc.get_num_bytes();
			transforms_header->constant_track_data_offset = align_to(transforms_header->constant_tracks_bitset_offset + bitset_desc.get_num_bytes(), 4);
			transforms_header->clip_range_data_offset = align_to(transforms_header->constant_track_data_offset + constant_data_size, 4);

			uint32_t written_segment_start_indices_size = 0;
			if (clip_context.num_segments > 1)
				written_segment_start_indices_size = write_segment_start_indices(clip_context, transforms_header->get_segment_start_indices());
			else
				transforms_header->segment_start_indices_offset = InvalidPtrOffset();

			const uint32_t segment_data_start_offset = transforms_header->clip_range_data_offset + clip_range_data_size;
			const uint32_t written_segment_headers_size = write_segment_headers(clip_context, settings, transforms_header->get_segment_headers(), segment_data_start_offset);

			uint32_t written_bitset_size = 0;
			written_bitset_size += write_default_track_bitset(clip_context, transforms_header->get_default_tracks_bitset(), bitset_desc, output_bone_mapping, num_output_bones);
			written_bitset_size += write_constant_track_bitset(clip_context, transforms_header->get_constant_tracks_bitset(), bitset_desc, output_bone_mapping, num_output_bones);

			uint32_t written_constant_data_size = 0;
			if (constant_data_size > 0)
				written_constant_data_size = write_constant_track_data(clip_context, transforms_header->get_constant_track_data(), constant_data_size, output_bone_mapping, num_output_bones);
			else
				transforms_header->constant_track_data_offset = InvalidPtrOffset();

			uint32_t written_clip_range_data_size = 0;
			if (range_reduction != range_reduction_flags8::none)
				written_clip_range_data_size = write_clip_range_data(clip_context, range_reduction, transforms_header->get_clip_range_data(), clip_range_data_size, output_bone_mapping, num_output_bones);
			else
				transforms_header->clip_range_data_offset = InvalidPtrOffset();

			const uint32_t written_segment_data_size = write_segment_data(clip_context, settings, range_reduction, *transforms_header, output_bone_mapping, num_output_bones);

#if defined(ACL_HAS_ASSERT_CHECKS)
			{
				// Make sure we wrote the right amount of data
				buffer += written_segment_start_indices_size;
				buffer = align_to(buffer, 4);								// Align segment headers
				buffer += written_segment_headers_size;
				buffer = align_to(buffer, 4);								// Align bitsets
				buffer += written_bitset_size;
				buffer = align_to(buffer, 4);								// Align constant track data
				buffer += written_constant_data_size;
				buffer = align_to(buffer, 4);								// Align range data
				buffer += written_clip_range_data_size;
				buffer += written_segment_data_size;

				// Ensure we have sufficient padding for unaligned 16 byte loads
				buffer += 15;

				(void)buffer_start;	// Avoid VS2017 bug, it falsely reports this variable as unused even when asserts are enabled
				ACL_ASSERT(written_segment_start_indices_size == segment_start_indices_size, "Wrote too little or too much data");
				ACL_ASSERT(written_segment_headers_size == segment_headers_size, "Wrote too little or too much data");
				ACL_ASSERT(written_bitset_size == (bitset_desc.get_num_bytes() * 2), "Wrote too little or too much data");
				ACL_ASSERT(written_constant_data_size == constant_data_size, "Wrote too little or too much data");
				ACL_ASSERT(written_clip_range_data_size == clip_range_data_size, "Wrote too little or too much data");
				ACL_ASSERT(uint32_t(buffer - buffer_start) == buffer_size, "Wrote too little or too much data");
				for (const uint8_t* padding = buffer - 15; padding < buffer; ++padding)
					ACL_ASSERT(*padding == 0, "Padding was overwritten");
			}
#else
			(void)written_segment_start_indices_size;
			(void)written_segment_headers_size;
			(void)written_bitset_size;
			(void)written_constant_data_size;
			(void)written_clip_range_data_size;
			(void)written_segment_data_size;
			(void)buffer_start;
#endif
			

			// Finish the raw buffer header
			buffer_header->size = buffer_size;
			buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(header), buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

#if defined(SJSON_CPP_WRITER)
			compression_time.stop();

			if (out_stats.logging != StatLogging::None)
				write_stats(allocator, track_list, clip_context, *out_compressed_tracks, settings, raw_clip_context, additive_base_clip_context, compression_time, out_stats);
#endif

			deallocate_type_array(allocator, output_bone_mapping, num_output_bones);
			destroy_clip_context(allocator, clip_context);
			destroy_clip_context(allocator, raw_clip_context);

			if (is_additive)
				destroy_clip_context(allocator, additive_base_clip_context);

			return ErrorResult();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Compresses a track array with uniform sampling.
	//
	// This compression algorithm is the simplest by far and as such it offers
	// the fastest compression and decompression. Every sample is retained and
	// every track has the same number of samples playing back at the same
	// sample rate. This means that when we sample at a particular time within
	// the clip, we can trivially calculate the offsets required to read the
	// desired data. All the data is sorted in order to ensure all reads are
	// as contiguous as possible for optimal cache locality during decompression.
	//
	//    allocator:				The allocator instance to use to allocate and free memory.
	//    track_list:				The track list to compress.
	//    settings:					The compression settings to use.
	//    out_compressed_tracks:	The resulting compressed tracks. The caller owns the returned memory and must free it.
	//    out_stats:				Stat output structure.
	//////////////////////////////////////////////////////////////////////////
	inline ErrorResult compress_track_list(IAllocator& allocator, const track_array& track_list, const compression_settings& settings, compressed_tracks*& out_compressed_tracks, OutputStats& out_stats)
	{
		using namespace acl_impl;

		ErrorResult error_result = track_list.is_valid();
		if (error_result.any())
			return error_result;

		if (track_list.get_num_samples_per_track() > 0xFFFF)
			return ErrorResult("ACL only supports up to 65535 samples");

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		if (track_list.get_track_category() == track_category8::transformf)
			error_result = compress_transform_track_list(allocator, track_array_cast<track_array_qvvf>(track_list), settings, nullptr, additive_clip_format8::none, out_compressed_tracks, out_stats);
		else
			error_result = compress_scalar_track_list(allocator, track_list, out_compressed_tracks, out_stats);

		return error_result;
	}

	//////////////////////////////////////////////////////////////////////////
	// Compresses a transform track array and using its additive base and uniform sampling.
	//
	// This compression algorithm is the simplest by far and as such it offers
	// the fastest compression and decompression. Every sample is retained and
	// every track has the same number of samples playing back at the same
	// sample rate. This means that when we sample at a particular time within
	// the clip, we can trivially calculate the offsets required to read the
	// desired data. All the data is sorted in order to ensure all reads are
	// as contiguous as possible for optimal cache locality during decompression.
	//
	//    allocator:				The allocator instance to use to allocate and free memory.
	//    track_list:				The track list to compress.
	//    settings:					The compression settings to use.
	//    out_compressed_tracks:	The resulting compressed tracks. The caller owns the returned memory and must free it.
	//    out_stats:				Stat output structure.
	//////////////////////////////////////////////////////////////////////////
	inline ErrorResult compress_track_list(IAllocator& allocator, const track_array_qvvf& track_list, const compression_settings& settings, const track_array_qvvf& additive_base_track_list, additive_clip_format8 additive_format, compressed_tracks*& out_compressed_tracks, OutputStats& out_stats)
	{
		using namespace acl_impl;

		ErrorResult error_result = track_list.is_valid();
		if (error_result.any())
			return error_result;

		error_result = additive_base_track_list.is_valid();
		if (error_result.any())
			return error_result;

		if (track_list.get_num_samples_per_track() > 0xFFFF)
			return ErrorResult("ACL only supports up to 65535 samples");

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		return compress_transform_track_list(allocator, track_list, settings, &additive_base_track_list, additive_format, out_compressed_tracks, out_stats);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
