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

#include "acl/core/memory.h"
#include "acl/core/error.h"
#include "acl/core/bitset.h"
#include "acl/core/enum_utils.h"
#include "acl/core/algorithm_types.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/algorithm/uniformly_sampled/common.h"
#include "acl/compression/compressed_clip_impl.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/stream/clip_context.h"
#include "acl/compression/stream/track_stream.h"
#include "acl/compression/stream/convert_rotation_streams.h"
#include "acl/compression/stream/compact_constant_streams.h"
#include "acl/compression/stream/normalize_streams.h"
#include "acl/compression/stream/quantize_streams.h"
#include "acl/compression/stream/segment_streams.h"
#include "acl/compression/stream/write_stream_bitsets.h"
#include "acl/compression/stream/write_stream_data.h"
#include "acl/compression/stream/write_range_data.h"

#include <stdint.h>
#include <cstdio>

//////////////////////////////////////////////////////////////////////////
// Full Precision Encoder
//
// The goal of the full precision format is to be used as a reference
// point for compression speed, compressed size, and decompression speed.
// This will not be a raw format in that we will at least drop constant
// or bind pose tracks. As such, it is near-raw but not quite.
//
// This is the highest precision encoder and the fastest to compress.
//
// Data format:
//    TODO: Detail the format
//////////////////////////////////////////////////////////////////////////

namespace acl
{
	namespace uniformly_sampled
	{
		struct CompressionSettings
		{
			RotationFormat8 rotation_format;
			VectorFormat8 translation_format;

			RangeReductionFlags8 range_reduction;

			SegmentingSettings segmenting;

			CompressionSettings()
				: rotation_format(RotationFormat8::Quat_128)
				, translation_format(VectorFormat8::Vector3_96)
				, range_reduction(RangeReductionFlags8::None)
				, segmenting()
			{}
		};

		namespace impl
		{
			inline void write_segment_headers(const ClipContext& clip_context, const CompressionSettings& settings, SegmentHeader* segment_headers, uint16_t segment_headers_start_offset)
			{
				uint32_t format_per_track_data_size = get_format_per_track_data_size(clip_context, settings.rotation_format, settings.translation_format);

				uint32_t data_offset = segment_headers_start_offset;
				for (uint16_t segment_index = 0; segment_index < clip_context.num_segments; ++segment_index)
				{
					const SegmentContext& segment = clip_context.segments[segment_index];
					SegmentHeader& header = segment_headers[segment_index];

					header.num_samples = segment.num_samples;
					header.animated_pose_bit_size = segment.animated_pose_bit_size;
					header.format_per_track_data_offset = data_offset;
					header.range_data_offset = align_to(header.format_per_track_data_offset + format_per_track_data_size, 2);		// Aligned to 2 bytes
					header.track_data_offset = align_to(header.range_data_offset + segment.range_data_size, 4);						// Aligned to 4 bytes

					data_offset = header.track_data_offset + segment.animated_data_size;
				}
			}

			inline void write_segment_data(const ClipContext& clip_context, const CompressionSettings& settings, ClipHeader& header)
			{
				SegmentHeader* segment_headers = header.get_segment_headers();
				uint32_t format_per_track_data_size = get_format_per_track_data_size(clip_context, settings.rotation_format, settings.translation_format);

				for (uint16_t segment_index = 0; segment_index < clip_context.num_segments; ++segment_index)
				{
					const SegmentContext& segment = clip_context.segments[segment_index];
					SegmentHeader& segment_header = segment_headers[segment_index];

					if (format_per_track_data_size > 0)
						write_format_per_track_data(segment.bone_streams, segment.num_bones, header.get_format_per_track_data(segment_header), format_per_track_data_size);
					else
						segment_header.format_per_track_data_offset = InvalidPtrOffset();

					if (segment.range_data_size > 0)
						write_segment_range_data(segment, settings.range_reduction, header.get_segment_range_data(segment_header), segment.range_data_size);
					else
						segment_header.range_data_offset = InvalidPtrOffset();

					if (segment.animated_data_size > 0)
						write_animated_track_data(segment, settings.rotation_format, settings.translation_format, header.get_track_data(segment_header), segment.animated_data_size);
					else
						segment_header.track_data_offset = InvalidPtrOffset();
				}
			}
		}

		// Encoder entry point
		inline CompressedClip* compress_clip(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, const CompressionSettings& settings)
		{
			using namespace impl;

			uint16_t num_bones = clip.get_num_bones();
			uint32_t num_samples = clip.get_num_samples();

			if (ACL_TRY_ASSERT(num_bones > 0, "Clip has no bones!"))
				return nullptr;
			if (ACL_TRY_ASSERT(num_samples > 0, "Clip has no samples!"))
				return nullptr;

			if (settings.translation_format != VectorFormat8::Vector3_96)
			{
				bool has_clip_range_reduction = is_enum_flag_set(settings.range_reduction, RangeReductionFlags8::Translations);
				bool has_segment_range_reduction = settings.segmenting.enabled && is_enum_flag_set(settings.segmenting.range_reduction, RangeReductionFlags8::Translations);
				if (ACL_TRY_ASSERT(has_clip_range_reduction | has_segment_range_reduction, "%s quantization requires range reduction to be enabled at the clip or segment level!", get_vector_format_name(settings.translation_format)))
					return nullptr;
			}

			if (settings.segmenting.enabled && settings.segmenting.range_reduction != RangeReductionFlags8::None)
			{
				if (ACL_TRY_ASSERT(settings.range_reduction != RangeReductionFlags8::None, "Per segment range reduction requires per clip range reduction to be enabled!"))
					return nullptr;
			}

			ClipContext raw_clip_context;
			initialize_clip_context(allocator, clip, raw_clip_context);

			ClipContext clip_context;
			initialize_clip_context(allocator, clip, clip_context);

			convert_rotation_streams(allocator, clip_context, settings.rotation_format);

			// Extract our clip ranges now, we need it for compacting the constant streams
			extract_clip_bone_ranges(allocator, clip_context);

			// TODO: Expose this, especially the translation threshold depends on the unit scale.
			// Centimeters VS meters, a different threshold should be used. Perhaps we should pass an
			// argument to the compression algorithm that states the units used or we should force centimeters
			compact_constant_streams(allocator, clip_context, 0.00001f, 0.001f);

			uint32_t clip_range_data_size = 0;
			if (settings.range_reduction != RangeReductionFlags8::None)
			{
				normalize_clip_streams(clip_context, settings.range_reduction);
				clip_range_data_size = get_stream_range_data_size(clip_context, settings.range_reduction, settings.rotation_format, settings.translation_format);
			}

			if (settings.segmenting.enabled)
			{
				segment_streams(allocator, clip_context, settings.segmenting);

				if (settings.segmenting.range_reduction != RangeReductionFlags8::None)
				{
					extract_segment_bone_ranges(allocator, clip_context);
					normalize_segment_streams(clip_context, settings.range_reduction);
				}
			}

			quantize_streams(allocator, clip_context, settings.rotation_format, settings.translation_format, clip, skeleton, raw_clip_context);

			const SegmentContext& clip_segment = clip_context.segments[0];

			uint32_t constant_data_size = get_constant_data_size(clip_context);

			for (SegmentContext& segment : clip_context.segment_iterator())
				segment.animated_data_size = get_animated_data_size(segment, settings.rotation_format, settings.translation_format, segment.animated_pose_bit_size);

			uint32_t format_per_track_data_size = get_format_per_track_data_size(clip_context, settings.rotation_format, settings.translation_format);

			uint32_t bitset_size = get_bitset_size(num_bones * Constants::NUM_TRACKS_PER_BONE);

			uint32_t buffer_size = 0;
			// Per clip data
			buffer_size += sizeof(CompressedClip);
			buffer_size += sizeof(ClipHeader);
			buffer_size += sizeof(SegmentHeader) * clip_context.num_segments;	// Segment headers
			buffer_size += sizeof(uint32_t) * bitset_size;		// Default tracks bitset
			buffer_size += sizeof(uint32_t) * bitset_size;		// Constant tracks bitset
			buffer_size = align_to(buffer_size, 4);				// Align constant track data
			buffer_size += constant_data_size;					// Constant track data
			buffer_size = align_to(buffer_size, 4);				// Align range data
			buffer_size += clip_range_data_size;				// Range data
			// Per segment data
			for (const SegmentContext& segment : clip_context.segment_iterator())
			{
				buffer_size += format_per_track_data_size;			// Format per track data
				buffer_size = align_to(buffer_size, 2);				// Align range data
				buffer_size += segment.range_data_size;				// Range data
				buffer_size = align_to(buffer_size, 4);				// Align animated data
				buffer_size += segment.animated_data_size;			// Animated track data
			}

			uint8_t* buffer = allocate_type_array_aligned<uint8_t>(allocator, buffer_size, 16);

			CompressedClip* compressed_clip = make_compressed_clip(buffer, buffer_size, AlgorithmType8::UniformlySampled);

			ClipHeader& header = get_clip_header(*compressed_clip);
			header.num_bones = num_bones;
			header.num_segments = clip_context.num_segments;
			header.rotation_format = settings.rotation_format;
			header.translation_format = settings.translation_format;
			header.clip_range_reduction = settings.range_reduction;
			header.segment_range_reduction = settings.segmenting.range_reduction;
			header.num_samples = num_samples;
			header.sample_rate = clip.get_sample_rate();
			header.segment_headers_offset = sizeof(ClipHeader);
			header.default_tracks_bitset_offset = header.segment_headers_offset + (sizeof(SegmentHeader) * clip_context.num_segments);
			header.constant_tracks_bitset_offset = header.default_tracks_bitset_offset + (sizeof(uint32_t) * bitset_size);
			header.constant_track_data_offset = align_to(header.constant_tracks_bitset_offset + (sizeof(uint32_t) * bitset_size), 4);	// Aligned to 4 bytes
			header.clip_range_data_offset = align_to(header.constant_track_data_offset + constant_data_size, 4);						// Aligned to 4 bytes

			uint16_t segment_headers_start_offset = header.clip_range_data_offset + clip_range_data_size;
			impl::write_segment_headers(clip_context, settings, header.get_segment_headers(), segment_headers_start_offset);
			write_default_track_bitset(clip_context, header.get_default_tracks_bitset(), bitset_size);
			write_constant_track_bitset(clip_context, header.get_constant_tracks_bitset(), bitset_size);

			if (constant_data_size > 0)
				write_constant_track_data(clip_context, header.get_constant_track_data(), constant_data_size);
			else
				header.constant_track_data_offset = InvalidPtrOffset();

			if (settings.range_reduction != RangeReductionFlags8::None)
				write_clip_range_data(clip_segment, settings.range_reduction, header.get_clip_range_data(), clip_range_data_size);
			else
				header.clip_range_data_offset = InvalidPtrOffset();

			write_segment_data(clip_context, settings, header);

			finalize_compressed_clip(*compressed_clip);

			destroy_clip_context(allocator, clip_context);
			destroy_clip_context(allocator, raw_clip_context);

			return compressed_clip;
		}

		inline void print_stats(const CompressedClip& clip, std::FILE* file, const CompressionSettings& settings)
		{
			using namespace impl;

			const ClipHeader& header = get_clip_header(clip);

			uint32_t num_tracks = header.num_bones * Constants::NUM_TRACKS_PER_BONE;
			uint32_t bitset_size = get_bitset_size(num_tracks);

			uint32_t num_default_tracks = bitset_count_set_bits(header.get_default_tracks_bitset(), bitset_size);
			uint32_t num_constant_tracks = bitset_count_set_bits(header.get_constant_tracks_bitset(), bitset_size);
			uint32_t num_animated_tracks = num_tracks - num_default_tracks - num_constant_tracks;

			fprintf(file, "Clip rotation format: %s\n", get_rotation_format_name(header.rotation_format));
			fprintf(file, "Clip translation format: %s\n", get_vector_format_name(header.translation_format));
			fprintf(file, "Clip clip range reduction: %s\n", get_range_reduction_name(header.clip_range_reduction));
			fprintf(file, "Clip segment range reduction: %s\n", get_range_reduction_name(header.segment_range_reduction));
			fprintf(file, "Clip num default tracks: %u\n", num_default_tracks);
			fprintf(file, "Clip num constant tracks: %u\n", num_constant_tracks);
			fprintf(file, "Clip num animated tracks: %u\n", num_animated_tracks);
			fprintf(file, "Clip num segments: %u\n", header.num_segments);

			if (settings.segmenting.enabled)
			{
				fprintf(file, "Clip segmenting ideal num samples: %u\n", settings.segmenting.ideal_num_samples);
				fprintf(file, "Clip segmenting max num samples: %u\n", settings.segmenting.max_num_samples);
			}
		}
	}
}
