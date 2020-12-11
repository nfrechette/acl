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

#include "acl/core/iallocator.h"
#include "acl/core/compressed_database.h"
#include "acl/core/error_result.h"
#include "acl/core/hash.h"

#include <cstdint>

namespace acl
{
	namespace acl_impl
	{
		struct frame_tier_mapping
		{
			const uint8_t* animated_data;			// Pointer to the segment's animated data which contains this frame

			uint32_t tracks_index;
			uint32_t segment_index;
			uint32_t clip_frame_index;
			uint32_t segment_frame_index;
			uint32_t frame_bit_size;				// Size in bits of this frame

			float contributing_error;
		};

		struct database_tier_mapping
		{
			frame_tier_mapping* frames = nullptr;	// Frames mapped to this tier
			uint32_t num_frames = 0;				// Number of frames mapped to this tier

			database_tier8 tier;					// Actual tier

			bool is_empty() const { return num_frames == 0; }
		};

		struct segment_contriguting_error
		{
			const frame_contributing_error* errors;	// List of frame contributing errors for this segment, sorted lowest first
			uint32_t num_frames;					// Total number of frames in this segment
			uint32_t num_movable;					// Number of frames that can be moved
			uint32_t num_assigned;					// Number of frames already assigned
		};

		struct clip_contributing_error
		{
			segment_contriguting_error* segments;
			uint32_t num_segments;
			uint32_t num_frames;					// Number of frames
			uint32_t num_assigned;					// Number of frames already assigned
		};

		struct frame_assignment_context
		{
			iallocator& allocator;

			const compressed_tracks* const* compressed_tracks_list;
			uint32_t num_compressed_tracks;

			database_tier_mapping mappings[2];		// 0 = high importance, 1 = low importance
			uint32_t num_movable_frames;

			clip_contributing_error* contributing_error_per_clip;

			frame_assignment_context(iallocator& allocator_, const compressed_tracks* const* compressed_tracks_list_, uint32_t num_compressed_tracks_, uint32_t num_movable_frames_)
				: allocator(allocator_)
				, compressed_tracks_list(compressed_tracks_list_)
				, num_compressed_tracks(num_compressed_tracks_)
				, num_movable_frames(num_movable_frames_)
				, contributing_error_per_clip(allocate_type_array<clip_contributing_error>(allocator_, num_compressed_tracks_))
			{
				mappings[0].tier = database_tier8::high_importance;
				mappings[1].tier = database_tier8::low_importance;

				// Setup our error metadata to make iterating on it easier and track what has been assigned
				for (uint32_t list_index = 0; list_index < num_compressed_tracks_; ++list_index)
				{
					const compressed_tracks* tracks = compressed_tracks_list_[list_index];
					const tracks_header& header = get_tracks_header(*tracks);
					const transform_tracks_header& transform_header = get_transform_tracks_header(*tracks);
					const bool has_multiple_segments = transform_header.has_multiple_segments();
					const uint32_t* segment_start_indices = has_multiple_segments ? transform_header.get_segment_start_indices() : nullptr;
					const optional_metadata_header& metadata_header = get_optional_metadata_header(*tracks);
					const frame_contributing_error* contributing_errors = metadata_header.get_contributing_error(*tracks);

					clip_contributing_error& clip_error = contributing_error_per_clip[list_index];
					clip_error.segments = allocate_type_array<segment_contriguting_error>(allocator_, transform_header.num_segments);
					clip_error.num_segments = transform_header.num_segments;
					clip_error.num_frames = 0;
					clip_error.num_assigned = 0;

					for (uint32_t segment_index = 0; segment_index < transform_header.num_segments; ++segment_index)
					{
						const uint32_t segment_start_frame_index = has_multiple_segments ? segment_start_indices[segment_index] : 0;

						uint32_t num_segment_frames;
						if (transform_header.num_segments == 1)
							num_segment_frames = header.num_samples;	// Only one segment, it has every frame
						else if (segment_index + 1 == transform_header.num_segments)
							num_segment_frames = header.num_samples - segment_start_indices[segment_index];	// Last segment has the remaining frames
						else
							num_segment_frames = segment_start_indices[segment_index + 1] - segment_start_indices[segment_index];

						const uint32_t num_movable = num_segment_frames >= 2 ? (num_segment_frames - 2) : 0;

						segment_contriguting_error& segment_error = clip_error.segments[segment_index];
						segment_error.errors = contributing_errors + segment_start_frame_index;
						segment_error.num_frames = num_segment_frames;
						segment_error.num_movable = num_movable;
						segment_error.num_assigned = 0;

						clip_error.num_frames += num_segment_frames;
					}
				}
			}

			~frame_assignment_context()
			{
				deallocate_type_array(allocator, mappings[0].frames, mappings[0].num_frames);
				deallocate_type_array(allocator, mappings[1].frames, mappings[1].num_frames);

				for (uint32_t list_index = 0; list_index < num_compressed_tracks; ++list_index)
					deallocate_type_array(allocator, contributing_error_per_clip[list_index].segments, contributing_error_per_clip[list_index].num_segments);
				deallocate_type_array(allocator, contributing_error_per_clip, num_compressed_tracks);
			}

			database_tier_mapping& get_tier_mapping(database_tier8 tier) { return mappings[(uint32_t)tier]; }
			const database_tier_mapping& get_tier_mapping(database_tier8 tier) const { return mappings[(uint32_t)tier]; }

			void set_tier_num_frames(database_tier8 tier, uint32_t num_frames)
			{
				database_tier_mapping& mapping = mappings[(uint32_t)tier];
				ACL_ASSERT(mapping.frames == nullptr, "Tier already setup");

				mapping.frames = allocate_type_array<frame_tier_mapping>(allocator, num_frames);
				mapping.num_frames = num_frames;
			}
		};

		inline uint32_t calculate_num_frames(const compressed_tracks* const* compressed_tracks_list, uint32_t num_compressed_tracks)
		{
			uint32_t num_frames = 0;

			for (uint32_t list_index = 0; list_index < num_compressed_tracks; ++list_index)
			{
				const compressed_tracks* tracks = compressed_tracks_list[list_index];

				const tracks_header& header = get_tracks_header(*tracks);

				num_frames += header.num_samples;
			}

			return num_frames;
		}

		inline uint32_t calculate_num_movable_frames(const compressed_tracks* const* compressed_tracks_list, uint32_t num_compressed_tracks)
		{
			uint32_t num_movable_frames = 0;

			for (uint32_t list_index = 0; list_index < num_compressed_tracks; ++list_index)
			{
				const compressed_tracks* tracks = compressed_tracks_list[list_index];

				const tracks_header& header = get_tracks_header(*tracks);
				const transform_tracks_header& transform_header = get_transform_tracks_header(*tracks);

				// A frame is movable if it isn't the first or last frame of a segment
				// If we have more than 1 frame, we can remove 2 frames per segment
				// We know that the only way to get a segment with 1 frame is if the whole clip contains
				// a single frame and thus has one segment
				// If we have 0 or 1 frame, none are movable
				if (header.num_samples >= 2)
					num_movable_frames += header.num_samples - (transform_header.num_segments * 2);
			}

			return num_movable_frames;
		}

		inline uint32_t calculate_num_segments(const compressed_tracks* const* compressed_tracks_list, uint32_t num_compressed_tracks)
		{
			uint32_t num_segments = 0;

			for (uint32_t list_index = 0; list_index < num_compressed_tracks; ++list_index)
			{
				const compressed_tracks* tracks = compressed_tracks_list[list_index];
				const transform_tracks_header& transform_header = get_transform_tracks_header(*tracks);

				num_segments += transform_header.num_segments;
			}

			return num_segments;
		}

		inline void assign_frames_to_tier(frame_assignment_context& context, database_tier_mapping& tier_mapping)
		{
			// Iterate until we've fully assigned every frame we can to this tier
			for (uint32_t assigned_frame_count = 0; assigned_frame_count < tier_mapping.num_frames; ++assigned_frame_count)
			{
				frame_tier_mapping best_mapping{};
				best_mapping.contributing_error = std::numeric_limits<float>::infinity();

				// Iterate over every segment and find the one with the frame that has the lowest contributing error to assign to this tier
				for (uint32_t list_index = 0; list_index < context.num_compressed_tracks; ++list_index)
				{
					const compressed_tracks* tracks = context.compressed_tracks_list[list_index];
					const transform_tracks_header& transforms_header = get_transform_tracks_header(*tracks);
					const bool has_multiple_segments = transforms_header.has_multiple_segments();
					const uint32_t* segment_start_indices = has_multiple_segments ? transforms_header.get_segment_start_indices() : nullptr;
					const segment_header* segment_headers = transforms_header.get_segment_headers();

					const clip_contributing_error& clip_error = context.contributing_error_per_clip[list_index];

					for (uint32_t segment_index = 0; segment_index < transforms_header.num_segments; ++segment_index)
					{
						const segment_contriguting_error& segment_error = clip_error.segments[segment_index];

						// High importance frames can always be moved since they end up in our compressed tracks
						const uint32_t num_movable = tier_mapping.tier == database_tier8::high_importance ? segment_error.num_frames : segment_error.num_movable;
						if (segment_error.num_assigned >= num_movable)
							continue;	// No more movable data in this segment, skip it

						const frame_contributing_error& contributing_error = segment_error.errors[segment_error.num_assigned];
						ACL_ASSERT(tier_mapping.tier == database_tier8::high_importance || rtm::scalar_is_finite(contributing_error.error), "Error should be finite");

						if (contributing_error.error <= best_mapping.contributing_error)
						{
							// This frame has a lower error, use it
							const uint8_t* format_per_track_data;
							const uint8_t* range_data;
							const uint8_t* animated_data;
							transforms_header.get_segment_data(segment_headers[segment_index], format_per_track_data, range_data, animated_data);

							const uint32_t segment_start_frame_index = has_multiple_segments ? segment_start_indices[segment_index] : 0;

							best_mapping.animated_data = animated_data;
							best_mapping.tracks_index = list_index;
							best_mapping.segment_index = segment_index;
							best_mapping.frame_bit_size = segment_headers[segment_index].animated_pose_bit_size;
							best_mapping.clip_frame_index = segment_start_frame_index + contributing_error.index;
							best_mapping.segment_frame_index = contributing_error.index;
							best_mapping.contributing_error = contributing_error.error;
						}
					}
				}

				ACL_ASSERT(tier_mapping.tier == database_tier8::high_importance || rtm::scalar_is_finite(best_mapping.contributing_error), "Error should be finite");

				// Assigned our mapping
				tier_mapping.frames[assigned_frame_count] = best_mapping;

				// Mark it as being assigned so we don't try to use it again
				context.contributing_error_per_clip[best_mapping.tracks_index].segments[best_mapping.segment_index].num_assigned++;
				context.contributing_error_per_clip[best_mapping.tracks_index].num_assigned++;
			}

			// Once we have assigned every frame we could to this tier, sort them by clip, by segment, then by segment frame index
			auto sort_predicate = [](const frame_tier_mapping& lhs, const frame_tier_mapping& rhs)
			{
				// Sort by clip index first
				if (lhs.tracks_index < rhs.tracks_index)
					return true;
				else if (lhs.tracks_index == rhs.tracks_index)
				{
					// Sort by segment index second
					if (lhs.segment_index < rhs.segment_index)
						return true;
					else if (lhs.segment_index == rhs.segment_index)
					{
						// Sort by frame index third
						if (lhs.segment_frame_index < rhs.segment_frame_index)
							return true;
					}
				}

				return false;
			};

			std::sort(tier_mapping.frames, tier_mapping.frames + tier_mapping.num_frames, sort_predicate);
		}

		inline void assign_frames_to_tiers(frame_assignment_context& context)
		{
			// Assign frames to our lowest importance tier first
			assign_frames_to_tier(context, context.get_tier_mapping(database_tier8::low_importance));

			// Then our high importance tier
			assign_frames_to_tier(context, context.get_tier_mapping(database_tier8::high_importance));

			// Sanity check that everything has been assigned
#if defined(ACL_HAS_ASSERT_CHECKS)
			for (uint32_t list_index = 0; list_index < context.num_compressed_tracks; ++list_index)
			{
				const clip_contributing_error& clip_error = context.contributing_error_per_clip[list_index];
				ACL_ASSERT(clip_error.num_assigned == clip_error.num_frames, "Every frame should have been assigned");
			}
#endif
		}

		inline uint32_t find_first_metadata_offset(const optional_metadata_header& header)
		{
			if (header.track_list_name.is_valid())
				return (uint32_t)header.track_list_name;

			if (header.track_name_offsets.is_valid())
				return (uint32_t)header.track_name_offsets;

			if (header.parent_track_indices.is_valid())
				return (uint32_t)header.parent_track_indices;

			if (header.track_descriptions.is_valid())
				return (uint32_t)header.track_descriptions;

			if (header.contributing_error.is_valid())
				return (uint32_t)header.contributing_error;

			ACL_ASSERT(false, "Expected metadata to be present");
			return ~0U;
		}

		inline uint32_t get_metadata_track_list_name_size(const optional_metadata_header& header)
		{
			if (!header.track_list_name.is_valid())
				return 0;

			if (header.track_name_offsets.is_valid())
				return (uint32_t)header.track_name_offsets - (uint32_t)header.track_list_name;

			if (header.parent_track_indices.is_valid())
				return (uint32_t)header.parent_track_indices - (uint32_t)header.track_list_name;

			if (header.track_descriptions.is_valid())
				return (uint32_t)header.track_descriptions - (uint32_t)header.track_list_name;

			if (header.contributing_error.is_valid())
				return (uint32_t)header.contributing_error - (uint32_t)header.track_list_name;

			ACL_ASSERT(false, "Expected metadata to be present");
			return ~0U;
		}

		inline uint32_t get_metadata_track_names_size(const optional_metadata_header& header)
		{
			if (!header.track_name_offsets.is_valid())
				return 0;

			if (header.parent_track_indices.is_valid())
				return (uint32_t)header.parent_track_indices - (uint32_t)header.track_name_offsets;

			if (header.track_descriptions.is_valid())
				return (uint32_t)header.track_descriptions - (uint32_t)header.track_name_offsets;

			if (header.contributing_error.is_valid())
				return (uint32_t)header.contributing_error - (uint32_t)header.track_name_offsets;

			ACL_ASSERT(false, "Expected metadata to be present");
			return ~0U;
		}

		inline uint32_t get_metadata_parent_track_indices_size(const optional_metadata_header& header)
		{
			if (!header.parent_track_indices.is_valid())
				return 0;

			if (header.track_descriptions.is_valid())
				return (uint32_t)header.track_descriptions - (uint32_t)header.parent_track_indices;

			if (header.contributing_error.is_valid())
				return (uint32_t)header.contributing_error - (uint32_t)header.parent_track_indices;

			ACL_ASSERT(false, "Expected metadata to be present");
			return ~0U;
		}

		inline uint32_t get_metadata_track_descriptions_size(const optional_metadata_header& header)
		{
			if (!header.track_descriptions.is_valid())
				return 0;

			if (header.contributing_error.is_valid())
				return (uint32_t)header.contributing_error - (uint32_t)header.track_descriptions;

			ACL_ASSERT(false, "Expected metadata to be present");
			return ~0U;
		}

		inline uint32_t build_sample_indices(const database_tier_mapping& tier_mapping, uint32_t tracks_index, uint32_t segment_index)
		{
			// TODO: Binary search our first entry and bail out once done?

			const bitset_description desc = bitset_description::make_from_num_bits<32>();

			uint32_t sample_indices = 0;
			for (uint32_t frame_index = 0; frame_index < tier_mapping.num_frames; ++frame_index)
			{
				const frame_tier_mapping& frame = tier_mapping.frames[frame_index];
				if (frame.tracks_index != tracks_index)
					continue;	// This is not the tracks instance we care about

				if (frame.segment_index != segment_index)
					continue;	// This is not the segment we care about

				bitset_set(&sample_indices, desc, frame.segment_frame_index, true);
			}

			return sample_indices;
		}

		inline void write_segment_headers(const database_tier_mapping& tier_mapping, uint32_t tracks_index, const transform_tracks_header& input_transforms_header, const segment_header* headers, uint32_t segment_data_base_offset, segment_tier0_header* out_headers)
		{
			const bitset_description desc = bitset_description::make_from_num_bits<32>();

			uint32_t segment_data_offset = segment_data_base_offset;
			for (uint32_t segment_index = 0; segment_index < input_transforms_header.num_segments; ++segment_index)
			{
				const uint32_t animated_pose_bit_size = headers[segment_index].animated_pose_bit_size;

				out_headers[segment_index].animated_pose_bit_size = animated_pose_bit_size;
				out_headers[segment_index].segment_data = segment_data_offset;
				out_headers[segment_index].sample_indices = build_sample_indices(tier_mapping, tracks_index, segment_index);

				const uint8_t* format_per_track_data;
				const uint8_t* range_data;
				const uint8_t* animated_data;
				input_transforms_header.get_segment_data(headers[segment_index], format_per_track_data, range_data, animated_data);

				// Range data, whether present or not, follows the per track metadata, use it to calculate our size
				const uint32_t format_per_track_data_size = uint32_t(range_data - format_per_track_data);

				// Animated data follows the range data (present or not), use it to calculate our size
				const uint32_t range_data_size = uint32_t(animated_data - range_data);

				const uint32_t num_animated_frames = bitset_count_set_bits(&out_headers[segment_index].sample_indices, desc);
				const uint32_t animated_data_num_bits = num_animated_frames * animated_pose_bit_size;
				const uint32_t animated_data_size = (animated_data_num_bits + 7) / 8;

				segment_data_offset += format_per_track_data_size;				// Format per track data

				// TODO: Alignment only necessary with 16bit per component (segment constant tracks), need to fix scalar decoding path
				segment_data_offset = align_to(segment_data_offset, 2);			// Align range data
				segment_data_offset += range_data_size;							// Range data

				// TODO: Variable bit rate doesn't need alignment
				segment_data_offset = align_to(segment_data_offset, 4);			// Align animated data
				segment_data_offset += animated_data_size;						// Animated data
			}
		}

		inline void write_segment_data(const database_tier_mapping& tier_mapping, uint32_t tracks_index,
			const transform_tracks_header& input_transforms_header, const segment_header* input_headers,
			transform_tracks_header& output_transforms_header, const segment_tier0_header* output_headers)
		{
			for (uint32_t segment_index = 0; segment_index < input_transforms_header.num_segments; ++segment_index)
			{
				const uint8_t* input_format_per_track_data;
				const uint8_t* input_range_data;
				const uint8_t* input_animated_data;
				input_transforms_header.get_segment_data(input_headers[segment_index], input_format_per_track_data, input_range_data, input_animated_data);

				uint8_t* output_format_per_track_data;
				uint8_t* output_range_data;
				uint8_t* output_animated_data;
				output_transforms_header.get_segment_data(output_headers[segment_index], output_format_per_track_data, output_range_data, output_animated_data);

				// Range data, whether present or not, follows the per track metadata, use it to calculate our size
				const uint32_t format_per_track_data_size = uint32_t(input_range_data - input_format_per_track_data);

				// Animated data follows the range data (present or not), use it to calculate our size
				const uint32_t range_data_size = uint32_t(input_animated_data - input_range_data);

				// Copy our per track metadata, it does not change
				std::memcpy(output_format_per_track_data, input_format_per_track_data, format_per_track_data_size);

				// Copy our range data whether it is present or not, it does not change
				std::memcpy(output_range_data, input_range_data, range_data_size);

				// Populate our new animated data from our sorted frame mapping data
				uint64_t output_animated_bit_offset = 0;
				for (uint32_t frame_index = 0; frame_index < tier_mapping.num_frames; ++frame_index)
				{
					const frame_tier_mapping& frame = tier_mapping.frames[frame_index];
					if (frame.tracks_index != tracks_index)
						continue;	// This is not the tracks instance we care about

					if (frame.segment_index != segment_index)
						continue;	// This is not the segment we care about

					// Append this frame
					const uint64_t input_animated_bit_offset = frame.segment_frame_index * frame.frame_bit_size;
					memcpy_bits(output_animated_data, output_animated_bit_offset, input_animated_data, input_animated_bit_offset, frame.frame_bit_size);
					output_animated_bit_offset += frame.frame_bit_size;
				}
			}
		}

		inline void build_compressed_tracks(const frame_assignment_context& context, compressed_tracks** out_compressed_tracks)
		{
			const bitset_description desc = bitset_description::make_from_num_bits<32>();

			const database_tier_mapping& tier_mapping = context.get_tier_mapping(database_tier8::high_importance);
			uint32_t clip_header_offset = 0;

			for (uint32_t list_index = 0; list_index < context.num_compressed_tracks; ++list_index)
			{
				const compressed_tracks* input_tracks = context.compressed_tracks_list[list_index];
				const clip_contributing_error& clip_error = context.contributing_error_per_clip[list_index];

				const tracks_header& input_header = get_tracks_header(*input_tracks);
				const transform_tracks_header& input_transforms_header = get_transform_tracks_header(*input_tracks);
				const segment_header* input_segment_headers = input_transforms_header.get_segment_headers();
				const optional_metadata_header& input_metadata_header = get_optional_metadata_header(*input_tracks);

				const uint32_t num_sub_tracks_per_bone = input_header.get_has_scale() ? 3 : 2;
				const uint32_t num_sub_tracks = input_header.num_tracks * num_sub_tracks_per_bone;
				const bitset_description bitset_desc = bitset_description::make_from_num_bits(num_sub_tracks);

				// Adding an extra index at the end to delimit things, the index is always invalid: 0xFFFFFFFF
				const uint32_t segment_start_indices_size = clip_error.num_segments > 1 ? (sizeof(uint32_t) * (clip_error.num_segments + 1)) : 0;
				const uint32_t segment_headers_size = sizeof(segment_tier0_header) * clip_error.num_segments;

				// Range data follows constant data, use that to calculate our size
				const uint32_t constant_data_size = (uint32_t)input_transforms_header.clip_range_data_offset - (uint32_t)input_transforms_header.constant_track_data_offset;

				// Animated group size type data follows the range data, use that to calculate our size
				const uint32_t clip_range_data_size = (uint32_t)input_transforms_header.animated_group_types_offset - (uint32_t)input_transforms_header.clip_range_data_offset;

				// The data from our first segment follows the animated group types, use that to calculate our size
				const uint32_t animated_group_types_size = (uint32_t)input_segment_headers[0].segment_data - (uint32_t)input_transforms_header.animated_group_types_offset;

				// Calculate the new size of our clip
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

				buffer_size = align_to(buffer_size, 4);								// Align database header
				buffer_size += sizeof(tracks_database_header);						// Database header

				buffer_size = align_to(buffer_size, 4);								// Align bitsets

				const uint32_t clip_segment_header_size = buffer_size - clip_header_size;

				buffer_size += bitset_desc.get_num_bytes();							// Default tracks bitset
				buffer_size += bitset_desc.get_num_bytes();							// Constant tracks bitset
				buffer_size = align_to(buffer_size, 4);								// Align constant track data
				buffer_size += constant_data_size;									// Constant track data
				buffer_size = align_to(buffer_size, 4);								// Align range data
				buffer_size += clip_range_data_size;								// Range data
				buffer_size += animated_group_types_size;							// Our animated group types

				const uint32_t clip_data_size = buffer_size - clip_segment_header_size - clip_header_size;

				// Per segment data
				for (uint32_t segment_index = 0; segment_index < input_transforms_header.num_segments; ++segment_index)
				{
					const uint8_t* format_per_track_data;
					const uint8_t* range_data;
					const uint8_t* animated_data;
					input_transforms_header.get_segment_data(input_segment_headers[segment_index], format_per_track_data, range_data, animated_data);

					// Range data, whether present or not, follows the per track metadata, use it to calculate our size
					const uint32_t format_per_track_data_size = uint32_t(range_data - format_per_track_data);

					// Animated data follows the range data (present or not), use it to calculate our size
					const uint32_t range_data_size = uint32_t(animated_data - range_data);

					buffer_size += format_per_track_data_size;				// Format per track data

					// TODO: Alignment only necessary with 16bit per component (segment constant tracks), need to fix scalar decoding path
					buffer_size = align_to(buffer_size, 2);					// Align range data
					buffer_size += range_data_size;							// Range data

					// Check our data mapping to find our how many frames we'll retain
					const uint32_t sample_indices = build_sample_indices(tier_mapping, list_index, segment_index);
					const uint32_t num_animated_frames = bitset_count_set_bits(&sample_indices, desc);
					const uint32_t animated_data_size = ((num_animated_frames * input_segment_headers[segment_index].animated_pose_bit_size) + 7) / 8;

					// TODO: Variable bit rate doesn't need alignment
					buffer_size = align_to(buffer_size, 4);					// Align animated data
					buffer_size += animated_data_size;						// Animated track data
				}

				// Optional metadata
				const uint32_t metadata_start_offset = align_to(buffer_size, 4);
				const uint32_t metadata_track_list_name_size = get_metadata_track_list_name_size(input_metadata_header);
				const uint32_t metadata_track_names_size = get_metadata_track_names_size(input_metadata_header);
				const uint32_t metadata_parent_track_indices_size = get_metadata_parent_track_indices_size(input_metadata_header);
				const uint32_t metadata_track_descriptions_size = get_metadata_track_descriptions_size(input_metadata_header);
				const uint32_t metadata_contributing_error_size = 0;	// We'll strip it!

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

				// Allocate our new buffer
				uint8_t* buffer = allocate_type_array_aligned<uint8_t>(context.allocator, buffer_size, alignof(compressed_tracks));
				std::memset(buffer, 0, buffer_size);

				uint8_t* buffer_start = buffer;
				out_compressed_tracks[list_index] = reinterpret_cast<compressed_tracks*>(buffer);

				raw_buffer_header* buffer_header = safe_ptr_cast<raw_buffer_header>(buffer);
				buffer += sizeof(raw_buffer_header);

				tracks_header* header = safe_ptr_cast<tracks_header>(buffer);
				buffer += sizeof(tracks_header);

				// Copy our header and update the parts that change
				std::memcpy(header, &input_header, sizeof(tracks_header));

				header->set_has_database(true);
				header->set_has_metadata(metadata_size != 0);

				transform_tracks_header* transforms_header = safe_ptr_cast<transform_tracks_header>(buffer);
				buffer += sizeof(transform_tracks_header);

				// Copy our header and update the parts that change
				std::memcpy(transforms_header, &input_transforms_header, sizeof(transform_tracks_header));

				const uint32_t segment_start_indices_offset = align_to<uint32_t>(sizeof(transform_tracks_header), 4);	// Relative to the start of our transform_tracks_header
				transforms_header->database_header_offset = align_to(segment_start_indices_offset + segment_start_indices_size, 4);
				transforms_header->segment_headers_offset = align_to(transforms_header->database_header_offset + sizeof(tracks_database_header), 4);
				transforms_header->default_tracks_bitset_offset = align_to(transforms_header->segment_headers_offset + segment_headers_size, 4);
				transforms_header->constant_tracks_bitset_offset = transforms_header->default_tracks_bitset_offset + bitset_desc.get_num_bytes();
				transforms_header->constant_track_data_offset = align_to(transforms_header->constant_tracks_bitset_offset + bitset_desc.get_num_bytes(), 4);
				transforms_header->clip_range_data_offset = align_to(transforms_header->constant_track_data_offset + constant_data_size, 4);
				transforms_header->animated_group_types_offset = transforms_header->clip_range_data_offset + clip_range_data_size;

				// Copy our segment start indices, they do not change
				if (input_transforms_header.has_multiple_segments())
					std::memcpy(transforms_header->get_segment_start_indices(), input_transforms_header.get_segment_start_indices(), segment_start_indices_size);

				// Setup our database header
				tracks_database_header* tracks_db_header = transforms_header->get_database_header();
				tracks_db_header->clip_header_offset = clip_header_offset;

				// Update our clip header offset
				clip_header_offset += sizeof(database_runtime_clip_header);
				clip_header_offset += sizeof(database_runtime_segment_header) * input_transforms_header.num_segments;

				// Write our new segment headers
				const uint32_t segment_data_base_offset = transforms_header->animated_group_types_offset + animated_group_types_size;
				write_segment_headers(tier_mapping, list_index, input_transforms_header, input_segment_headers, segment_data_base_offset, transforms_header->get_segment_tier0_headers());

				// Copy our bitsets, they do not change
				std::memcpy(transforms_header->get_default_tracks_bitset(), input_transforms_header.get_default_tracks_bitset(), bitset_desc.get_num_bytes());
				std::memcpy(transforms_header->get_constant_tracks_bitset(), input_transforms_header.get_constant_tracks_bitset(), bitset_desc.get_num_bytes());

				// Copy our constant track data, it does not change
				std::memcpy(transforms_header->get_constant_track_data(), input_transforms_header.get_constant_track_data(), constant_data_size);

				// Copy our clip range data, it does not change
				std::memcpy(transforms_header->get_clip_range_data(), input_transforms_header.get_clip_range_data(), clip_range_data_size);

				// Copy our animated group type data, it does not change
				std::memcpy(transforms_header->get_animated_group_types(), input_transforms_header.get_animated_group_types(), animated_group_types_size);

				// Write our new segment data
				write_segment_data(tier_mapping, list_index, input_transforms_header, input_segment_headers, *transforms_header, transforms_header->get_segment_tier0_headers());

				if (metadata_size != 0)
				{
					optional_metadata_header* metadata_header = reinterpret_cast<optional_metadata_header*>(buffer_start + buffer_size - sizeof(optional_metadata_header));
					uint32_t metadata_offset = metadata_start_offset;	// Relative to the start of our compressed_tracks

					// Setup our metadata offsets
					if (metadata_track_list_name_size != 0)
					{
						metadata_header->track_list_name = metadata_offset;
						metadata_offset += metadata_track_list_name_size;
					}
					else
						metadata_header->track_list_name = invalid_ptr_offset();

					if (metadata_track_names_size != 0)
					{
						metadata_header->track_name_offsets = metadata_offset;
						metadata_offset += metadata_track_names_size;
					}
					else
						metadata_header->track_name_offsets = invalid_ptr_offset();

					if (metadata_parent_track_indices_size != 0)
					{
						metadata_header->parent_track_indices = metadata_offset;
						metadata_offset += metadata_parent_track_indices_size;
					}
					else
						metadata_header->parent_track_indices = invalid_ptr_offset();

					if (metadata_track_descriptions_size != 0)
					{
						metadata_header->track_descriptions = metadata_offset;
						metadata_offset += metadata_track_descriptions_size;
					}
					else
						metadata_header->track_descriptions = invalid_ptr_offset();

					// Strip the contributing error data, no longer needed
					metadata_header->contributing_error = invalid_ptr_offset();

					ACL_ASSERT((metadata_offset - metadata_start_offset) == metadata_size, "Unexpected metadata size");

					// Copy our metadata, it does not change
					std::memcpy(metadata_header->get_track_list_name(*out_compressed_tracks[list_index]), input_metadata_header.get_track_list_name(*input_tracks), metadata_track_list_name_size);
					std::memcpy(metadata_header->get_track_name_offsets(*out_compressed_tracks[list_index]), input_metadata_header.get_track_name_offsets(*input_tracks), metadata_track_names_size);
					std::memcpy(metadata_header->get_parent_track_indices(*out_compressed_tracks[list_index]), input_metadata_header.get_parent_track_indices(*input_tracks), metadata_parent_track_indices_size);
					std::memcpy(metadata_header->get_track_descriptions(*out_compressed_tracks[list_index]), input_metadata_header.get_track_descriptions(*input_tracks), metadata_track_descriptions_size);
				}

				// Finish the compressed tracks raw buffer header
				buffer_header->size = buffer_size;
				buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(header), buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

				ACL_ASSERT(out_compressed_tracks[list_index]->is_valid(true).empty(), "Failed to build compressed tracks");
			}
		}

		// Returns the number of clips written
		inline uint32_t write_database_clip_metadata(const compressed_tracks* const* db_compressed_tracks_list, uint32_t num_tracks, database_clip_metadata* clip_metadatas)
		{
			if (clip_metadatas == nullptr)
				return num_tracks;	// Nothing to write

			uint32_t clip_header_offset = 0;

			for (uint32_t tracks_index = 0; tracks_index < num_tracks; ++tracks_index)
			{
				const compressed_tracks* tracks = db_compressed_tracks_list[tracks_index];

				database_clip_metadata& clip_metadata = clip_metadatas[tracks_index];
				clip_metadata.clip_hash = tracks->get_hash();
				clip_metadata.clip_header_offset = clip_header_offset;

				const transform_tracks_header& transforms_header = get_transform_tracks_header(*tracks);
				clip_header_offset += sizeof(database_runtime_clip_header) + sizeof(database_runtime_segment_header) * transforms_header.num_segments;
			}

			return num_tracks;
		}

		// Returns a pointer to the first frame of the given segment and the number of frames contained
		inline const frame_tier_mapping* find_segment_frames(const database_tier_mapping& tier_mapping, uint32_t tracks_index, uint32_t segment_index, uint32_t& out_num_frames)
		{
			for (uint32_t frame_index = 0; frame_index < tier_mapping.num_frames; ++frame_index)
			{
				const frame_tier_mapping& frame = tier_mapping.frames[frame_index];
				if (frame.tracks_index != tracks_index)
					continue;	// This is not the tracks instance we care about

				if (frame.segment_index != segment_index)
					continue;	// This is not the segment we care about

				// Found our first frame, count how many we have
				uint32_t num_segment_frames = 0;
				for (uint32_t frame_index2 = frame_index; frame_index2 < tier_mapping.num_frames; ++frame_index2)
				{
					const frame_tier_mapping& frame2 = tier_mapping.frames[frame_index2];
					if (frame2.tracks_index != tracks_index)
						break;	// This is not the tracks instance we care about

					if (frame2.segment_index != segment_index)
						break;	// This is not the segment we care about

					num_segment_frames++;
				}

				// Done
				out_num_frames = num_segment_frames;
				return &frame;
			}

			// This segment doesn't contain any frames
			out_num_frames = 0;
			return nullptr;
		}

		// Returns the number of bits contained in the segment animated data
		inline uint32_t calculate_segment_animated_data_bit_size(const frame_tier_mapping* frames, uint32_t num_frames)
		{
			if (num_frames == 0)
				return 0;	// Empty segment

			// Every frame in a segment has the same size
			return frames->frame_bit_size * num_frames;
		}

		// Returns a bitset with the which frames are present in this segment
		inline uint32_t build_sample_indices(const frame_tier_mapping* frames, uint32_t num_frames)
		{
			const bitset_description desc = bitset_description::make_from_num_bits<32>();

			uint32_t sample_indices = 0;
			for (uint32_t frame_index = 0; frame_index < num_frames; ++frame_index)
			{
				const frame_tier_mapping& frame = frames[frame_index];

				bitset_set(&sample_indices, desc, frame.segment_frame_index, true);
			}

			return sample_indices;
		}

		// Returns the number of bytes written
		inline uint32_t write_segment_data(const frame_tier_mapping* frames, uint32_t num_frames, uint8_t* out_segment_data)
		{
			uint64_t num_bits_written = 0;

			for (uint32_t frame_index = 0; frame_index < num_frames; ++frame_index)
			{
				const frame_tier_mapping& frame = frames[frame_index];

				memcpy_bits(out_segment_data, num_bits_written, frame.animated_data, frame.segment_frame_index * frame.frame_bit_size, frame.frame_bit_size);
				num_bits_written += frame.frame_bit_size;
			}

			return uint32_t(num_bits_written + 7) / 8;
		}

		// Returns the number of chunks written
		inline uint32_t write_database_chunk_descriptions(const frame_assignment_context& context, const compression_database_settings& settings, database_chunk_description* chunk_descriptions)
		{
			const database_tier_mapping& tier_mapping = context.get_tier_mapping(database_tier8::low_importance);
			if (tier_mapping.is_empty())
				return 0;	// No data

			const uint32_t max_chunk_size = settings.max_chunk_size;
			const uint32_t simd_padding = 15;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_size = sizeof(database_chunk_header);
			uint32_t num_chunks = 0;

			for (uint32_t tracks_index = 0; tracks_index < context.num_compressed_tracks; ++tracks_index)
			{
				const compressed_tracks* tracks = context.compressed_tracks_list[tracks_index];
				const transform_tracks_header& transforms_header = get_transform_tracks_header(*tracks);

				for (uint32_t segment_index = 0; segment_index < transforms_header.num_segments; ++segment_index)
				{
					uint32_t num_segment_frames;
					const frame_tier_mapping* segment_frames = find_segment_frames(tier_mapping, tracks_index, segment_index, num_segment_frames);

					const uint32_t segment_data_bit_size = calculate_segment_animated_data_bit_size(segment_frames, num_segment_frames);
					const uint32_t segment_data_size = (segment_data_bit_size + 7) / 8;
					ACL_ASSERT(segment_data_size + simd_padding + sizeof(database_chunk_segment_header) <= max_chunk_size, "Segment is larger than our max chunk size");

					const uint32_t new_chunk_size = chunk_size + segment_data_size + simd_padding + sizeof(database_chunk_segment_header);
					if (new_chunk_size >= max_chunk_size)
					{
						// Chunk is full, write it out and start a new one
						if (chunk_descriptions != nullptr)
						{
							chunk_descriptions[num_chunks].size = max_chunk_size;
							chunk_descriptions[num_chunks].offset = bulk_data_offset;
						}

						bulk_data_offset += max_chunk_size;
						chunk_size = sizeof(database_chunk_header);
						num_chunks++;
					}

					chunk_size += segment_data_size + sizeof(database_chunk_segment_header);

					ACL_ASSERT(chunk_size <= max_chunk_size, "Expected a valid chunk size, segment is larger than max chunk size?");
				}
			}

			// If we have leftover data, finalize our last chunk
			// Because of this, it might end up being larger than the max chunk size which is fine since we can't split a segment over two chunks
			if (chunk_size != sizeof(database_chunk_header))
			{
				if (chunk_descriptions != nullptr)
				{
					chunk_descriptions[num_chunks].size = chunk_size + simd_padding;	// Last chunk needs padding
					chunk_descriptions[num_chunks].offset = bulk_data_offset;
				}

				num_chunks++;
			}

			return num_chunks;
		}

		// Returns the size of the bulk data
		inline uint32_t write_database_bulk_data(const frame_assignment_context& context, const compression_database_settings& settings, const compressed_tracks* const* db_compressed_tracks_list, uint8_t* bulk_data)
		{
			const database_tier_mapping& tier_mapping = context.get_tier_mapping(database_tier8::low_importance);
			if (tier_mapping.is_empty())
				return 0;	// No data

			const uint32_t max_chunk_size = settings.max_chunk_size;
			const uint32_t simd_padding = 15;

			database_chunk_header* chunk_header = nullptr;
			database_chunk_segment_header* segment_chunk_headers = nullptr;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_sample_data_offset = 0;
			uint32_t chunk_size = sizeof(database_chunk_header);
			uint32_t chunk_index = 0;

			uint32_t clip_header_offset = 0;

			if (bulk_data != nullptr)
			{
				// Setup our chunk headers
				chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data);
				chunk_header->index = chunk_index;
				chunk_header->size = 0;
				chunk_header->num_segments = 0;

				segment_chunk_headers = chunk_header->get_segment_headers();
			}

			// We first iterate to find our chunk delimitations and write our headers
			for (uint32_t tracks_index = 0; tracks_index < context.num_compressed_tracks; ++tracks_index)
			{
				const compressed_tracks* tracks = db_compressed_tracks_list[tracks_index];
				const transform_tracks_header& transforms_header = get_transform_tracks_header(*tracks);

				uint32_t segment_header_offset = clip_header_offset + sizeof(database_runtime_clip_header);

				for (uint32_t segment_index = 0; segment_index < transforms_header.num_segments; ++segment_index)
				{
					uint32_t num_segment_frames;
					const frame_tier_mapping* segment_frames = find_segment_frames(tier_mapping, tracks_index, segment_index, num_segment_frames);

					const uint32_t segment_data_bit_size = calculate_segment_animated_data_bit_size(segment_frames, num_segment_frames);
					const uint32_t segment_data_size = (segment_data_bit_size + 7) / 8;
					ACL_ASSERT(segment_data_size + simd_padding + sizeof(database_chunk_segment_header) <= max_chunk_size, "Segment is larger than our max chunk size");

					const uint32_t new_chunk_size = chunk_size + segment_data_size + simd_padding + sizeof(database_chunk_segment_header);
					if (new_chunk_size >= max_chunk_size)
					{
						// Finalize our chunk header
						if (bulk_data != nullptr)
							chunk_header->size = max_chunk_size;

						// Chunk is full, start a new one
						bulk_data_offset += max_chunk_size;
						chunk_sample_data_offset = 0;
						chunk_size = sizeof(database_chunk_header);
						chunk_index++;

						// Setup our chunk headers
						if (bulk_data != nullptr)
						{
							chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_offset);
							chunk_header->index = chunk_index;
							chunk_header->size = 0;
							chunk_header->num_segments = 0;

							segment_chunk_headers = chunk_header->get_segment_headers();
						}
					}

					if (bulk_data != nullptr)
					{
						// TODO: Should we skip segments with no data?

						// Update our chunk headers
						database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_header->num_segments];
						segment_chunk_header.clip_hash = tracks->get_hash();
						segment_chunk_header.sample_indices = build_sample_indices(segment_frames, num_segment_frames);
						segment_chunk_header.samples_offset = chunk_sample_data_offset;	// Relative to start of sample data for now
						segment_chunk_header.clip_header_offset = clip_header_offset;
						segment_chunk_header.segment_header_offset = segment_header_offset;

						chunk_header->num_segments++;
					}

					chunk_size += segment_data_size + sizeof(database_chunk_segment_header);
					chunk_sample_data_offset += segment_data_size;
					segment_header_offset += sizeof(database_runtime_segment_header);

					ACL_ASSERT(chunk_size <= max_chunk_size, "Expected a valid chunk size, segment is larger than max chunk size?");
				}

				clip_header_offset = segment_header_offset;
			}

			// If we have leftover data, finalize our last chunk
			// Because of this, it might end up being larger than the max chunk size which is fine since we can't split a segment over two chunks
			if (chunk_size != sizeof(database_chunk_header))
			{
				chunk_size += simd_padding;	// Last chunk needs padding

				if (bulk_data != nullptr)
				{
					// Finalize our chunk header
					chunk_header->size = chunk_size;
				}

				bulk_data_offset += chunk_size;
			}

			// Now that our chunk headers are written, write our sample data
			if (bulk_data != nullptr)
			{
				// Reset our header pointers
				chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data);
				segment_chunk_headers = chunk_header->get_segment_headers();

				uint32_t chunk_segment_index = 0;
				for (uint32_t tracks_index = 0; tracks_index < context.num_compressed_tracks; ++tracks_index)
				{
					const compressed_tracks* tracks = db_compressed_tracks_list[tracks_index];
					const transform_tracks_header& transforms_header = get_transform_tracks_header(*tracks);

					for (uint32_t segment_index = 0; segment_index < transforms_header.num_segments; ++segment_index)
					{
						uint32_t num_segment_frames;
						const frame_tier_mapping* segment_frames = find_segment_frames(tier_mapping, tracks_index, segment_index, num_segment_frames);

						const uint32_t segment_data_bit_size = calculate_segment_animated_data_bit_size(segment_frames, num_segment_frames);
						const uint32_t segment_data_size = (segment_data_bit_size + 7) / 8;

						if (chunk_segment_index >= chunk_header->num_segments)
						{
							// We hit the next chunk, update our pointers
							chunk_header = add_offset_to_ptr<database_chunk_header>(chunk_header, chunk_header->size);
							segment_chunk_headers = chunk_header->get_segment_headers();
							chunk_segment_index = 0;
						}

						// Calculate the finale offset for our chunk's data relative to the bulk data start and the final header size
						const uint32_t chunk_data_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(chunk_header) - bulk_data);
						const uint32_t chunk_header_size = sizeof(database_chunk_header) + chunk_header->num_segments * sizeof(database_chunk_segment_header);

						// Update the sample offset from being relative to the start of the sample data to the start of the bulk data
						database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_segment_index];
						segment_chunk_header.samples_offset = chunk_data_offset + chunk_header_size + segment_chunk_header.samples_offset;

						uint8_t* animated_data = segment_chunk_header.samples_offset.add_to(bulk_data);
						const uint32_t size = write_segment_data(segment_frames, num_segment_frames, animated_data);
						ACL_ASSERT(size == segment_data_size, "Unexpected segment data size"); (void)size;

						chunk_segment_index++;
					}
				}
			}

			return bulk_data_offset;
		}

		inline compressed_database* build_compressed_database(const frame_assignment_context& context, const compression_database_settings& settings, const compressed_tracks* const* db_compressed_tracks_list)
		{
			// Find our chunk limits and calculate our database size
			const uint32_t num_tracks = write_database_clip_metadata(db_compressed_tracks_list, context.num_compressed_tracks, nullptr);
			const uint32_t num_segments = calculate_num_segments(db_compressed_tracks_list, context.num_compressed_tracks);
			const uint32_t num_chunks = write_database_chunk_descriptions(context, settings, nullptr);
			const uint32_t bulk_data_size = write_database_bulk_data(context, settings, db_compressed_tracks_list, nullptr);

			uint32_t database_buffer_size = 0;
			database_buffer_size += sizeof(raw_buffer_header);							// Header
			database_buffer_size += sizeof(database_header);							// Header

			database_buffer_size = align_to(database_buffer_size, 4);					// Align chunk descriptions
			database_buffer_size += num_chunks * sizeof(database_chunk_description);	// Chunk descriptions

			database_buffer_size = align_to(database_buffer_size, 4);					// Align clip hashes
			database_buffer_size += num_tracks * sizeof(database_clip_metadata);		// Clip metadata

			database_buffer_size = align_to(database_buffer_size, 8);					// Align bulk data
			database_buffer_size += bulk_data_size;										// Bulk data

			uint8_t* database_buffer = allocate_type_array_aligned<uint8_t>(context.allocator, database_buffer_size, alignof(compressed_database));
			std::memset(database_buffer, 0, database_buffer_size);

			compressed_database* database = reinterpret_cast<compressed_database*>(database_buffer);

			const uint8_t* database_buffer_start = database_buffer;

			raw_buffer_header* database_buffer_header = safe_ptr_cast<raw_buffer_header>(database_buffer);
			database_buffer += sizeof(raw_buffer_header);

			uint8_t* db_header_start = database_buffer;
			database_header* db_header = safe_ptr_cast<database_header>(database_buffer);
			database_buffer += sizeof(database_header);

			// Write our header
			db_header->tag = static_cast<uint32_t>(buffer_tag32::compressed_database);
			db_header->version = compressed_tracks_version16::latest;
			db_header->num_chunks = num_chunks;
			db_header->max_chunk_size = settings.max_chunk_size;
			db_header->num_clips = num_tracks;
			db_header->num_segments = num_segments;
			db_header->bulk_data_size = bulk_data_size;
			db_header->set_is_bulk_data_inline(true);	// Data is always inline when compressing

			database_buffer = align_to(database_buffer, 4);								// Align chunk descriptions
			database_buffer += num_chunks * sizeof(database_chunk_description);			// Chunk descriptions

			database_buffer = align_to(database_buffer, 4);								// Align clip hashes
			db_header->clip_metadata_offset = database_buffer - db_header_start;		// Clip metadata
			database_buffer += num_tracks * sizeof(database_clip_metadata);				// Clip metadata

			database_buffer = align_to(database_buffer, 8);								// Align bulk data
			if (bulk_data_size != 0)
				db_header->bulk_data_offset = database_buffer - db_header_start;		// Bulk data
			else
				db_header->bulk_data_offset = invalid_ptr_offset();
			database_buffer += bulk_data_size;											// Bulk data

			// Write our chunk descriptions
			const uint32_t num_written_chunks = write_database_chunk_descriptions(context, settings, db_header->get_chunk_descriptions());
			ACL_ASSERT(num_written_chunks == num_chunks, "Unexpected amount of data written"); (void)num_written_chunks;

			// Write our clip metadata
			const uint32_t num_written_tracks = write_database_clip_metadata(db_compressed_tracks_list, context.num_compressed_tracks, db_header->get_clip_metadatas());
			ACL_ASSERT(num_written_tracks == num_tracks, "Unexpected amount of data written"); (void)num_written_tracks;

			// Write our bulk data
			const uint32_t written_bulk_data_size = write_database_bulk_data(context, settings, db_compressed_tracks_list, db_header->get_bulk_data());
			ACL_ASSERT(written_bulk_data_size == bulk_data_size, "Unexpected amount of data written"); (void)written_bulk_data_size;
			db_header->bulk_data_hash = hash32(db_header->get_bulk_data(), bulk_data_size);

			ACL_ASSERT(uint32_t(database_buffer - database_buffer_start) == database_buffer_size, "Unexpected amount of data written"); (void)database_buffer_start;

#if defined(ACL_HAS_ASSERT_CHECKS)
			// Make sure nobody overwrote our padding (contained in last chunk if we have data)
			if (bulk_data_size != 0)
			{
				for (const uint8_t* padding = database_buffer - 15; padding < database_buffer; ++padding)
					ACL_ASSERT(*padding == 0, "Padding was overwritten");
			}
#endif

			// Finish the raw buffer header
			database_buffer_header->size = database_buffer_size;
			database_buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(db_header), database_buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

			ACL_ASSERT(database->is_valid(true).empty(), "Failed to build compressed database");

			return database;
		}
	}

	inline error_result build_database(iallocator& allocator, const compression_database_settings& settings,
		const compressed_tracks* const* compressed_tracks_list, uint32_t num_compressed_tracks,
		compressed_tracks** out_compressed_tracks, compressed_database*& out_database)
	{
		using namespace acl_impl;

		// Reset everything just to be safe
		for (uint32_t list_index = 0; list_index < num_compressed_tracks; ++list_index)
			out_compressed_tracks[list_index] = nullptr;
		out_database = nullptr;

		// Validate everything and early out if something isn't right
		const error_result settings_result = settings.is_valid();
		if (settings_result.any())
			return error_result("Compression database settings are invalid");

		if (compressed_tracks_list == nullptr || num_compressed_tracks == 0)
			return error_result("No compressed track list provided");

		for (uint32_t list_index = 0; list_index < num_compressed_tracks; ++list_index)
		{
			const compressed_tracks* tracks = compressed_tracks_list[list_index];
			if (tracks == nullptr)
				return error_result("Compressed track list contains a null entry");

			const error_result tracks_result = tracks->is_valid(false);
			if (tracks_result.any())
				return error_result("Compressed track instance is invalid");

			if (tracks->has_database())
				return error_result("Compressed track instance is already bound to a database");

			const tracks_header& header = get_tracks_header(*tracks);
			if (!header.get_has_metadata())
				return error_result("Compressed track instance does not contain any metadata");

			const optional_metadata_header& metadata_header = get_optional_metadata_header(*tracks);
			if (!metadata_header.contributing_error.is_valid())
				return error_result("Compressed track instance does not contain contributing error metadata");
		}

		// Calculate how many frames are movable to the database
		// A frame is movable if it isn't the first or last frame of a segment
		const uint32_t num_frames = calculate_num_frames(compressed_tracks_list, num_compressed_tracks);
		const uint32_t num_movable_frames = calculate_num_movable_frames(compressed_tracks_list, num_compressed_tracks);

		// Calculate how many frames we'll move to every tier
		const uint32_t num_low_importance_frames = std::min<uint32_t>(num_movable_frames, uint32_t(settings.low_importance_tier_proportion * float(num_frames)));
		const uint32_t num_high_importance_frames = num_frames - num_low_importance_frames;

		frame_assignment_context context(allocator, compressed_tracks_list, num_compressed_tracks, num_movable_frames);
		context.set_tier_num_frames(database_tier8::high_importance, num_high_importance_frames);
		context.set_tier_num_frames(database_tier8::low_importance, num_low_importance_frames);

		// Assign every frame to its tier
		assign_frames_to_tiers(context);

		// Build our new compressed track instances with the high importance tier data
		build_compressed_tracks(context, out_compressed_tracks);

		// Build our database with the lower tier data
		out_database = build_compressed_database(context, settings, out_compressed_tracks);

		return error_result();
	}

	inline error_result split_compressed_database_bulk_data(iallocator& allocator, const compressed_database& database, compressed_database*& out_split_database, uint8_t*& out_bulk_data)
	{
		using namespace acl_impl;

		const error_result result = database.is_valid(true);
		if (result.any())
			return result;

		if (!database.is_bulk_data_inline())
			return error_result("Bulk data is not inline in source database");

		const uint32_t total_size = database.get_total_size();
		const uint32_t bulk_data_size = database.get_bulk_data_size();
		const uint32_t db_size = total_size - bulk_data_size;

		// Allocate and setup our new database
		uint8_t* database_buffer = allocate_type_array_aligned<uint8_t>(allocator, db_size, alignof(compressed_database));
		out_split_database = reinterpret_cast<compressed_database*>(database_buffer);

		std::memcpy(database_buffer, &database, db_size);

		raw_buffer_header* database_buffer_header = safe_ptr_cast<raw_buffer_header>(database_buffer);
		database_buffer += sizeof(raw_buffer_header);

		database_header* db_header = safe_ptr_cast<database_header>(database_buffer);
		database_buffer += sizeof(database_header);

		db_header->bulk_data_offset = invalid_ptr_offset();
		db_header->set_is_bulk_data_inline(false);

		database_buffer_header->size = db_size;
		database_buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(db_header), db_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header
		ACL_ASSERT(out_split_database->is_valid(true).empty(), "Failed to split database");

		// Allocate and setup our new bulk data
		uint8_t* bulk_data_buffer = bulk_data_size != 0 ? allocate_type_array_aligned<uint8_t>(allocator, bulk_data_size, alignof(compressed_database)) : nullptr;
		out_bulk_data = bulk_data_buffer;

		std::memcpy(bulk_data_buffer, database.get_bulk_data(), bulk_data_size);

		const uint32_t bulk_data_hash = hash32(bulk_data_buffer, bulk_data_size);
		ACL_ASSERT(bulk_data_hash == database.get_bulk_data_hash(), "Bulk data hash mismatch"); (void)bulk_data_hash;

		return error_result();
	}

	inline error_result database_merge_mapping::is_valid() const
	{
		if (tracks == nullptr)
			return error_result("No compressed tracks provided");

		if (tracks->is_valid(false).any())
			return error_result("Compressed tracks aren't valid");

		if (database == nullptr)
			return error_result("No compressed database provided");

		if (database->is_valid(false).any())
			return error_result("Compressed database isn't valid");

		if (!database->contains(*tracks))
			return error_result("Compressed database doesn't contain the compressed tracks");

		if (!database->is_bulk_data_inline())
			return error_result("Compressed database does not have inline bulk data");

		if (database->get_num_clips() != 1)
			return error_result("Compressed database already contains more than 1 clip");

		return error_result();
	}

	namespace acl_impl
	{
		// Returns the number of chunks written
		inline uint32_t write_database_chunk_descriptions(const compression_database_settings& settings, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, database_chunk_description* chunk_descriptions)
		{
			const uint32_t max_chunk_size = settings.max_chunk_size;
			const uint32_t simd_padding = 15;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_size = sizeof(database_chunk_header);
			uint32_t num_chunks = 0;

			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_database& database = *merge_mappings[mapping_index].database;
				const database_header& header = get_database_header(database);
				const database_chunk_description* db_chunk_descriptions = header.get_chunk_descriptions();
				const uint32_t num_db_chunks = header.num_chunks;
				for (uint32_t chunk_index = 0; chunk_index < num_db_chunks; ++chunk_index)
				{
					const database_chunk_description& chunk_description = db_chunk_descriptions[chunk_index];
					if (chunk_description.size >= max_chunk_size)
					{
						// Chunk is already full, add it as-is
						if (chunk_descriptions != nullptr)
						{
							chunk_descriptions[num_chunks].size = chunk_description.size;
							chunk_descriptions[num_chunks].offset = bulk_data_offset;
						}

						bulk_data_offset += chunk_description.size;
						num_chunks++;
						continue;
					}

					// The last chunk already has SIMD padding, remove it
					const uint32_t last_chunk_padding = (chunk_index == (num_db_chunks - 1)) ? simd_padding : 0;
					const uint32_t new_chunk_size = chunk_size + chunk_description.size + simd_padding - last_chunk_padding;
					if (new_chunk_size >= max_chunk_size)
					{
						// Chunk is full, write it out and start a new one
						if (chunk_descriptions != nullptr)
						{
							chunk_descriptions[num_chunks].size = max_chunk_size;
							chunk_descriptions[num_chunks].offset = bulk_data_offset;
						}

						bulk_data_offset += max_chunk_size;
						chunk_size = sizeof(database_chunk_header);
						num_chunks++;
					}

					// Make sure we aren't larger than the new chunk we just created
					ACL_ASSERT((chunk_size + chunk_description.size + simd_padding - last_chunk_padding) < max_chunk_size, "Chunk size is too large");

					// Update our chunk size and remove the padding if present and the chunk header since we have our own
					chunk_size += chunk_description.size - last_chunk_padding - sizeof(database_chunk_header);
				}
			}

			// If we wrote any data, finalize our chunk
			if (chunk_size != sizeof(database_chunk_header))
			{
				if (chunk_descriptions != nullptr)
				{
					chunk_descriptions[num_chunks].size = chunk_size + simd_padding;
					chunk_descriptions[num_chunks].offset = bulk_data_offset;
				}

				num_chunks++;
			}

			return num_chunks;
		}

		struct merged_db_metadata
		{
			uint32_t num_chunks = 0;
			uint32_t num_clips = 0;
			uint32_t num_segments = 0;
		};

		inline uint32_t merged_database_segment_index_to_mapping_index(uint32_t merged_database_segment_index, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t merged_segment_count = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

				if (merged_database_segment_index >= merged_segment_count && merged_database_segment_index < merged_segment_count + transform_header.num_segments)
					return mapping_index;

				merged_segment_count += transform_header.num_segments;
			}

			ACL_ASSERT(false, "Failed to find mapping index");
			return ~0U;
		}

		inline uint32_t merged_database_segment_index_to_runtime_clip_offset(uint32_t merged_database_segment_index, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t merged_segment_count = 0;
			uint32_t runtime_offset = 0;
			uint32_t clip_runtime_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

				clip_runtime_offset = runtime_offset;
				runtime_offset += sizeof(database_runtime_clip_header);

				if (merged_database_segment_index >= merged_segment_count && merged_database_segment_index < merged_segment_count + transform_header.num_segments)
					return clip_runtime_offset;

				runtime_offset += transform_header.num_segments * sizeof(database_runtime_segment_header);
				merged_segment_count += transform_header.num_segments;
			}

			ACL_ASSERT(false, "Failed to find mapping index");
			return ~0U;
		}

		inline uint32_t merged_database_segment_index_to_runtime_segment_offset(uint32_t merged_database_segment_index, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t merged_segment_count = 0;
			uint32_t runtime_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

				runtime_offset += sizeof(database_runtime_clip_header);

				if (merged_database_segment_index >= merged_segment_count && merged_database_segment_index < merged_segment_count + transform_header.num_segments)
				{
					const uint32_t clip_segment_index = merged_database_segment_index - merged_segment_count;
					return runtime_offset + clip_segment_index * sizeof(database_runtime_segment_header);
				}

				runtime_offset += transform_header.num_segments * sizeof(database_runtime_segment_header);
				merged_segment_count += transform_header.num_segments;
			}

			ACL_ASSERT(false, "Failed to find mapping index");
			return ~0U;
		}

		// Returns the size of the bulk data
		inline uint32_t write_database_bulk_data(iallocator& allocator, const compression_database_settings& settings, const merged_db_metadata& db_metadata, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, uint8_t* bulk_data)
		{
			(void)db_metadata;

			// TODO: If the last chunk is too small, merge it with the previous chunk?

			const uint32_t max_chunk_size = settings.max_chunk_size;
			const uint32_t simd_padding = 15;

			uint8_t* tmp_chunk_data = nullptr;

			database_chunk_header* tmp_chunk_header = nullptr;
			database_chunk_segment_header* tmp_segment_chunk_headers = nullptr;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_sample_data_offset = 0;
			uint32_t tmp_chunk_size = sizeof(database_chunk_header);

			if (bulk_data != nullptr)
			{
				// Allocate a temporary chunk so we can append to it while we build the final bulk data
				tmp_chunk_data = allocate_type_array<uint8_t>(allocator, max_chunk_size);

				// Reset our temporary chunk
				std::memset(tmp_chunk_data, 0, max_chunk_size);

				// Setup our chunk pointers
				tmp_chunk_header = safe_ptr_cast<database_chunk_header>(tmp_chunk_data);
				tmp_segment_chunk_headers = tmp_chunk_header->get_segment_headers();

				// Our temporary chunk is used to hold partial chunks, we set an invalid chunk index to be able to identify partial chunks
				// in our second step (detailed below). Whole chunks will have a valid chunk index that is fixed up later.
				tmp_chunk_header->index = ~0U;
			}

			// The merge process of the bulk data is performed in three steps:
			//    - We first find where the chunks break down by appending whole chunks as-is and by merging partial chunks
			//      when possible. Whole chunks will require minor fixup later.
			//    - Once we know where the chunks start and end, we copy the partial chunk data and fixup our offsets
			//    - With our chunk data in place in the bulk data buffer, we can fixup our headers
			//
			// Whole chunks we copy have their sample data offsets relative to the original bulk data them belong into.
			// We convert it to relative offsets in the first step and back to absolute offsets in the second step.
			//
			// To simplify iteration in the second step, we re-use the chunk segment clip's header offset to map where the segment data
			// lives in the source chunk. We setup the offset in the first step for partial chunks, copy in the second step and set
			// the final clip header offset value in the third step. The sample offset alone isn't enough to know which database it
			// comes from as such as also re-purpose the clip hash to be the merged database segment index.
			// We also re-purpose the clip hash for whole chunks to properly fixup the runtime header offsets in the third step.
			// The clip hash will be properly set in the third step.
			//
			// We similarly re-use the chunk segment segment's header offset to contain the segment data size to copy. We set this for
			// partial chunks in the first step, use it to copy our data in the second step. We set its final segment offset value
			// in the third step.
			//
			// This avoids the need to allocate and manage separate metadata.

			{
				uint32_t merged_database_segment_index = 0;

				// First step: we iterate to find our chunk delimitations and write our headers
				for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
				{
					const compressed_database& database = *merge_mappings[mapping_index].database;
					const database_header& header = get_database_header(database);
					const database_chunk_description* db_chunk_descriptions = header.get_chunk_descriptions();
					const uint32_t num_db_chunks = header.num_chunks;

					for (uint32_t chunk_index = 0; chunk_index < num_db_chunks; ++chunk_index)
					{
						const database_chunk_description& db_chunk_description = db_chunk_descriptions[chunk_index];
						if (db_chunk_description.size >= max_chunk_size)
						{
							// Chunk is already full, add it as-is
							if (bulk_data != nullptr)
							{
								// Append our new chunk right away
								std::memcpy(bulk_data + bulk_data_offset, db_chunk_description.get_chunk_header(database.get_bulk_data()), db_chunk_description.size);

								// Chunk indices will be fixed up later

								// Fixup our offsets
								database_chunk_header* new_chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_offset);
								database_chunk_segment_header* new_segment_chunk_headers = new_chunk_header->get_segment_headers();

								const uint32_t db_chunk_offset = db_chunk_description.offset;
								const uint32_t chunk_header_size = sizeof(database_chunk_header) + new_chunk_header->num_segments * sizeof(database_chunk_segment_header);

								// Update our chunk headers
								for (uint32_t db_chunk_segment_index = 0; db_chunk_segment_index < new_chunk_header->num_segments; ++db_chunk_segment_index)
								{
									database_chunk_segment_header& segment_chunk_header = new_segment_chunk_headers[db_chunk_segment_index];

									// Original samples_offset is relative to the start of the bulk data but we need it relative
									// to the start of the chunk for now
									segment_chunk_header.samples_offset = segment_chunk_header.samples_offset - db_chunk_offset - chunk_header_size;

									// See comment at the top of this function, we re-purpose this value
									segment_chunk_header.clip_hash = merged_database_segment_index;

									merged_database_segment_index++;
								}
							}

							bulk_data_offset += db_chunk_description.size;
							continue;
						}

						// The last chunk already has SIMD padding, remove it
						const uint32_t last_chunk_padding = (chunk_index == (num_db_chunks - 1)) ? simd_padding : 0;
						const uint32_t new_chunk_size = tmp_chunk_size + db_chunk_description.size + simd_padding - last_chunk_padding;
						if (new_chunk_size >= max_chunk_size)
						{
							// Chunk is full

							// Finalize our chunk header
							if (bulk_data != nullptr)
							{
								tmp_chunk_header->size = max_chunk_size;

								// Copy our temporary chunk into its final location in the bulk data
								std::memcpy(bulk_data + bulk_data_offset, tmp_chunk_header, max_chunk_size);

								// Reset our temporary chunk
								std::memset(tmp_chunk_data, 0, max_chunk_size);
								tmp_chunk_header->index = ~0U;
							}

							// Start a new one
							bulk_data_offset += max_chunk_size;
							chunk_sample_data_offset = 0;
							tmp_chunk_size = sizeof(database_chunk_header);
						}

						// Make sure we aren't larger than the new chunk we just created
						ACL_ASSERT((tmp_chunk_size + db_chunk_description.size + simd_padding - last_chunk_padding) < max_chunk_size, "Chunk size is too large");

						if (bulk_data != nullptr)
						{
							// TODO: Should we skip segments with no data?

							// Update our chunk headers
							const database_chunk_header* db_chunk_header = db_chunk_description.get_chunk_header(database.get_bulk_data());
							const database_chunk_segment_header* db_segment_chunk_headers = db_chunk_header->get_segment_headers();
							for (uint32_t db_chunk_segment_index = 0; db_chunk_segment_index < db_chunk_header->num_segments; ++db_chunk_segment_index)
							{
								const database_chunk_segment_header& db_segment_chunk_header = db_segment_chunk_headers[db_chunk_segment_index];
								database_chunk_segment_header& segment_chunk_header = tmp_segment_chunk_headers[tmp_chunk_header->num_segments];

								segment_chunk_header.sample_indices = db_segment_chunk_header.sample_indices;
								segment_chunk_header.samples_offset = chunk_sample_data_offset;	// Relative to start of the sample data for now

								uint32_t segment_data_size;
								if (db_chunk_segment_index + 1 < db_chunk_header->num_segments)
								{
									// Not the last segment, use the offset from the next one to calculate our size
									segment_data_size = db_segment_chunk_headers[db_chunk_segment_index + 1].samples_offset - db_segment_chunk_header.samples_offset;
								}
								else
								{
									// Last segment, use the end of the chunk to calculate our size
									segment_data_size = db_chunk_description.offset + db_chunk_description.size - last_chunk_padding - db_segment_chunk_header.samples_offset;
								}

								// See comment at the top of this function, we re-purpose these offsets/values
								segment_chunk_header.clip_hash = merged_database_segment_index;
								segment_chunk_header.clip_header_offset = static_cast<uint32_t>(db_segment_chunk_header.samples_offset);
								segment_chunk_header.segment_header_offset = segment_data_size;

								chunk_sample_data_offset += segment_data_size;
								merged_database_segment_index++;
								tmp_chunk_header->num_segments++;
							}
						}

						// Update our chunk size and remove the padding if present and the chunk header since we have our own
						tmp_chunk_size += db_chunk_description.size - last_chunk_padding - sizeof(database_chunk_header);
					}
				}

				// If we wrote any data, finalize our chunk
				if (tmp_chunk_size != sizeof(database_chunk_header))
				{
					if (bulk_data != nullptr)
					{
						// Finalize our chunk header
						tmp_chunk_header->size = tmp_chunk_size + simd_padding;

						// Copy our temporary chunk into its final location in the bulk data
						std::memcpy(bulk_data + bulk_data_offset, tmp_chunk_header, tmp_chunk_header->size);
					}

					bulk_data_offset += tmp_chunk_size + simd_padding;
				}

				ACL_ASSERT(bulk_data == nullptr || merged_database_segment_index == db_metadata.num_segments, "Unexpected segment count");
			}

			// Now that our chunk headers are written, write our sample data and do the final fixup for the headers
			if (bulk_data != nullptr)
			{
				uint32_t bulk_data_update_offset = 0;

				// Second step: copy our partial chunk data into its final location
				while (bulk_data_update_offset < bulk_data_offset)
				{
					database_chunk_header* chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_update_offset);
					database_chunk_segment_header* segment_chunk_headers = chunk_header->get_segment_headers();

					// Move onto the next chunk
					bulk_data_update_offset += chunk_header->size;

					for (uint32_t chunk_segment_index = 0; chunk_segment_index < chunk_header->num_segments; ++chunk_segment_index)
					{
						database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_segment_index];

						// See comment at the top of this function, we re-purpose these offsets/values
						const uint32_t merged_database_segment_index = segment_chunk_header.clip_hash;

						// Calculate the finale offset for our chunk's data relative to the bulk data start and the final header size
						const uint32_t chunk_data_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(chunk_header) - bulk_data);
						const uint32_t chunk_header_size = sizeof(database_chunk_header) + chunk_header->num_segments * sizeof(database_chunk_segment_header);

						// Update the sample offset from being relative to the start of the sample data to the start of the bulk data
						segment_chunk_header.samples_offset = chunk_data_offset + chunk_header_size + segment_chunk_header.samples_offset;

						// Copy our partial chunk data
						if (chunk_header->index == ~0U)
						{
							// See comment at the top of this function, we re-purpose these offsets/values
							const uint32_t segment_data_size = segment_chunk_header.segment_header_offset;
							const ptr_offset32<const uint8_t> src_samples_offset = static_cast<uint32_t>(segment_chunk_header.clip_header_offset);

							const uint32_t mapping_index = merged_database_segment_index_to_mapping_index(merged_database_segment_index, merge_mappings, num_merge_mappings);
							const compressed_database& database = *merge_mappings[mapping_index].database;

							const uint8_t* src_animated_data = src_samples_offset.add_to(database.get_bulk_data());

							uint8_t* dst_animated_data = segment_chunk_header.samples_offset.add_to(bulk_data);

							std::memcpy(dst_animated_data, src_animated_data, segment_data_size);
						}
					}
				}

				// Reset iteration
				bulk_data_update_offset = 0;

				uint32_t chunk_index = 0;

				// Third step: fixup our headers
				while (bulk_data_update_offset < bulk_data_offset)
				{
					database_chunk_header* chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_update_offset);
					database_chunk_segment_header* segment_chunk_headers = chunk_header->get_segment_headers();

					// Move onto the next chunk
					bulk_data_update_offset += chunk_header->size;

					// Set our chunk index
					chunk_header->index = chunk_index++;

					for (uint32_t chunk_segment_index = 0; chunk_segment_index < chunk_header->num_segments; ++chunk_segment_index)
					{
						database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_segment_index];

						// See comment at the top of this function, we re-purpose this value
						const uint32_t merged_database_segment_index = segment_chunk_header.clip_hash;

						const uint32_t mapping_index = merged_database_segment_index_to_mapping_index(merged_database_segment_index, merge_mappings, num_merge_mappings);
						const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
						const uint32_t clip_hash = tracks.get_hash();

						// Set our final values now that they are known
						segment_chunk_header.clip_hash = clip_hash;
						segment_chunk_header.clip_header_offset = merged_database_segment_index_to_runtime_clip_offset(merged_database_segment_index, merge_mappings, num_merge_mappings);
						segment_chunk_header.segment_header_offset = merged_database_segment_index_to_runtime_segment_offset(merged_database_segment_index, merge_mappings, num_merge_mappings);
					}
				}
			}

			deallocate_type_array(allocator, tmp_chunk_data, max_chunk_size);

			return bulk_data_offset;
		}

		inline void write_database_clip_metadata(const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, database_clip_metadata* clip_metadata)
		{
			uint32_t runtime_header_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;

				const transform_tracks_header& transforms_header = get_transform_tracks_header(tracks);

				clip_metadata[mapping_index].clip_hash = tracks.get_hash();
				clip_metadata[mapping_index].clip_header_offset = runtime_header_offset;

				runtime_header_offset += sizeof(database_runtime_clip_header) + transforms_header.num_segments * sizeof(database_runtime_segment_header);
			}
		}

		inline void update_input_mappings(const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t runtime_header_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;

				// Update our metadata
				transform_tracks_header& transforms_header = get_transform_tracks_header(tracks);
				tracks_database_header* tracks_db_header = transforms_header.get_database_header();
				tracks_db_header->clip_header_offset = runtime_header_offset;

				// Recalculate our hash
				raw_buffer_header* buffer_header = safe_ptr_cast<raw_buffer_header>(&tracks);
				uint8_t* tracks_header = add_offset_to_ptr<uint8_t>(&tracks, sizeof(raw_buffer_header));
				buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(tracks_header), buffer_header->size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

				// Update our header offset
				runtime_header_offset += sizeof(database_runtime_clip_header) + transforms_header.num_segments * sizeof(database_runtime_segment_header);
			}
		}
	}

	inline error_result merge_compressed_databases(iallocator& allocator, const compression_database_settings& settings, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, compressed_database*& out_merged_compressed_database)
	{
		using namespace acl_impl;

		const error_result settings_result = settings.is_valid();
		if (settings_result.any())
			return settings_result;

		if (merge_mappings == nullptr || num_merge_mappings == 0)
			return error_result("No merge mappings provided");

		for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
		{
			const error_result result = merge_mappings[mapping_index].is_valid();
			if (result.any())
				return result;

			const acl_impl::database_header& header = acl_impl::get_database_header(*merge_mappings[mapping_index].database);
			if (settings.max_chunk_size < header.max_chunk_size)
				return error_result("Cannot merge databases into smaller chunks");
		}

		// Since we'll create a new merged database, the input mappings need to be updated to point to the new merged database
		// Do so now since it'll change our hash which we need later for safe binding
		update_input_mappings(merge_mappings, num_merge_mappings);

		merged_db_metadata db_metadata;

		for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
		{
			db_metadata.num_clips += merge_mappings[mapping_index].database->get_num_clips();
			db_metadata.num_segments += merge_mappings[mapping_index].database->get_num_segments();
		}

		db_metadata.num_chunks = write_database_chunk_descriptions(settings, merge_mappings, num_merge_mappings, nullptr);
		const uint32_t bulk_data_size = write_database_bulk_data(allocator, settings, db_metadata, merge_mappings, num_merge_mappings, nullptr);

		uint32_t database_buffer_size = 0;
		database_buffer_size += sizeof(raw_buffer_header);							// Header
		database_buffer_size += sizeof(database_header);							// Header

		database_buffer_size = align_to(database_buffer_size, 4);					// Align chunk descriptions
		database_buffer_size += db_metadata.num_chunks * sizeof(database_chunk_description);	// Chunk descriptions

		database_buffer_size = align_to(database_buffer_size, 4);					// Align clip hashes
		database_buffer_size += db_metadata.num_clips * sizeof(database_clip_metadata);			// Clip metadata (only one when we compress)

		database_buffer_size = align_to(database_buffer_size, 8);					// Align bulk data
		database_buffer_size += bulk_data_size;										// Bulk data

		uint8_t* database_buffer = allocate_type_array_aligned<uint8_t>(allocator, database_buffer_size, alignof(compressed_database));
		std::memset(database_buffer, 0, database_buffer_size);

		const uint8_t* database_buffer_start = database_buffer;
		out_merged_compressed_database = reinterpret_cast<compressed_database*>(database_buffer);

		raw_buffer_header* database_buffer_header = safe_ptr_cast<raw_buffer_header>(database_buffer);
		database_buffer += sizeof(raw_buffer_header);

		uint8_t* db_header_start = database_buffer;
		database_header* db_header = safe_ptr_cast<database_header>(database_buffer);
		database_buffer += sizeof(database_header);

		// Write our header
		db_header->tag = static_cast<uint32_t>(buffer_tag32::compressed_database);
		db_header->version = compressed_tracks_version16::latest;
		db_header->num_chunks = db_metadata.num_chunks;
		db_header->max_chunk_size = settings.max_chunk_size;
		db_header->num_clips = db_metadata.num_clips;
		db_header->num_segments = db_metadata.num_segments;
		db_header->bulk_data_size = bulk_data_size;
		db_header->set_is_bulk_data_inline(true);	// Data is always inline when merging

		database_buffer = align_to(database_buffer, 4);									// Align chunk descriptions
		database_buffer += db_metadata.num_chunks * sizeof(database_chunk_description);	// Chunk descriptions

		database_buffer = align_to(database_buffer, 4);									// Align clip hashes
		db_header->clip_metadata_offset = database_buffer - db_header_start;			// Clip metadata
		database_buffer += db_metadata.num_clips * sizeof(database_clip_metadata);		// Clip metadata

		database_buffer = align_to(database_buffer, 8);									// Align bulk data
		if (bulk_data_size != 0)
			db_header->bulk_data_offset = database_buffer - db_header_start;			// Bulk data
		else
			db_header->bulk_data_offset = invalid_ptr_offset();
		database_buffer += bulk_data_size;												// Bulk data

		// Write our chunk descriptions
		const uint32_t num_written_chunks = write_database_chunk_descriptions(settings, merge_mappings, num_merge_mappings, db_header->get_chunk_descriptions());
		ACL_ASSERT(num_written_chunks == db_metadata.num_chunks, "Unexpected amount of data written"); (void)num_written_chunks;

		// Write our clip metadata
		write_database_clip_metadata(merge_mappings, num_merge_mappings, db_header->get_clip_metadatas());

		// Write our bulk data
		const uint32_t written_bulk_data_size = write_database_bulk_data(allocator, settings, db_metadata, merge_mappings, num_merge_mappings, db_header->get_bulk_data());
		ACL_ASSERT(written_bulk_data_size == bulk_data_size, "Unexpected amount of data written"); (void)written_bulk_data_size;
		db_header->bulk_data_hash = hash32(db_header->get_bulk_data(), bulk_data_size);

		ACL_ASSERT(uint32_t(database_buffer - database_buffer_start) == database_buffer_size, "Unexpected amount of data written"); (void)database_buffer_start;

#if defined(ACL_HAS_ASSERT_CHECKS)
		// Make sure nobody overwrote our padding (contained in last chunk if we have data)
		if (bulk_data_size != 0)
		{
			for (const uint8_t* padding = database_buffer - 15; padding < database_buffer; ++padding)
				ACL_ASSERT(*padding == 0, "Padding was overwritten");
		}
#endif

		// Finish the raw buffer header
		database_buffer_header->size = database_buffer_size;
		database_buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(db_header), database_buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

		return error_result();
	}
}
