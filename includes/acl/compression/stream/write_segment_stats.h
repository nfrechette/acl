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

#include "acl/compression/stream/clip_context.h"
#include "acl/compression/skeleton_error_metric.h"
#include "acl/sjson/sjson_writer.h"

namespace acl
{
	inline void write_summary_segment_stats(const SegmentContext& segment, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, SJSONObjectWriter& writer)
	{
		writer["segment_index"] = segment.segment_index;
		writer["num_samples"] = segment.num_samples;

		const uint32_t format_per_track_data_size = get_format_per_track_data_size(*segment.clip, rotation_format, translation_format, scale_format);

		uint32_t segment_size = 0;
		segment_size += format_per_track_data_size;						// Format per track data
		segment_size = align_to(segment_size, 2);						// Align range data
		segment_size += segment.range_data_size;						// Range data
		segment_size = align_to(segment_size, 4);						// Align animated data
		segment_size += segment.animated_data_size;						// Animated track data

		writer["segment_size"] = segment_size;
		writer["animated_frame_size"] = float(segment.animated_data_size) / float(segment.num_samples);
	}

	inline void write_detailed_segment_stats(const SegmentContext& segment, SJSONObjectWriter& writer)
	{
		uint32_t bit_rate_counts[NUM_BIT_RATES] = {0};

		for (const BoneStreams& bone_stream : segment.bone_iterator())
		{
			const uint8_t rotation_bit_rate = bone_stream.rotations.get_bit_rate();
			if (rotation_bit_rate != INVALID_BIT_RATE)
				bit_rate_counts[rotation_bit_rate]++;

			const uint8_t translation_bit_rate = bone_stream.translations.get_bit_rate();
			if (translation_bit_rate != INVALID_BIT_RATE)
				bit_rate_counts[translation_bit_rate]++;

			const uint8_t scale_bit_rate = bone_stream.scales.get_bit_rate();
			if (scale_bit_rate != INVALID_BIT_RATE)
				bit_rate_counts[scale_bit_rate]++;
		}

		writer["bit_rate_counts"] = [&](SJSONArrayWriter& writer)
		{
			for (uint8_t bit_rate = 0; bit_rate < NUM_BIT_RATES; ++bit_rate)
				writer.push_value(bit_rate_counts[bit_rate]);
		};
	}

	inline void write_exhaustive_segment_stats(Allocator& allocator, const SegmentContext& segment, const ClipContext& raw_clip_context, const RigidSkeleton& skeleton, SJSONObjectWriter& writer)
	{
		const uint16_t num_bones = skeleton.get_num_bones();
		const bool has_scale = segment_context_has_scale(segment);

		Transform_32* raw_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);
		Transform_32* lossy_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);

		const float sample_rate = float(raw_clip_context.segments[0].bone_streams[0].rotations.get_sample_rate());
		const float ref_duration = float(raw_clip_context.num_samples - 1) / sample_rate;

		const float segment_duration = float(segment.num_samples - 1) / sample_rate;

		BoneError worst_bone_error = { INVALID_BONE_INDEX, 0.0f, 0.0f };

		writer["error_per_frame_and_bone"] = [&](SJSONArrayWriter& writer)
		{
			for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
			{
				const float sample_time = min(float(sample_index) / sample_rate, segment_duration);
				const float ref_sample_time = min(float(segment.clip_sample_offset + sample_index) / sample_rate, ref_duration);

				sample_streams(raw_clip_context.segments[0].bone_streams, num_bones, ref_sample_time, raw_local_pose);
				sample_streams(segment.bone_streams, num_bones, sample_time, lossy_local_pose);

				writer.push_newline();
				writer.push_array([&](SJSONArrayWriter& writer)
				{
					for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
					{
						float error;
						if (has_scale)
							error = calculate_object_bone_error(skeleton, raw_local_pose, lossy_local_pose, bone_index);
						else
							error = calculate_object_bone_error_no_scale(skeleton, raw_local_pose, lossy_local_pose, bone_index);
						writer.push_value(error);

						if (error > worst_bone_error.error)
						{
							worst_bone_error.error = error;
							worst_bone_error.index = bone_index;
							worst_bone_error.sample_time = sample_time;
						}
					}
				});
			}
		};

		writer["max_error"] = worst_bone_error.error;
		writer["worst_bone"] = worst_bone_error.index;
		writer["worst_time"] = worst_bone_error.sample_time;

		deallocate_type_array(allocator, raw_local_pose, num_bones);
		deallocate_type_array(allocator, lossy_local_pose, num_bones);
	}
}
