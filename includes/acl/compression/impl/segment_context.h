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
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/hash.h"
#include "acl/core/iterator.h"
#include "acl/compression/impl/track_stream.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		struct clip_context;

		//////////////////////////////////////////////////////////////////////////
		// The sample distribution.
		//////////////////////////////////////////////////////////////////////////
		enum class SampleDistribution8 : uint8_t
		{
			// Samples are uniform, use the whole clip to determine the interpolation alpha.
			Uniform,

			// Samples are not uniform, use each track to determine the interpolation alpha.
			Variable,
		};

		struct SegmentContext
		{
			clip_context* clip;
			BoneStreams* bone_streams;
			BoneRanges* ranges;

			uint32_t num_samples;
			uint32_t num_bones;

			uint32_t clip_sample_offset;
			uint32_t segment_index;

			SampleDistribution8 distribution;

			bool are_rotations_normalized;
			bool are_translations_normalized;
			bool are_scales_normalized;

			// Stat tracking
			uint32_t animated_pose_rotation_bit_size;
			uint32_t animated_pose_translation_bit_size;
			uint32_t animated_pose_scale_bit_size;
			uint32_t animated_pose_bit_size;
			uint32_t animated_data_size;
			uint32_t range_data_size;
			uint32_t segment_data_size;
			uint32_t total_header_size;

			//////////////////////////////////////////////////////////////////////////
			iterator<BoneStreams> bone_iterator() { return iterator<BoneStreams>(bone_streams, num_bones); }
			const_iterator<BoneStreams> const_bone_iterator() const { return const_iterator<BoneStreams>(bone_streams, num_bones); }
		};

		inline void destroy_segment_context(iallocator& allocator, SegmentContext& segment)
		{
			deallocate_type_array(allocator, segment.bone_streams, segment.num_bones);
			deallocate_type_array(allocator, segment.ranges, segment.num_bones);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
