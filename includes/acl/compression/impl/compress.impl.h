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

// Included only once from compress.h

#include "acl/core/buffer_tag.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
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
#include "acl/compression/impl/write_track_metadata.h"

#include <cstdint>

namespace acl
{
	namespace acl_impl
	{
		inline error_result compress_scalar_track_list(iallocator& allocator, const track_array& track_list, const compression_settings& settings, compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
		{
#if defined(SJSON_CPP_WRITER)
			scope_profiler compression_time;
#endif

			track_list_context context;
			if (!initialize_context(allocator, track_list, context))
				return error_result("Some samples are not finite");

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
			buffer_size += sizeof(raw_buffer_header);								// Header
			buffer_size += sizeof(tracks_header);									// Header
			buffer_size += sizeof(scalar_tracks_header);							// Header
			ACL_ASSERT(is_aligned_to(buffer_size, alignof(track_metadata)), "Invalid alignment");
			buffer_size += per_track_metadata_size;									// Per track metadata
			buffer_size = align_to(buffer_size, 4);									// Align constant values
			buffer_size += constant_values_size;									// Constant values
			ACL_ASSERT(is_aligned_to(buffer_size, 4), "Invalid alignment");
			buffer_size += range_values_size;										// Range values
			ACL_ASSERT(is_aligned_to(buffer_size, 4), "Invalid alignment");
			buffer_size += animated_values_size;									// Animated values

			// Optional metadata
			const uint32_t metadata_start_offset = align_to(buffer_size, 4);
			const uint32_t metadata_track_list_name_size = settings.include_track_list_name ? write_track_list_name(track_list, nullptr) : 0;
			const uint32_t metadata_track_names_size = settings.include_track_names ? write_track_names(track_list, context.track_output_indices, context.num_output_tracks, nullptr) : 0;

			uint32_t metadata_size = 0;
			metadata_size += metadata_track_list_name_size;
			metadata_size = align_to(metadata_size, 4);
			metadata_size += metadata_track_names_size;

			if (metadata_size != 0)
			{
				metadata_size = align_to(metadata_size, 4);
				metadata_size += sizeof(optional_metadata_header);

				buffer_size = align_to(buffer_size, 4);
				buffer_size += metadata_size;
			}
			else
				buffer_size += 15;	// Ensure we have sufficient padding for unaligned 16 byte loads

			uint8_t* buffer = allocate_type_array_aligned<uint8_t>(allocator, buffer_size, alignof(compressed_tracks));
			std::memset(buffer, 0, buffer_size);

			uint8_t* buffer_start = buffer;
			out_compressed_tracks = reinterpret_cast<compressed_tracks*>(buffer);

			raw_buffer_header* buffer_header = safe_ptr_cast<raw_buffer_header>(buffer);
			buffer += sizeof(raw_buffer_header);

			tracks_header* header = safe_ptr_cast<tracks_header>(buffer);
			buffer += sizeof(tracks_header);

			// Write our primary header
			header->tag = static_cast<uint32_t>(buffer_tag32::compressed_tracks);
			header->version = compressed_tracks_version16::latest;
			header->algorithm_type = algorithm_type8::uniformly_sampled;
			header->track_type = track_list.get_track_type();
			header->num_tracks = context.num_tracks;
			header->num_samples = context.num_samples;
			header->sample_rate = context.sample_rate;
			header->set_has_metadata(metadata_size != 0);

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

			if (metadata_size != 0)
				buffer = align_to(buffer, 4) + metadata_size;
			else
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

			// Optional metadata header is last
			uint32_t writter_metadata_track_list_name_size = 0;
			uint32_t written_metadata_track_names_size = 0;
			if (metadata_size != 0)
			{
				optional_metadata_header* metadada_header = reinterpret_cast<optional_metadata_header*>(buffer_start + buffer_size - sizeof(optional_metadata_header));
				uint32_t metadata_offset = metadata_start_offset;

				if (settings.include_track_list_name)
				{
					metadada_header->track_list_name = metadata_offset;
					writter_metadata_track_list_name_size = write_track_list_name(track_list, metadada_header->get_track_list_name(*out_compressed_tracks));
					metadata_offset += writter_metadata_track_list_name_size;
				}
				else
					metadada_header->track_list_name = invalid_ptr_offset();

				if (settings.include_track_names)
				{
					metadata_offset = align_to(metadata_offset, 4);
					metadada_header->track_name_offsets = metadata_offset;
					written_metadata_track_names_size = write_track_names(track_list, context.track_output_indices, context.num_output_tracks, metadada_header->get_track_name_offsets(*out_compressed_tracks));
					metadata_offset += written_metadata_track_names_size;
				}
				else
					metadada_header->track_name_offsets = invalid_ptr_offset();
			}

			// Finish the raw buffer header
			buffer_header->size = buffer_size;
			buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(header), buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

#if defined(ACL_HAS_ASSERT_CHECKS)
			if (metadata_size == 0)
			{
				for (const uint8_t* padding = buffer - 15; padding < buffer; ++padding)
					ACL_ASSERT(*padding == 0, "Padding was overwritten");
			}
#endif

#if defined(SJSON_CPP_WRITER)
			compression_time.stop();

			if (out_stats.logging != stat_logging::None)
				write_compression_stats(context, *out_compressed_tracks, compression_time, out_stats);
#endif

			return error_result();
		}

		inline error_result compress_transform_track_list(iallocator& allocator, const track_array_qvvf& track_list, compression_settings settings, const track_array_qvvf* additive_base_track_list, additive_clip_format8 additive_format,
			compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
		{
			error_result result = settings.is_valid();
			if (result.any())
				return result;

#if defined(SJSON_CPP_WRITER)
			scope_profiler compression_time;
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

			// If we have no additive base, our additive format is always none
			if (additive_base_track_list == nullptr || additive_base_track_list->is_empty())
				additive_format = additive_clip_format8::none;

			clip_context raw_clip_context;
			if (!initialize_clip_context(allocator, track_list, additive_format, raw_clip_context))
				return error_result("Some samples are not finite");

			clip_context lossy_clip_context;
			initialize_clip_context(allocator, track_list, additive_format, lossy_clip_context);

			const bool is_additive = additive_format != additive_clip_format8::none;
			clip_context additive_base_clip_context;
			if (is_additive)
			{
				if (!initialize_clip_context(allocator, *additive_base_track_list, additive_format, additive_base_clip_context))
					return error_result("Some base samples are not finite");
			}

			convert_rotation_streams(allocator, lossy_clip_context, settings.rotation_format);

			// Extract our clip ranges now, we need it for compacting the constant streams
			extract_clip_bone_ranges(allocator, lossy_clip_context);

			// Compact and collapse the constant streams
			compact_constant_streams(allocator, lossy_clip_context, track_list);

			uint32_t clip_range_data_size = 0;
			if (range_reduction != range_reduction_flags8::none)
			{
				normalize_clip_streams(lossy_clip_context, range_reduction);
				clip_range_data_size = get_stream_range_data_size(lossy_clip_context, range_reduction, settings.rotation_format);
			}

			segment_streams(allocator, lossy_clip_context, settings.segmenting);

			if (lossy_clip_context.num_segments > uint32_t(std::numeric_limits<uint16_t>::max()))
				return error_result("Too many segments");

			// If we have a single segment, skip segment range reduction since it won't help
			if (range_reduction != range_reduction_flags8::none && lossy_clip_context.num_segments > 1)
			{
				extract_segment_bone_ranges(allocator, lossy_clip_context);
				normalize_segment_streams(lossy_clip_context, range_reduction);
			}

			quantize_streams(allocator, lossy_clip_context, settings, raw_clip_context, additive_base_clip_context, out_stats);

			uint32_t num_output_bones = 0;
			uint32_t* output_bone_mapping = create_output_track_mapping(allocator, track_list, num_output_bones);

			const uint32_t constant_data_size = get_constant_data_size(lossy_clip_context, output_bone_mapping, num_output_bones);

			calculate_animated_data_size(lossy_clip_context, output_bone_mapping, num_output_bones);

			const uint32_t format_per_track_data_size = get_format_per_track_data_size(lossy_clip_context, settings.rotation_format, settings.translation_format, settings.scale_format);

			const uint32_t num_tracks_per_bone = lossy_clip_context.has_scale ? 3 : 2;
			const uint32_t num_tracks = uint32_t(num_output_bones) * num_tracks_per_bone;
			const bitset_description bitset_desc = bitset_description::make_from_num_bits(num_tracks);

			// Adding an extra index at the end to delimit things, the index is always invalid: 0xFFFFFFFF
			const uint32_t segment_start_indices_size = lossy_clip_context.num_segments > 1 ? (sizeof(uint32_t) * (lossy_clip_context.num_segments + 1)) : 0;
			const uint32_t segment_headers_size = sizeof(segment_header) * lossy_clip_context.num_segments;

			uint32_t buffer_size = 0;
			// Per clip data
			buffer_size += sizeof(raw_buffer_header);							// Header
			buffer_size += sizeof(tracks_header);								// Header
			buffer_size += sizeof(transform_tracks_header);						// Header

			const uint32_t clip_header_size = buffer_size;

			buffer_size = align_to(buffer_size, 4);								// Align segment start indices
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

			if (are_all_enum_flags_set(out_stats.logging, stat_logging::Detailed))
			{
				constexpr uint32_t k_cache_line_byte_size = 64;
				lossy_clip_context.decomp_touched_bytes = clip_header_size + clip_data_size;
				lossy_clip_context.decomp_touched_bytes += sizeof(uint32_t) * 4;			// We touch at most 4 segment start indices
				lossy_clip_context.decomp_touched_bytes += sizeof(segment_header) * 2;	// We touch at most 2 segment headers
				lossy_clip_context.decomp_touched_cache_lines = align_to(clip_header_size, k_cache_line_byte_size) / k_cache_line_byte_size;
				lossy_clip_context.decomp_touched_cache_lines += align_to(clip_data_size, k_cache_line_byte_size) / k_cache_line_byte_size;
				lossy_clip_context.decomp_touched_cache_lines += 1;						// All 4 segment start indices should fit in a cache line
				lossy_clip_context.decomp_touched_cache_lines += 1;						// Both segment headers should fit in a cache line
			}

			// Per segment data
			for (SegmentContext& segment : lossy_clip_context.segment_iterator())
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

			// Optional metadata
			const uint32_t metadata_start_offset = align_to(buffer_size, 4);
			const uint32_t metadata_track_list_name_size = settings.include_track_list_name ? write_track_list_name(track_list, nullptr) : 0;
			const uint32_t metadata_track_names_size = settings.include_track_names ? write_track_names(track_list, output_bone_mapping, num_output_bones, nullptr) : 0;

			uint32_t metadata_size = 0;
			metadata_size += metadata_track_list_name_size;
			metadata_size = align_to(metadata_size, 4);
			metadata_size += metadata_track_names_size;

			if (metadata_size != 0)
			{
				metadata_size = align_to(metadata_size, 4);
				metadata_size += sizeof(optional_metadata_header);

				buffer_size = align_to(buffer_size, 4);
				buffer_size += metadata_size;
			}
			else
				buffer_size += 15;	// Ensure we have sufficient padding for unaligned 16 byte loads
				

			uint8_t* buffer = allocate_type_array_aligned<uint8_t>(allocator, buffer_size, alignof(compressed_tracks));
			std::memset(buffer, 0, buffer_size);

			uint8_t* buffer_start = buffer;
			out_compressed_tracks = reinterpret_cast<compressed_tracks*>(buffer);

			raw_buffer_header* buffer_header = safe_ptr_cast<raw_buffer_header>(buffer);
			buffer += sizeof(raw_buffer_header);

			tracks_header* header = safe_ptr_cast<tracks_header>(buffer);
			buffer += sizeof(tracks_header);

			// Write our primary header
			header->tag = static_cast<uint32_t>(buffer_tag32::compressed_tracks);
			header->version = compressed_tracks_version16::latest;
			header->algorithm_type = algorithm_type8::uniformly_sampled;
			header->track_type = track_list.get_track_type();
			header->num_tracks = num_output_bones;
			header->num_samples = track_list.get_num_samples_per_track();
			header->sample_rate = track_list.get_sample_rate();
			header->set_rotation_format(settings.rotation_format);
			header->set_translation_format(settings.translation_format);
			header->set_scale_format(settings.scale_format);
			header->set_has_scale(lossy_clip_context.has_scale);
			// Our default scale is 1.0 if we have no additive base or if we don't use 'additive1', otherwise it is 0.0
			header->set_default_scale(!is_additive || additive_format != additive_clip_format8::additive1 ? 1 : 0);
			header->set_has_metadata(metadata_size != 0);

			// Write our transform tracks header
			transform_tracks_header* transforms_header = safe_ptr_cast<transform_tracks_header>(buffer);
			buffer += sizeof(transform_tracks_header);

			transforms_header->num_segments = uint16_t(lossy_clip_context.num_segments);
			transforms_header->segment_start_indices_offset = align_to(sizeof(transform_tracks_header), 4);	// Relative to the start of our header
			transforms_header->segment_headers_offset = align_to(transforms_header->segment_start_indices_offset + segment_start_indices_size, 4);
			transforms_header->default_tracks_bitset_offset = align_to(transforms_header->segment_headers_offset + segment_headers_size, 4);
			transforms_header->constant_tracks_bitset_offset = transforms_header->default_tracks_bitset_offset + bitset_desc.get_num_bytes();
			transforms_header->constant_track_data_offset = align_to(transforms_header->constant_tracks_bitset_offset + bitset_desc.get_num_bytes(), 4);
			transforms_header->clip_range_data_offset = align_to(transforms_header->constant_track_data_offset + constant_data_size, 4);

			uint32_t written_segment_start_indices_size = 0;
			if (lossy_clip_context.num_segments > 1)
				written_segment_start_indices_size = write_segment_start_indices(lossy_clip_context, transforms_header->get_segment_start_indices());
			else
				transforms_header->segment_start_indices_offset = invalid_ptr_offset();

			const uint32_t segment_data_start_offset = transforms_header->clip_range_data_offset + clip_range_data_size;
			const uint32_t written_segment_headers_size = write_segment_headers(lossy_clip_context, settings, transforms_header->get_segment_headers(), segment_data_start_offset);

			uint32_t written_bitset_size = 0;
			written_bitset_size += write_default_track_bitset(lossy_clip_context, transforms_header->get_default_tracks_bitset(), bitset_desc, output_bone_mapping, num_output_bones);
			written_bitset_size += write_constant_track_bitset(lossy_clip_context, transforms_header->get_constant_tracks_bitset(), bitset_desc, output_bone_mapping, num_output_bones);

			uint32_t written_constant_data_size = 0;
			if (constant_data_size > 0)
				written_constant_data_size = write_constant_track_data(lossy_clip_context, transforms_header->get_constant_track_data(), constant_data_size, output_bone_mapping, num_output_bones);
			else
				transforms_header->constant_track_data_offset = invalid_ptr_offset();

			uint32_t written_clip_range_data_size = 0;
			if (range_reduction != range_reduction_flags8::none)
				written_clip_range_data_size = write_clip_range_data(lossy_clip_context, range_reduction, transforms_header->get_clip_range_data(), clip_range_data_size, output_bone_mapping, num_output_bones);
			else
				transforms_header->clip_range_data_offset = invalid_ptr_offset();

			const uint32_t written_segment_data_size = write_segment_data(lossy_clip_context, settings, range_reduction, *transforms_header, output_bone_mapping, num_output_bones);

			// Optional metadata header is last
			uint32_t writter_metadata_track_list_name_size = 0;
			uint32_t written_metadata_track_names_size = 0;
			if (metadata_size != 0)
			{
				optional_metadata_header* metadada_header = reinterpret_cast<optional_metadata_header*>(buffer_start + buffer_size - sizeof(optional_metadata_header));
				uint32_t metadata_offset = metadata_start_offset;

				if (settings.include_track_list_name)
				{
					metadada_header->track_list_name = metadata_offset;
					writter_metadata_track_list_name_size = write_track_list_name(track_list, metadada_header->get_track_list_name(*out_compressed_tracks));
					metadata_offset += writter_metadata_track_list_name_size;
				}
				else
					metadada_header->track_list_name = invalid_ptr_offset();

				if (settings.include_track_names)
				{
					metadata_offset = align_to(metadata_offset, 4);
					metadada_header->track_name_offsets = metadata_offset;
					written_metadata_track_names_size = write_track_names(track_list, output_bone_mapping, num_output_bones, metadada_header->get_track_name_offsets(*out_compressed_tracks));
					metadata_offset += written_metadata_track_names_size;
				}
				else
					metadada_header->track_name_offsets = invalid_ptr_offset();
			}

#if defined(ACL_HAS_ASSERT_CHECKS)
			{
				// Make sure we wrote the right amount of data
				buffer = align_to(buffer, 4);								// Align segment start indices
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

				if (metadata_size != 0)
				{
					buffer = align_to(buffer, 4);
					buffer += metadata_size;
				}
				else
					buffer += 15;	// Ensure we have sufficient padding for unaligned 16 byte loads

				(void)buffer_start;	// Avoid VS2017 bug, it falsely reports this variable as unused even when asserts are enabled
				ACL_ASSERT(written_segment_start_indices_size == segment_start_indices_size, "Wrote too little or too much data");
				ACL_ASSERT(written_segment_headers_size == segment_headers_size, "Wrote too little or too much data");
				ACL_ASSERT(written_bitset_size == (bitset_desc.get_num_bytes() * 2), "Wrote too little or too much data");
				ACL_ASSERT(written_constant_data_size == constant_data_size, "Wrote too little or too much data");
				ACL_ASSERT(written_clip_range_data_size == clip_range_data_size, "Wrote too little or too much data");
				ACL_ASSERT(writter_metadata_track_list_name_size == metadata_track_list_name_size, "Wrote too little or too much data");
				ACL_ASSERT(written_metadata_track_names_size == metadata_track_names_size, "Wrote too little or too much data");
				ACL_ASSERT(uint32_t(buffer - buffer_start) == buffer_size, "Wrote too little or too much data");

				if (metadata_size == 0)
				{
					for (const uint8_t* padding = buffer - 15; padding < buffer; ++padding)
						ACL_ASSERT(*padding == 0, "Padding was overwritten");
				}
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

			if (out_stats.logging != stat_logging::None)
				write_stats(allocator, track_list, lossy_clip_context, *out_compressed_tracks, settings, raw_clip_context, additive_base_clip_context, compression_time, out_stats);
#endif

			deallocate_type_array(allocator, output_bone_mapping, num_output_bones);
			destroy_clip_context(allocator, lossy_clip_context);
			destroy_clip_context(allocator, raw_clip_context);

			if (is_additive)
				destroy_clip_context(allocator, additive_base_clip_context);

			return error_result();
		}
	}

	inline error_result compress_track_list(iallocator& allocator, const track_array& track_list, const compression_settings& settings, compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
	{
		using namespace acl_impl;

		error_result result = track_list.is_valid();
		if (result.any())
			return result;

		if (track_list.get_num_samples_per_track() > 0xFFFF)
			return error_result("ACL only supports up to 65535 samples");

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		if (track_list.get_track_category() == track_category8::transformf)
			result = compress_transform_track_list(allocator, track_array_cast<track_array_qvvf>(track_list), settings, nullptr, additive_clip_format8::none, out_compressed_tracks, out_stats);
		else
			result = compress_scalar_track_list(allocator, track_list, settings, out_compressed_tracks, out_stats);

		return result;
	}

	inline error_result compress_track_list(iallocator& allocator, const track_array_qvvf& track_list, const compression_settings& settings, const track_array_qvvf& additive_base_track_list, additive_clip_format8 additive_format, compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
	{
		using namespace acl_impl;

		error_result result = track_list.is_valid();
		if (result.any())
			return result;

		if (additive_format != additive_clip_format8::none)
		{
			result = additive_base_track_list.is_valid();
			if (result.any())
				return result;
		}

		if (track_list.get_num_samples_per_track() > 0xFFFF)
			return error_result("ACL only supports up to 65535 samples");

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		return compress_transform_track_list(allocator, track_list, settings, &additive_base_track_list, additive_format, out_compressed_tracks, out_stats);
	}
}
