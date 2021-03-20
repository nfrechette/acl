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

#include "acl/core/error.h"
#include "acl/core/hash.h"
#include "acl/core/iallocator.h"
#include "acl/core/iterator.h"
#include "acl/core/quality_tiers.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/impl/compressed_headers.h"
#include "acl/compression/impl/track_stream.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		struct clip_context;

		struct segment_context
		{
			clip_context* clip;
			transform_streams* bone_streams;
			BoneRanges* ranges;
			frame_contributing_error* contributing_error;	// Optional if we request it in the compression settings

			uint32_t num_samples;
			uint32_t num_bones;

			uint32_t clip_sample_offset;
			uint32_t segment_index;

			bool are_rotations_normalized;
			bool are_translations_normalized;
			bool are_scales_normalized;

			// Stat tracking
			uint32_t animated_rotation_bit_size;		// Tier 0
			uint32_t animated_translation_bit_size;		// Tier 0
			uint32_t animated_scale_bit_size;			// Tier 0
			uint32_t animated_pose_bit_size;			// Tier 0
			uint32_t animated_data_size;				// Tier 0
			uint32_t range_data_size;
			uint32_t segment_data_size;
			uint32_t total_header_size;

			//////////////////////////////////////////////////////////////////////////
			iterator<transform_streams> bone_iterator() { return iterator<transform_streams>(bone_streams, num_bones); }
			const_iterator<transform_streams> const_bone_iterator() const { return const_iterator<transform_streams>(bone_streams, num_bones); }
		};

		inline void destroy_segment_context(iallocator& allocator, segment_context& segment)
		{
			deallocate_type_array(allocator, segment.bone_streams, segment.num_bones);
			deallocate_type_array(allocator, segment.ranges, segment.num_bones);
			deallocate_type_array(allocator, segment.contributing_error, segment.num_samples);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
