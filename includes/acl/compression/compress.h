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
#include "acl/compression/impl/constant_track_impl.h"
#include "acl/compression/impl/normalize_track_impl.h"
#include "acl/compression/impl/quantize_track_impl.h"
#include "acl/compression/impl/track_list_context.h"
#include "acl/compression/impl/track_range_impl.h"
#include "acl/compression/impl/write_compression_stats_impl.h"
#include "acl/compression/impl/write_track_data_impl.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
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

		(void)out_stats;

		ErrorResult error_result = track_list.is_valid();
		if (error_result.any())
			return error_result;

		error_result = settings.is_valid();
		if (error_result.any())
			return error_result;

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

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
		header->num_bits_per_frame = num_bits_per_frame;

		header->metadata_per_track = buffer - buffer_start;
		buffer += per_track_metadata_size;
		buffer = align_to(buffer, 4);
		header->track_constant_values = buffer - buffer_start;
		buffer += constant_values_size;
		header->track_range_values = buffer - buffer_start;
		buffer += range_values_size;
		header->track_animated_values = buffer - buffer_start;
		buffer += animated_values_size;
		buffer += 15;

		ACL_ASSERT(buffer_start + buffer_size == buffer, "Buffer size and pointer mismatch");

		// Write our compressed data
		track_metadata* per_track_metadata = header->get_track_metadata();
		write_track_metadata(context, per_track_metadata);

		float* constant_values = header->get_track_constant_values();
		write_track_constant_values(context, constant_values);

		float* range_values = header->get_track_range_values();
		write_track_range_values(context, range_values);

		uint8_t* animated_values = header->get_track_animated_values();
		write_track_animated_values(context, animated_values);

		// Finish the raw buffer header
		buffer_header->size = buffer_size;
		buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(header), buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

#if defined(SJSON_CPP_WRITER)
		compression_time.stop();

		if (out_stats.logging != StatLogging::None)
			write_compression_stats(context, *out_compressed_tracks, compression_time, out_stats);
#endif

		return ErrorResult();
	}
}

ACL_IMPL_FILE_PRAGMA_POP
