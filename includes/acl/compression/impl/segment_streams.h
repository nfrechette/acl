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
#include "acl/compression/compression_settings.h"
#include "acl/compression/impl/clip_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline void segment_streams(iallocator& allocator, clip_context& clip, const compression_segmenting_settings& settings)
		{
			ACL_ASSERT(clip.num_segments == 1, "clip_context must have a single segment.");
			ACL_ASSERT(settings.ideal_num_samples <= settings.max_num_samples, "Invalid num samples for segmenting settings. %u > %u", settings.ideal_num_samples, settings.max_num_samples);

			if (clip.num_samples <= settings.max_num_samples)
				return;

			//////////////////////////////////////////////////////////////////////////
			// This algorithm is simple in nature. Its primary aim is to avoid having
			// the last segment being partial if multiple segments are present.
			// The extra samples from the last segment will be redistributed evenly
			// starting with the first segment.
			// As such, in order to quickly find which segment contains a particular sample
			// you can simply divide the number of samples by the number of segments to get
			// the floored value of number of samples per segment. This guarantees an accurate estimate.
			// You can then query the segment start index by dividing the desired sample index
			// with the floored value. If the sample isn't in the current segment, it will live in one of its neighbors.
			// TODO: Can we provide a tighter guarantee?
			//////////////////////////////////////////////////////////////////////////

			uint32_t num_segments = (clip.num_samples + settings.ideal_num_samples - 1) / settings.ideal_num_samples;
			const uint32_t max_num_samples = num_segments * settings.ideal_num_samples;

			const uint32_t original_num_segments = num_segments;
			uint32_t* num_samples_per_segment = allocate_type_array<uint32_t>(allocator, num_segments);
			std::fill(num_samples_per_segment, num_samples_per_segment + num_segments, settings.ideal_num_samples);

			const uint32_t num_leftover_samples = settings.ideal_num_samples - (max_num_samples - clip.num_samples);
			if (num_leftover_samples != 0)
				num_samples_per_segment[num_segments - 1] = num_leftover_samples;

			const uint32_t slack = settings.max_num_samples - settings.ideal_num_samples;
			if ((num_segments - 1) * slack >= num_leftover_samples)
			{
				// Enough segments to distribute the leftover samples of the last segment
				while (num_samples_per_segment[num_segments - 1] != 0)
				{
					for (uint32_t segment_index = 0; segment_index < num_segments - 1 && num_samples_per_segment[num_segments - 1] != 0; ++segment_index)
					{
						num_samples_per_segment[segment_index]++;
						num_samples_per_segment[num_segments - 1]--;
					}
				}

				num_segments--;
			}

			ACL_ASSERT(num_segments != 1, "Expected a number of segments greater than 1.");

			segment_context* clip_segment = clip.segments;
			clip.segments = allocate_type_array<segment_context>(allocator, num_segments);
			clip.num_segments = num_segments;

			uint32_t clip_sample_index = 0;
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				const uint32_t num_samples_in_segment = num_samples_per_segment[segment_index];

				segment_context& segment = clip.segments[segment_index];
				segment.clip = &clip;
				segment.bone_streams = allocate_type_array<transform_streams>(allocator, clip.num_bones);
				segment.ranges = nullptr;
				segment.contributing_error = nullptr;
				segment.num_bones = clip.num_bones;
				segment.num_samples = num_samples_in_segment;
				segment.clip_sample_offset = clip_sample_index;
				segment.segment_index = segment_index;
				segment.are_rotations_normalized = false;
				segment.are_translations_normalized = false;
				segment.are_scales_normalized = false;
				segment.animated_rotation_bit_size = 0;
				segment.animated_translation_bit_size = 0;
				segment.animated_scale_bit_size = 0;
				segment.animated_pose_bit_size = 0;
				segment.animated_data_size = 0;
				segment.range_data_size = 0;
				segment.total_header_size = 0;

				for (uint32_t bone_index = 0; bone_index < clip.num_bones; ++bone_index)
				{
					const transform_streams& clip_bone_stream = clip_segment->bone_streams[bone_index];
					transform_streams& segment_bone_stream = segment.bone_streams[bone_index];

					segment_bone_stream.segment = &segment;
					segment_bone_stream.bone_index = bone_index;
					segment_bone_stream.parent_bone_index = clip_bone_stream.parent_bone_index;
					segment_bone_stream.output_index = clip_bone_stream.output_index;

					if (clip_bone_stream.is_rotation_constant)
					{
						segment_bone_stream.rotations = clip_bone_stream.rotations.duplicate();
					}
					else
					{
						const uint32_t sample_size = clip_bone_stream.rotations.get_sample_size();
						rotation_track_stream rotations(allocator, num_samples_in_segment, sample_size, clip_bone_stream.rotations.get_sample_rate(), clip_bone_stream.rotations.get_rotation_format(), clip_bone_stream.rotations.get_bit_rate());
						std::memcpy(rotations.get_raw_sample_ptr(0), clip_bone_stream.rotations.get_raw_sample_ptr(clip_sample_index), size_t(num_samples_in_segment) * sample_size);

						segment_bone_stream.rotations = std::move(rotations);
					}

					if (clip_bone_stream.is_translation_constant)
					{
						segment_bone_stream.translations = clip_bone_stream.translations.duplicate();
					}
					else
					{
						const uint32_t sample_size = clip_bone_stream.translations.get_sample_size();
						translation_track_stream translations(allocator, num_samples_in_segment, sample_size, clip_bone_stream.translations.get_sample_rate(), clip_bone_stream.translations.get_vector_format(), clip_bone_stream.translations.get_bit_rate());
						std::memcpy(translations.get_raw_sample_ptr(0), clip_bone_stream.translations.get_raw_sample_ptr(clip_sample_index), size_t(num_samples_in_segment) * sample_size);

						segment_bone_stream.translations = std::move(translations);
					}

					if (clip_bone_stream.is_scale_constant)
					{
						segment_bone_stream.scales = clip_bone_stream.scales.duplicate();
					}
					else
					{
						const uint32_t sample_size = clip_bone_stream.scales.get_sample_size();
						scale_track_stream scales(allocator, num_samples_in_segment, sample_size, clip_bone_stream.scales.get_sample_rate(), clip_bone_stream.scales.get_vector_format(), clip_bone_stream.scales.get_bit_rate());
						std::memcpy(scales.get_raw_sample_ptr(0), clip_bone_stream.scales.get_raw_sample_ptr(clip_sample_index), size_t(num_samples_in_segment) * sample_size);

						segment_bone_stream.scales = std::move(scales);
					}

					segment_bone_stream.is_rotation_constant = clip_bone_stream.is_rotation_constant;
					segment_bone_stream.is_rotation_default = clip_bone_stream.is_rotation_default;
					segment_bone_stream.is_translation_constant = clip_bone_stream.is_translation_constant;
					segment_bone_stream.is_translation_default = clip_bone_stream.is_translation_default;
					segment_bone_stream.is_scale_constant = clip_bone_stream.is_scale_constant;
					segment_bone_stream.is_scale_default = clip_bone_stream.is_scale_default;
				}

				clip_sample_index += num_samples_in_segment;
			}

			deallocate_type_array(allocator, num_samples_per_segment, original_num_segments);
			destroy_segment_context(allocator, *clip_segment);
			deallocate_type_array(allocator, clip_segment, 1);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
