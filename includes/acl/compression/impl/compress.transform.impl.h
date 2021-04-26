#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2021 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/compression/impl/track_stream.h"
#include "acl/compression/impl/convert_rotation_streams.h"
#include "acl/compression/impl/compact_constant_streams.h"
#include "acl/compression/impl/normalize_streams.h"
#include "acl/compression/impl/quantize_streams.h"
#include "acl/compression/impl/segment_streams.h"
#include "acl/compression/impl/write_segment_data.h"
#include "acl/compression/impl/write_stats.h"
#include "acl/compression/impl/write_stream_data.h"
#include "acl/compression/impl/write_sub_track_types.h"
#include "acl/compression/impl/write_track_metadata.h"

#include <cstdint>

namespace acl
{
	namespace acl_impl
	{
		inline error_result compress_transform_track_list(iallocator& allocator, const track_array_qvvf& track_list, compression_settings settings,
			const track_array_qvvf* additive_base_track_list, additive_clip_format8 additive_format,
			compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
		{
			error_result result = settings.is_valid();
			if (result.any())
				return result;

#if defined(SJSON_CPP_WRITER)
			scope_profiler compression_time;
#endif

			// Segmenting settings are an implementation detail
			compression_segmenting_settings segmenting_settings;

			// If we enable database support, include the metadata we need
			if (settings.enable_database_support)
				settings.metadata.include_contributing_error = true;

			// If every track is retains full precision, we disable segmenting since it provides no benefit
			if (!is_rotation_format_variable(settings.rotation_format) && !is_vector_format_variable(settings.translation_format) && !is_vector_format_variable(settings.scale_format))
			{
				if (settings.metadata.include_contributing_error)
					return error_result("Raw tracks have no contributing error");

				segmenting_settings.ideal_num_samples = 0xFFFFFFFF;
				segmenting_settings.max_num_samples = 0xFFFFFFFF;
			}

			if (settings.metadata.include_contributing_error && segmenting_settings.max_num_samples > 32)
				return error_result("Cannot have more than 32 samples per segment when calculating the contributing error per frame");

			// If we want the optional track descriptions, make sure to include the parent track indices
			if (settings.metadata.include_track_descriptions)
				settings.metadata.include_parent_track_indices = true;

			ACL_ASSERT(settings.is_valid().empty(), "Invalid compression settings");
			ACL_ASSERT(segmenting_settings.is_valid().empty(), "Invalid segmenting settings");

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
			if (!initialize_clip_context(allocator, track_list, settings, additive_format, raw_clip_context))
				return error_result("Some samples are not finite");

			clip_context lossy_clip_context;
			initialize_clip_context(allocator, track_list, settings, additive_format, lossy_clip_context);

			const bool is_additive = additive_format != additive_clip_format8::none;
			clip_context additive_base_clip_context;
			if (is_additive && !initialize_clip_context(allocator, *additive_base_track_list, settings, additive_format, additive_base_clip_context))
				return error_result("Some base samples are not finite");

			// Convert our rotations if we need to
			convert_rotation_streams(allocator, lossy_clip_context, settings.rotation_format);

			// Extract our clip ranges now, we need it for compacting the constant streams
			extract_clip_bone_ranges(allocator, lossy_clip_context);

			// Compact and collapse the constant streams
			compact_constant_streams(allocator, lossy_clip_context, track_list, settings);

			uint32_t clip_range_data_size = 0;
			if (range_reduction != range_reduction_flags8::none)
			{
				// Normalize our samples into the clip wide ranges per bone
				normalize_clip_streams(lossy_clip_context, range_reduction);
				clip_range_data_size = get_clip_range_data_size(lossy_clip_context, range_reduction, settings.rotation_format);
			}

			segment_streams(allocator, lossy_clip_context, segmenting_settings);

			// If we have a single segment, skip segment range reduction since it won't help
			if (range_reduction != range_reduction_flags8::none && lossy_clip_context.num_segments > 1)
			{
				// Extract and fixup our segment wide ranges per bone
				extract_segment_bone_ranges(allocator, lossy_clip_context);

				// Normalize our samples into the segment wide ranges per bone
				normalize_segment_streams(lossy_clip_context, range_reduction);
			}

			quantize_streams(allocator, lossy_clip_context, settings, raw_clip_context, additive_base_clip_context, out_stats);

			uint32_t num_output_bones = 0;
			uint32_t* output_bone_mapping = create_output_track_mapping(allocator, track_list, num_output_bones);

			const uint32_t constant_data_size = get_constant_data_size(lossy_clip_context);

			calculate_animated_data_size(lossy_clip_context, output_bone_mapping, num_output_bones);

			uint32_t num_animated_variable_sub_tracks_padded = 0;
			const uint32_t format_per_track_data_size = get_format_per_track_data_size(lossy_clip_context, settings.rotation_format, settings.translation_format, settings.scale_format, &num_animated_variable_sub_tracks_padded);

			const uint32_t num_sub_tracks_per_bone = lossy_clip_context.has_scale ? 3 : 2;

			// Calculate how many sub-track packed entries we have
			// Each sub-track is 2 bits packed within a 32 bit entry
			// For each sub-track type, we round up to simplify bookkeeping
			// For example, if we have 3 tracks made up of rotation/translation we'll have one entry for each with unused padding
			// All rotation types come first, followed by all translation types, and with scale types at the end when present
			const uint32_t num_sub_track_entries = ((num_output_bones + k_num_sub_tracks_per_packed_entry - 1) / k_num_sub_tracks_per_packed_entry) * num_sub_tracks_per_bone;
			const uint32_t packed_sub_track_buffer_size = num_sub_track_entries * sizeof(packed_sub_track_types);

			// Adding an extra index at the end to delimit things, the index is always invalid: 0xFFFFFFFF
			const uint32_t segment_start_indices_size = lossy_clip_context.num_segments > 1 ? (uint32_t(sizeof(uint32_t)) * (lossy_clip_context.num_segments + 1)) : 0;
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
			buffer_size = align_to(buffer_size, 4);								// Align sub-track types

			const uint32_t clip_segment_header_size = buffer_size - clip_header_size;

			buffer_size += packed_sub_track_buffer_size;						// Packed sub-track types sorted by type
			buffer_size = align_to(buffer_size, 4);								// Align constant track data
			buffer_size += constant_data_size;									// Constant track data
			buffer_size = align_to(buffer_size, 4);								// Align range data
			buffer_size += clip_range_data_size;								// Range data

			const uint32_t clip_data_size = buffer_size - clip_segment_header_size - clip_header_size;

			if (are_all_enum_flags_set(out_stats.logging, stat_logging::detailed))
			{
				constexpr uint32_t k_cache_line_byte_size = 64;
				lossy_clip_context.decomp_touched_bytes = clip_header_size + clip_data_size;
				lossy_clip_context.decomp_touched_bytes += sizeof(uint32_t) * 4;		// We touch at most 4 segment start indices
				lossy_clip_context.decomp_touched_bytes += sizeof(segment_header) * 2;	// We touch at most 2 segment headers
				lossy_clip_context.decomp_touched_cache_lines = align_to(clip_header_size, k_cache_line_byte_size) / k_cache_line_byte_size;
				lossy_clip_context.decomp_touched_cache_lines += align_to(clip_data_size, k_cache_line_byte_size) / k_cache_line_byte_size;
				lossy_clip_context.decomp_touched_cache_lines += 1;						// All 4 segment start indices should fit in a cache line
				lossy_clip_context.decomp_touched_cache_lines += 1;						// Both segment headers should fit in a cache line
			}

			// Per segment data
			for (segment_context& segment : lossy_clip_context.segment_iterator())
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

				segment.segment_data_size = buffer_size - header_start;
				segment.total_header_size = header_end - header_start;
			}

			const uint32_t segment_data_size = buffer_size - clip_data_size - clip_segment_header_size - clip_header_size;

			// Optional metadata
			const uint32_t metadata_start_offset = align_to(buffer_size, 4);
			const uint32_t metadata_track_list_name_size = settings.metadata.include_track_list_name ? write_track_list_name(track_list, nullptr) : 0;
			const uint32_t metadata_track_names_size = settings.metadata.include_track_names ? write_track_names(track_list, output_bone_mapping, num_output_bones, nullptr) : 0;
			const uint32_t metadata_parent_track_indices_size = settings.metadata.include_parent_track_indices ? write_parent_track_indices(track_list, output_bone_mapping, num_output_bones, nullptr) : 0;
			const uint32_t metadata_track_descriptions_size = settings.metadata.include_track_descriptions ? write_track_descriptions(track_list, output_bone_mapping, num_output_bones, nullptr) : 0;
			const uint32_t metadata_contributing_error_size = settings.metadata.include_contributing_error ? write_contributing_error(lossy_clip_context, nullptr) : 0;

			uint32_t metadata_size = 0;
			metadata_size += metadata_track_list_name_size;
			metadata_size = align_to(metadata_size, 4);
			metadata_size += metadata_track_names_size;
			metadata_size = align_to(metadata_size, 4);
			metadata_size += metadata_parent_track_indices_size;
			metadata_size = align_to(metadata_size, 4);
			metadata_size += metadata_track_descriptions_size;
			metadata_size = align_to(metadata_size, 4);
			metadata_size += metadata_contributing_error_size;

			if (metadata_size != 0)
			{
				buffer_size = align_to(buffer_size, 4);
				buffer_size += metadata_size;

				buffer_size = align_to(buffer_size, 4);
				buffer_size += sizeof(optional_metadata_header);
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
			header->num_samples = num_output_bones != 0 ? track_list.get_num_samples_per_track() : 0;
			header->sample_rate = num_output_bones != 0 ? track_list.get_sample_rate() : 0.0F;
			header->set_rotation_format(settings.rotation_format);
			header->set_translation_format(settings.translation_format);
			header->set_scale_format(settings.scale_format);
			header->set_has_scale(lossy_clip_context.has_scale);
			// Our default scale is 1.0 if we have no additive base or if we don't use 'additive1', otherwise it is 0.0
			header->set_default_scale(!is_additive || additive_format != additive_clip_format8::additive1 ? 1 : 0);
			header->set_has_database(false);
			header->set_has_metadata(metadata_size != 0);

			// Write our transform tracks header
			transform_tracks_header* transforms_header = safe_ptr_cast<transform_tracks_header>(buffer);
			buffer += sizeof(transform_tracks_header);

			transforms_header->num_segments = lossy_clip_context.num_segments;
			transforms_header->num_animated_variable_sub_tracks = num_animated_variable_sub_tracks_padded;
			get_num_constant_samples(lossy_clip_context, transforms_header->num_constant_rotation_samples, transforms_header->num_constant_translation_samples, transforms_header->num_constant_scale_samples);
			get_num_animated_sub_tracks(lossy_clip_context.segments[0],
				transforms_header->num_animated_rotation_sub_tracks, transforms_header->num_animated_translation_sub_tracks, transforms_header->num_animated_scale_sub_tracks);

			const uint32_t segment_start_indices_offset = align_to<uint32_t>(sizeof(transform_tracks_header), 4);	// Relative to the start of our transform_tracks_header
			transforms_header->database_header_offset = invalid_ptr_offset();
			transforms_header->segment_headers_offset = align_to(segment_start_indices_offset + segment_start_indices_size, 4);
			transforms_header->sub_track_types_offset = align_to(transforms_header->segment_headers_offset + segment_headers_size, 4);
			transforms_header->constant_track_data_offset = align_to(transforms_header->sub_track_types_offset + packed_sub_track_buffer_size, 4);
			transforms_header->clip_range_data_offset = align_to(transforms_header->constant_track_data_offset + constant_data_size, 4);

			uint32_t written_segment_start_indices_size = 0;
			if (lossy_clip_context.num_segments > 1)
				written_segment_start_indices_size = write_segment_start_indices(lossy_clip_context, transforms_header->get_segment_start_indices());

			const uint32_t segment_data_start_offset = transforms_header->clip_range_data_offset + clip_range_data_size;
			const uint32_t written_segment_headers_size = write_segment_headers(lossy_clip_context, settings, transforms_header->get_segment_headers(), segment_data_start_offset);

			uint32_t written_sub_track_buffer_size = 0;
			written_sub_track_buffer_size += write_packed_sub_track_types(lossy_clip_context, transforms_header->get_sub_track_types(), output_bone_mapping, num_output_bones);

			uint32_t written_constant_data_size = 0;
			if (constant_data_size != 0)
				written_constant_data_size = write_constant_track_data(lossy_clip_context, settings.rotation_format, transforms_header->get_constant_track_data(), constant_data_size, output_bone_mapping, num_output_bones);

			uint32_t written_clip_range_data_size = 0;
			if (range_reduction != range_reduction_flags8::none)
				written_clip_range_data_size = write_clip_range_data(lossy_clip_context, range_reduction, transforms_header->get_clip_range_data(), clip_range_data_size, output_bone_mapping, num_output_bones);

			const uint32_t written_segment_data_size = write_segment_data(lossy_clip_context, settings, range_reduction, transforms_header->get_segment_headers(), *transforms_header, output_bone_mapping, num_output_bones);

			// Optional metadata header is last
			uint32_t writter_metadata_track_list_name_size = 0;
			uint32_t written_metadata_track_names_size = 0;
			uint32_t written_metadata_parent_track_indices_size = 0;
			uint32_t written_metadata_track_descriptions_size = 0;
			uint32_t written_metadata_contributing_error_size = 0;
			if (metadata_size != 0)
			{
				optional_metadata_header* metadata_header = reinterpret_cast<optional_metadata_header*>(buffer_start + buffer_size - sizeof(optional_metadata_header));
				uint32_t metadata_offset = metadata_start_offset;	// Relative to the start of our compressed_tracks

				if (settings.metadata.include_track_list_name)
				{
					metadata_header->track_list_name = metadata_offset;
					writter_metadata_track_list_name_size = write_track_list_name(track_list, metadata_header->get_track_list_name(*out_compressed_tracks));
					metadata_offset += writter_metadata_track_list_name_size;
				}
				else
					metadata_header->track_list_name = invalid_ptr_offset();

				if (settings.metadata.include_track_names)
				{
					metadata_offset = align_to(metadata_offset, 4);
					metadata_header->track_name_offsets = metadata_offset;
					written_metadata_track_names_size = write_track_names(track_list, output_bone_mapping, num_output_bones, metadata_header->get_track_name_offsets(*out_compressed_tracks));
					metadata_offset += written_metadata_track_names_size;
				}
				else
					metadata_header->track_name_offsets = invalid_ptr_offset();

				if (settings.metadata.include_parent_track_indices)
				{
					metadata_offset = align_to(metadata_offset, 4);
					metadata_header->parent_track_indices = metadata_offset;
					written_metadata_parent_track_indices_size = write_parent_track_indices(track_list, output_bone_mapping, num_output_bones, metadata_header->get_parent_track_indices(*out_compressed_tracks));
					metadata_offset += written_metadata_parent_track_indices_size;
				}
				else
					metadata_header->parent_track_indices = invalid_ptr_offset();

				if (settings.metadata.include_track_descriptions)
				{
					metadata_offset = align_to(metadata_offset, 4);
					metadata_header->track_descriptions = metadata_offset;
					written_metadata_track_descriptions_size = write_track_descriptions(track_list, output_bone_mapping, num_output_bones, metadata_header->get_track_descriptions(*out_compressed_tracks));
					metadata_offset += written_metadata_track_descriptions_size;
				}
				else
					metadata_header->track_descriptions = invalid_ptr_offset();

				if (settings.metadata.include_contributing_error)
				{
					metadata_offset = align_to(metadata_offset, 4);
					metadata_header->contributing_error = metadata_offset;
					written_metadata_contributing_error_size = write_contributing_error(lossy_clip_context, metadata_header->get_contributing_error(*out_compressed_tracks));
					metadata_offset += written_metadata_contributing_error_size;
				}
				else
					metadata_header->contributing_error = invalid_ptr_offset();
			}

			// Finish the compressed tracks raw buffer header
			buffer_header->size = buffer_size;
			buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(header), buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

#if defined(ACL_HAS_ASSERT_CHECKS)
			{
				// Make sure we wrote the right amount of data
				buffer = align_to(buffer, 4);								// Align segment start indices
				buffer += written_segment_start_indices_size;
				buffer = align_to(buffer, 4);								// Align segment headers
				buffer += written_segment_headers_size;
				buffer = align_to(buffer, 4);								// Align sub-track types
				buffer += written_sub_track_buffer_size;
				buffer = align_to(buffer, 4);								// Align constant track data
				buffer += written_constant_data_size;
				buffer = align_to(buffer, 4);								// Align range data
				buffer += written_clip_range_data_size;
				buffer += written_segment_data_size;

				if (metadata_size != 0)
				{
					buffer = align_to(buffer, 4);
					buffer += metadata_size;

					buffer = align_to(buffer, 4);
					buffer += sizeof(optional_metadata_header);
				}
				else
					buffer += 15;	// Ensure we have sufficient padding for unaligned 16 byte loads

				(void)buffer_start;	// Avoid VS2017 bug, it falsely reports this variable as unused even when asserts are enabled
				ACL_ASSERT(written_segment_start_indices_size == segment_start_indices_size, "Wrote too little or too much data");
				ACL_ASSERT(written_segment_headers_size == segment_headers_size, "Wrote too little or too much data");
				ACL_ASSERT(written_segment_data_size == segment_data_size, "Wrote too little or too much data");
				ACL_ASSERT(written_sub_track_buffer_size == packed_sub_track_buffer_size, "Wrote too little or too much data");
				ACL_ASSERT(written_constant_data_size == constant_data_size, "Wrote too little or too much data");
				ACL_ASSERT(written_clip_range_data_size == clip_range_data_size, "Wrote too little or too much data");
				ACL_ASSERT(writter_metadata_track_list_name_size == metadata_track_list_name_size, "Wrote too little or too much data");
				ACL_ASSERT(written_metadata_track_names_size == metadata_track_names_size, "Wrote too little or too much data");
				ACL_ASSERT(written_metadata_parent_track_indices_size == metadata_parent_track_indices_size, "Wrote too little or too much data");
				ACL_ASSERT(written_metadata_track_descriptions_size == metadata_track_descriptions_size, "Wrote too little or too much data");
				ACL_ASSERT(written_metadata_contributing_error_size == metadata_contributing_error_size, "Wrote too little or too much data");
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
			(void)written_sub_track_buffer_size;
			(void)written_constant_data_size;
			(void)written_clip_range_data_size;
			(void)written_segment_data_size;
			(void)segment_data_size;
			(void)buffer_start;
#endif

#if defined(SJSON_CPP_WRITER)
			compression_time.stop();

			if (out_stats.logging != stat_logging::none)
				write_stats(allocator, track_list, lossy_clip_context, *out_compressed_tracks, settings, segmenting_settings, range_reduction, raw_clip_context, additive_base_clip_context, compression_time, out_stats);
#endif

			deallocate_type_array(allocator, output_bone_mapping, num_output_bones);
			destroy_clip_context(lossy_clip_context);
			destroy_clip_context(raw_clip_context);
			destroy_clip_context(additive_base_clip_context);

			return error_result();
		}
	}
}
