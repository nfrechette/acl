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

#include "acl/core/error.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/segment_context.h"

#include <cstdint>
#include <functional>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline void get_num_sub_tracks(const SegmentContext& segment,
			const std::function<bool(animation_track_type8 group_type, uint32_t bone_index)>& group_filter_action,
			uint32_t& out_num_rotation_sub_tracks, uint32_t& out_num_translation_sub_tracks, uint32_t& out_num_scale_sub_tracks)
		{
			uint32_t num_rotation_sub_tracks = 0;
			uint32_t num_translation_sub_tracks = 0;
			uint32_t num_scale_sub_tracks = 0;
			for (uint32_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];
				if (bone_stream.output_index == k_invalid_track_index)
					continue;	// Stripped

				if (group_filter_action(animation_track_type8::rotation, bone_index))
					num_rotation_sub_tracks++;

				if (group_filter_action(animation_track_type8::translation, bone_index))
					num_translation_sub_tracks++;

				if (group_filter_action(animation_track_type8::scale, bone_index))
					num_scale_sub_tracks++;
			}

			out_num_rotation_sub_tracks = num_rotation_sub_tracks;
			out_num_translation_sub_tracks = num_translation_sub_tracks;
			out_num_scale_sub_tracks = num_scale_sub_tracks;
		}

		inline void get_num_animated_sub_tracks(const SegmentContext& segment,
			uint32_t& out_num_animated_rotation_sub_tracks, uint32_t& out_num_animated_translation_sub_tracks, uint32_t& out_num_animated_scale_sub_tracks)
		{
			const auto animated_group_filter_action = [&](animation_track_type8 group_type, uint32_t bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];
				if (group_type == animation_track_type8::rotation)
					return !bone_stream.is_rotation_constant;
				else if (group_type == animation_track_type8::translation)
					return !bone_stream.is_translation_constant;
				else
					return !bone_stream.is_scale_constant;
			};

			get_num_sub_tracks(segment, animated_group_filter_action, out_num_animated_rotation_sub_tracks, out_num_animated_translation_sub_tracks, out_num_animated_scale_sub_tracks);
		}

		inline animation_track_type8* calculate_sub_track_groups(const SegmentContext& segment, const uint32_t* output_bone_mapping, uint32_t num_output_bones, uint32_t& out_num_groups,
			const std::function<bool(animation_track_type8 group_type, uint32_t bone_index)>& group_filter_action)
		{
			uint32_t num_rotation_sub_tracks = 0;
			uint32_t num_translation_sub_tracks = 0;
			uint32_t num_scale_sub_tracks = 0;
			get_num_sub_tracks(segment, group_filter_action, num_rotation_sub_tracks, num_translation_sub_tracks, num_scale_sub_tracks);

			const uint32_t num_rotation_groups = (num_rotation_sub_tracks + 3) / 4;
			const uint32_t num_translation_groups = (num_translation_sub_tracks + 3) / 4;
			const uint32_t num_scale_groups = (num_scale_sub_tracks + 3) / 4;
			const uint32_t num_groups = num_rotation_groups + num_translation_groups + num_scale_groups;

			animation_track_type8* sub_track_groups = allocate_type_array<animation_track_type8>(*segment.clip->allocator, num_groups);
			std::memset(sub_track_groups, 0xFF, num_groups * sizeof(animation_track_type8));

			// Simulate reading in groups of 4
			uint32_t num_cached_rotations = 0;
			uint32_t num_left_rotations = num_rotation_sub_tracks;

			uint32_t num_cached_translations = 0;
			uint32_t num_left_translations = num_translation_sub_tracks;

			uint32_t num_cached_scales = 0;
			uint32_t num_left_scales = num_scale_sub_tracks;

			uint32_t current_group_index = 0;

			for (uint32_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				if ((output_index % 4) == 0)
				{
					if (num_cached_rotations < 4 && num_left_rotations != 0)
					{
						sub_track_groups[current_group_index++] = animation_track_type8::rotation;
						const uint32_t num_unpacked = std::min<uint32_t>(num_left_rotations, 4);
						num_left_rotations -= num_unpacked;
						num_cached_rotations += num_unpacked;
					}

					if (num_cached_translations < 4 && num_left_translations != 0)
					{
						sub_track_groups[current_group_index++] = animation_track_type8::translation;
						const uint32_t num_unpacked = std::min<uint32_t>(num_left_translations, 4);
						num_left_translations -= num_unpacked;
						num_cached_translations += num_unpacked;
					}

					if (num_cached_scales < 4 && num_left_scales != 0)
					{
						sub_track_groups[current_group_index++] = animation_track_type8::scale;
						const uint32_t num_unpacked = std::min<uint32_t>(num_left_scales, 4);
						num_left_scales -= num_unpacked;
						num_cached_scales += num_unpacked;
					}
				}

				const uint32_t bone_index = output_bone_mapping[output_index];

				if (group_filter_action(animation_track_type8::rotation, bone_index))
					num_cached_rotations--;		// Consumed

				if (group_filter_action(animation_track_type8::translation, bone_index))
					num_cached_translations--;	// Consumed

				if (group_filter_action(animation_track_type8::scale, bone_index))
					num_cached_scales--;		// Consumed
			}

			ACL_ASSERT(current_group_index == num_groups, "Unexpected number of groups written");

			out_num_groups = num_groups;
			return sub_track_groups;
		}

		inline void group_writer(const SegmentContext& segment, const uint32_t* output_bone_mapping, uint32_t num_output_bones,
			const std::function<bool(animation_track_type8 group_type, uint32_t bone_index)>& group_filter_action,
			const std::function<void(animation_track_type8 group_type, uint32_t group_size, uint32_t bone_index)>& group_entry_action,
			const std::function<void(animation_track_type8 group_type, uint32_t group_size)>& group_flush_action)
		{
			uint32_t num_groups = 0;
			animation_track_type8* sub_track_groups = calculate_sub_track_groups(segment, output_bone_mapping, num_output_bones, num_groups, group_filter_action);

			uint32_t group_size = 0;

			uint32_t rotation_output_index = 0;
			uint32_t translation_output_index = 0;
			uint32_t scale_output_index = 0;
			for (uint32_t group_index = 0; group_index < num_groups; ++group_index)
			{
				const animation_track_type8 group_type = sub_track_groups[group_index];

				if (group_type == animation_track_type8::rotation)
				{
					for (; group_size < 4 && rotation_output_index < num_output_bones; ++rotation_output_index)
					{
						const uint32_t bone_index = output_bone_mapping[rotation_output_index];

						if (group_filter_action(animation_track_type8::rotation, bone_index))
							group_entry_action(group_type, group_size++, bone_index);
					}
				}
				else if (group_type == animation_track_type8::translation)
				{
					for (; group_size < 4 && translation_output_index < num_output_bones; ++translation_output_index)
					{
						const uint32_t bone_index = output_bone_mapping[translation_output_index];

						if (group_filter_action(animation_track_type8::translation, bone_index))
							group_entry_action(group_type, group_size++, bone_index);
					}
				}
				else // scale
				{
					for (; group_size < 4 && scale_output_index < num_output_bones; ++scale_output_index)
					{
						const uint32_t bone_index = output_bone_mapping[scale_output_index];

						if (group_filter_action(animation_track_type8::scale, bone_index))
							group_entry_action(group_type, group_size++, bone_index);
					}
				}

				ACL_ASSERT(group_size != 0, "Group cannot be empty");

				// Group full or we ran out of tracks, write it out and move onto to the next group
				group_flush_action(group_type, group_size);
				group_size = 0;
			}

			deallocate_type_array(*segment.clip->allocator, sub_track_groups, num_groups);
		}

		inline void animated_group_writer(const SegmentContext& segment, const uint32_t* output_bone_mapping, uint32_t num_output_bones,
			const std::function<bool(animation_track_type8 group_type, uint32_t bone_index)>& group_filter_action,
			const std::function<void(animation_track_type8 group_type, uint32_t group_size, uint32_t bone_index)>& group_entry_action,
			const std::function<void(animation_track_type8 group_type, uint32_t group_size)>& group_flush_action)
		{
			const auto animated_group_filter_action = [&](animation_track_type8 group_type, uint32_t bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];
				if (group_type == animation_track_type8::rotation)
					return !bone_stream.is_rotation_constant && group_filter_action(group_type, bone_index);
				else if (group_type == animation_track_type8::translation)
					return !bone_stream.is_translation_constant && group_filter_action(group_type, bone_index);
				else
					return !bone_stream.is_scale_constant && group_filter_action(group_type, bone_index);
			};

			group_writer(segment, output_bone_mapping, num_output_bones, animated_group_filter_action, group_entry_action, group_flush_action);
		}

		inline void constant_group_writer(const SegmentContext& segment, const uint32_t* output_bone_mapping, uint32_t num_output_bones,
			const std::function<void(animation_track_type8 group_type, uint32_t group_size, uint32_t bone_index)>& group_entry_action,
			const std::function<void(animation_track_type8 group_type, uint32_t group_size)>& group_flush_action)
		{
			const auto constant_group_filter_action = [&](animation_track_type8 group_type, uint32_t bone_index)
			{
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];
				if (group_type == animation_track_type8::rotation)
					return !bone_stream.is_rotation_default && bone_stream.is_rotation_constant;
				else if (group_type == animation_track_type8::translation)
					return !bone_stream.is_translation_default && bone_stream.is_translation_constant;
				else
					return !bone_stream.is_scale_default && bone_stream.is_scale_constant;
			};

			group_writer(segment, output_bone_mapping, num_output_bones, constant_group_filter_action, group_entry_action, group_flush_action);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
