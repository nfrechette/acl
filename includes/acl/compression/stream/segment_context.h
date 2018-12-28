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

#include "acl/core/iallocator.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/hash.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/stream/track_stream.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	struct ClipContext;

	struct SegmentContext
	{
		ClipContext* clip;
		BoneStreams* bone_streams;
		BoneRanges* ranges;

		uint16_t num_samples;
		uint16_t num_bones;

		uint32_t clip_sample_offset;
		uint32_t segment_index;

		bool are_rotations_normalized;
		bool are_translations_normalized;
		bool are_scales_normalized;

		// Stat tracking
		uint32_t animated_pose_bit_size;
		uint32_t animated_data_size;
		uint32_t range_data_size;
		uint32_t total_header_size;

		//////////////////////////////////////////////////////////////////////////

		struct BoneIterator
		{
			constexpr BoneIterator(BoneStreams* bone_streams_, uint16_t num_bones_) : bone_streams(bone_streams_), num_bones(num_bones_) {}

			BoneStreams* begin() { return bone_streams; }
			BoneStreams* end() { return bone_streams + num_bones; }

			BoneStreams* bone_streams;
			uint16_t num_bones;
		};

		constexpr BoneIterator bone_iterator() const { return BoneIterator(bone_streams, num_bones); }
	};

	inline void destroy_segment_context(IAllocator& allocator, SegmentContext& segment)
	{
		deallocate_type_array(allocator, segment.bone_streams, segment.num_bones);
		deallocate_type_array(allocator, segment.ranges, segment.num_bones);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
