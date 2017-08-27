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
	inline void write_stream_stats(Allocator& allocator, const ClipContext& clip_context, const ClipContext& raw_clip_context, const RigidSkeleton& skeleton, SJSONObjectWriter& writer)
	{
		uint16_t num_bones = skeleton.get_num_bones();

		Transform_32* raw_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);
		Transform_32* lossy_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);

		float sample_rate = float(raw_clip_context.segments[0].bone_streams[0].rotations.get_sample_rate());
		float ref_duration = float(raw_clip_context.num_samples - 1) / sample_rate;

		writer["segments"] = [&](SJSONArrayWriter& writer)
		{
			for (const SegmentContext& segment : clip_context.segment_iterator())
			{
				float segment_duration = float(segment.num_samples - 1) / sample_rate;

				writer.push_object([&](SJSONObjectWriter& writer)
				{
					BoneError bone_error = { INVALID_BONE_INDEX, 0.0f, 0.0f };

					writer["segment_index"] = segment.segment_index;
					writer["error_per_frame_and_bone"] = [&](SJSONArrayWriter& writer)
					{
						for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
						{
							float sample_time = min(float(sample_index) / sample_rate, segment_duration);
							float ref_sample_time = min(float(segment.clip_sample_offset + sample_index) / sample_rate, ref_duration);

							sample_streams(raw_clip_context.segments[0].bone_streams, num_bones, ref_sample_time, raw_local_pose);
							sample_streams(segment.bone_streams, num_bones, sample_time, lossy_local_pose);

							writer.push_newline();
							writer.push_array([&](SJSONArrayWriter& writer)
							{
								for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
								{
									float error = calculate_object_bone_error(skeleton, raw_local_pose, lossy_local_pose, bone_index);
									writer.push_value(error);

									if (error > bone_error.error)
									{
										bone_error.error = error;
										bone_error.index = bone_index;
										bone_error.sample_time = sample_time;
									}
								}
							});
						}
					};

					writer["max_error"] = bone_error.error;
					writer["worst_bone"] = bone_error.index;
					writer["worst_time"] = bone_error.sample_time;
				});
			}
		};

		deallocate_type_array(allocator, raw_local_pose, num_bones);
		deallocate_type_array(allocator, lossy_local_pose, num_bones);
	}
}
