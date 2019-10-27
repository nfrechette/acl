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
#include "acl/core/iterator.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/stream/track_stream.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	struct ClipContext;

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
		ClipContext* clip;
		BoneStreams* bone_streams;
		BoneRanges* ranges;

		uint16_t num_samples;
		uint16_t num_bones;

		uint32_t clip_sample_offset;
		uint32_t segment_index;

		SampleDistribution8 distribution;

		bool are_rotations_normalized;
		bool are_translations_normalized;
		bool are_scales_normalized;

		// Stat tracking
		uint32_t animated_pose_bit_size;
		uint32_t animated_data_size;
		uint32_t range_data_size;
		uint32_t total_header_size;

		//////////////////////////////////////////////////////////////////////////
		Iterator<BoneStreams> bone_iterator() { return Iterator<BoneStreams>(bone_streams, num_bones); }
		ConstIterator<BoneStreams> const_bone_iterator() const { return ConstIterator<BoneStreams>(bone_streams, num_bones); }
	};

	inline void destroy_segment_context(IAllocator& allocator, SegmentContext& segment)
	{
		deallocate_type_array(allocator, segment.bone_streams, segment.num_bones);
		deallocate_type_array(allocator, segment.ranges, segment.num_bones);
	}

	namespace acl_impl
	{
		class track_database;

		struct qvvf_ranges
		{
			Vector4_32 rotation_min;
			Vector4_32 rotation_max;
			Vector4_32 rotation_extent;

			Vector4_32 translation_min;
			Vector4_32 translation_max;
			Vector4_32 translation_extent;

			Vector4_32 scale_min;
			Vector4_32 scale_max;
			Vector4_32 scale_extent;

			bool is_rotation_constant;
			bool is_rotation_default;

			bool is_translation_constant;
			bool is_translation_default;

			bool is_scale_constant;
			bool is_scale_default;

			bool are_rotations_normalized;
			bool are_translations_normalized;
			bool are_scales_normalized;
		};

		struct segment_context
		{
			qvvf_ranges* ranges;						// Range information for every track in this segment
			BoneBitRate* bit_rates;						// Quantization bit rates for every track in this segment

			uint32_t index;								// Which segment this is
			uint32_t num_transforms;					// Number of transforms (same in every segment)

			uint32_t start_offset;						// The offset of the first sample in the parent clip
			uint32_t num_samples_per_track;				// How many samples are in this segment per track

			uint32_t num_simd_samples_per_track;		// The number of samples per track rounded up to padded SIMD width
			uint32_t num_soa_entries;					// Number of SOA vector entries per component (num simd samples per track / simd width)
			uint32_t soa_size;							// The size in bytes of the segment data in SOA form
			uint32_t soa_start_offset;					// The start offset in bytes of the segment data in SOA form relative to the start of the contiguous buffer
			uint32_t soa_transform_size;				// Size in bytes of each transform track in SOA form

			// Offset to each transform track's data
			uint32_t rotations_offset;					// Always zero, here for symmetry but can be ignored
			uint32_t translations_offset;
			uint32_t scales_offset;

			// Offset of each component within a track, fixed for all tracks since they each have the same number of samples and component size
			uint32_t samples_offset_x;					// Always zero, here for symmetry but can be ignored
			uint32_t samples_offset_y;
			uint32_t samples_offset_z;
			uint32_t samples_offset_w;

			SampleDistribution8 distribution;

			uint32_t format_per_track_data_size;
			uint32_t range_data_size;
			uint32_t animated_data_size;
			uint32_t animated_pose_bit_size;
			uint32_t total_header_size;					// Size of the segment header: metadata + range info
			uint32_t total_size;						// Size of the segment
		};

		inline void destroy_segments(IAllocator& allocator, segment_context* segments, uint32_t num_segments)
		{
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				segment_context& segment = segments[segment_index];
				deallocate_type_array(allocator, segment.ranges, segment.num_transforms);
				deallocate_type_array(allocator, segment.bit_rates, segment.num_transforms);
			}

			deallocate_type_array(allocator, segments, num_segments);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
