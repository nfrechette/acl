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
#include "acl/core/compressed_clip.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/segment_context.h"
#include "acl/compression/impl/write_range_data.h"
#include "acl/compression/impl/write_stream_data.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline void write_segment_start_indices(const ClipContext& clip_context, uint32_t* segment_start_indices)
		{
			for (uint16_t segment_index = 0; segment_index < clip_context.num_segments; ++segment_index)
			{
				const SegmentContext& segment = clip_context.segments[segment_index];
				segment_start_indices[segment_index] = segment.clip_sample_offset;
			}

			// Write our sentinel value
			segment_start_indices[clip_context.num_segments] = 0xFFFFFFFFU;
		}

		inline void write_segment_headers(const ClipContext& clip_context, const CompressionSettings& settings, SegmentHeader* segment_headers, uint32_t segment_data_start_offset)
		{
			const uint32_t format_per_track_data_size = get_format_per_track_data_size(clip_context, settings.rotation_format, settings.translation_format, settings.scale_format);

			uint32_t segment_data_offset = segment_data_start_offset;
			for (uint16_t segment_index = 0; segment_index < clip_context.num_segments; ++segment_index)
			{
				const SegmentContext& segment = clip_context.segments[segment_index];
				SegmentHeader& header = segment_headers[segment_index];

				header.animated_pose_bit_size = segment.animated_pose_bit_size;
				header.format_per_track_data_offset = segment_data_offset;
				header.range_data_offset = align_to(header.format_per_track_data_offset + format_per_track_data_size, 2);		// Aligned to 2 bytes
				header.track_data_offset = align_to(header.range_data_offset + segment.range_data_size, 4);						// Aligned to 4 bytes

				segment_data_offset = header.track_data_offset + segment.animated_data_size;
			}
		}

		inline void write_segment_data(const ClipContext& clip_context, const CompressionSettings& settings, ClipHeader& header, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
		{
			SegmentHeader* segment_headers = header.get_segment_headers();
			const uint32_t format_per_track_data_size = get_format_per_track_data_size(clip_context, settings.rotation_format, settings.translation_format, settings.scale_format);

			for (uint16_t segment_index = 0; segment_index < clip_context.num_segments; ++segment_index)
			{
				const SegmentContext& segment = clip_context.segments[segment_index];
				SegmentHeader& segment_header = segment_headers[segment_index];

				if (format_per_track_data_size > 0)
					write_format_per_track_data(segment, header.get_format_per_track_data(segment_header), format_per_track_data_size, output_bone_mapping, num_output_bones);
				else
					segment_header.format_per_track_data_offset = InvalidPtrOffset();

				if (segment.range_data_size > 0)
					write_segment_range_data(segment, settings.segmenting.range_reduction, header.get_segment_range_data(segment_header), segment.range_data_size, output_bone_mapping, num_output_bones);
				else
					segment_header.range_data_offset = InvalidPtrOffset();

				if (segment.animated_data_size > 0)
					write_animated_track_data(segment, header.get_track_data(segment_header), segment.animated_data_size, output_bone_mapping, num_output_bones);
				else
					segment_header.track_data_offset = InvalidPtrOffset();
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
