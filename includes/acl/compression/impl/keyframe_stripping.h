#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2023 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/compression_settings.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		struct clip_frame_contributing_error_t
		{
			uint32_t segment_index;
			frame_contributing_error contributing_error;
		};

		inline void strip_keyframes(iallocator& allocator, clip_context& lossy_clip_context, const compression_settings& settings)
		{
			if (!settings.keyframe_stripping.is_enabled())
				return;	// We don't want to strip keyframes, nothing to do

			const bitset_description hard_keyframes_desc = bitset_description::make_from_num_bits<32>();
			const uint32_t num_keyframes = lossy_clip_context.num_samples;

			clip_frame_contributing_error_t* contributing_error_per_keyframe = allocate_type_array<clip_frame_contributing_error_t>(allocator, num_keyframes);

			for (segment_context& segment : lossy_clip_context.segment_iterator())
			{
				// Copy the contributing error of each keyframe, we'll sort them later
				for (uint32_t keyframe_index = 0; keyframe_index < segment.num_samples; ++keyframe_index)
					contributing_error_per_keyframe[segment.clip_sample_offset + keyframe_index] = { segment.segment_index, segment.contributing_error[keyframe_index] };

				// Initialize which keyframes are retained, we'll strip them later
				bitset_set_range(&segment.hard_keyframes, hard_keyframes_desc, 0, segment.num_samples, true);
			}

			// Sort the contributing error of every keyframe within the clip
			auto sort_predicate = [](const clip_frame_contributing_error_t& lhs, const clip_frame_contributing_error_t& rhs) { return lhs.contributing_error.error < rhs.contributing_error.error; };
			std::sort(contributing_error_per_keyframe, contributing_error_per_keyframe + num_keyframes, sort_predicate);

			// First determine how many we wish to strip based on proportion
			// A frame is movable if it isn't the first or last frame of a segment
			// If we have more than 1 frame, we can remove 2 frames per segment
			// We know that the only way to get a segment with 1 frame is if the whole clip contains
			// a single frame and thus has one segment
			// If we have 0 or 1 frame, none are movable
			const uint32_t num_movable_frames = num_keyframes >= 2 ? (num_keyframes - (lossy_clip_context.num_segments * 2)) : 0;

			// First estimate how many keyframes to strip using the desired minimum proportion to strip
			uint32_t num_keyframes_to_strip = std::min<uint32_t>(num_movable_frames, uint32_t(settings.keyframe_stripping.proportion * float(num_keyframes)));

			// Then scan starting until we find our threshold if its above the proportion
			for (; num_keyframes_to_strip < num_movable_frames; ++num_keyframes_to_strip)
			{
				if (contributing_error_per_keyframe[num_keyframes_to_strip].contributing_error.error > settings.keyframe_stripping.threshold)
					break;	// The error exceeds our threshold, stop stripping keyframes
			}

			ACL_ASSERT(num_keyframes_to_strip <= num_movable_frames, "Cannot strip more than the number of movable keyframes");

			// Now that we know how many keyframes to strip, remove them
			for (uint32_t index = 0; index < num_keyframes_to_strip; ++index)
			{
				const clip_frame_contributing_error_t& contributing_error = contributing_error_per_keyframe[index];

				segment_context& keyframe_segment = lossy_clip_context.segments[contributing_error.segment_index];

				const uint32_t segment_keyframe_index = contributing_error.contributing_error.index;
				ACL_ASSERT(segment_keyframe_index != 0 && segment_keyframe_index < (keyframe_segment.num_samples - 1), "Cannot strip the first and last sample of a segment");

				bitset_set(&keyframe_segment.hard_keyframes, hard_keyframes_desc, segment_keyframe_index, false);
			}

			deallocate_type_array(allocator, contributing_error_per_keyframe, num_keyframes);

			lossy_clip_context.has_stripped_keyframes = num_keyframes_to_strip != 0;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
