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

#include "acl/core/memory.h"
#include "acl/core/error.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/stream/segment_context.h"

#include <stdint.h>

namespace acl
{
	struct ClipContext
	{
		SegmentContext* segments;
		BoneRanges* ranges;

		uint16_t num_segments;
		uint16_t num_bones;
		uint32_t num_samples;
		uint32_t sample_rate;

		float error_threshold;

		bool are_rotations_normalized;
		bool are_translations_normalized;

		//////////////////////////////////////////////////////////////////////////

		struct SegmentIterator
		{
			constexpr SegmentIterator(SegmentContext* segments_, uint16_t num_segments_) : segments(segments_), num_segments(num_segments_) {}

			SegmentContext* begin() { return segments; }
			SegmentContext* end() { return segments + num_segments; }

			SegmentContext* segments;
			uint16_t num_segments;
		};

		constexpr SegmentIterator segment_iterator() const { return SegmentIterator(segments, num_segments); }
	};

	inline void initialize_clip_context(Allocator& allocator, const AnimationClip& clip, ClipContext& out_clip_context)
	{
		uint16_t num_bones = clip.get_num_bones();
		uint32_t num_samples = clip.get_num_samples();
		uint32_t sample_rate = clip.get_sample_rate();
		const AnimatedBone* bones = clip.get_bones();

		ACL_ENSURE(num_bones > 0, "Clip has no bones!");
		ACL_ENSURE(num_samples > 0, "Clip has no samples!");

		// Create a single segment with the whole clip
		out_clip_context.segments = allocate_type_array<SegmentContext>(allocator, 1);
		out_clip_context.ranges = nullptr;
		out_clip_context.num_segments = 1;
		out_clip_context.num_bones = num_bones;
		out_clip_context.num_samples = num_samples;
		out_clip_context.sample_rate = sample_rate;
		out_clip_context.error_threshold = clip.get_error_threshold();
		out_clip_context.are_rotations_normalized = false;
		out_clip_context.are_translations_normalized = false;

		SegmentContext& segment = out_clip_context.segments[0];

		BoneStreams* bone_streams = allocate_type_array<BoneStreams>(allocator, num_bones);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const AnimatedBone& bone = bones[bone_index];
			BoneStreams& bone_stream = bone_streams[bone_index];

			bone_stream.segment = &segment;
			bone_stream.bone_index = bone_index;

			bone_stream.rotations = RotationTrackStream(allocator, num_samples, sizeof(Quat_32), sample_rate, RotationFormat8::Quat_128);
			bone_stream.translations = TranslationTrackStream(allocator, num_samples, sizeof(Vector4_32), sample_rate, VectorFormat8::Vector3_96);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Quat_32 rotation = quat_normalize(quat_cast(bone.rotation_track.get_sample(sample_index)));
				bone_stream.rotations.set_raw_sample(sample_index, rotation);

				Vector4_32 translation = vector_cast(bone.translation_track.get_sample(sample_index));
				bone_stream.translations.set_raw_sample(sample_index, translation);
			}

			bone_stream.is_rotation_constant = num_samples == 1;
			bone_stream.is_rotation_default = bone_stream.is_rotation_constant && quat_near_identity(quat_cast(bone.rotation_track.get_sample(0)));
			bone_stream.is_translation_constant = num_samples == 1;
			bone_stream.is_translation_default = bone_stream.is_translation_constant && vector_near_equal(vector_cast(bone.translation_track.get_sample(0)), vector_zero_32());
		}

		segment.bone_streams = bone_streams;
		segment.clip = &out_clip_context;
		segment.ranges = nullptr;
		segment.num_samples = num_samples;
		segment.num_bones = num_bones;
		segment.clip_sample_offset = 0;
		segment.animated_pose_bit_size = 0;
		segment.animated_data_size = 0;
		segment.range_data_size = 0;
		segment.segment_index = 0;
		segment.are_rotations_normalized = false;
		segment.are_translations_normalized = false;
	}

	inline void destroy_clip_context(Allocator& allocator, ClipContext& clip_context)
	{
		for (SegmentContext& segment : clip_context.segment_iterator())
			deallocate_type_array(allocator, segment.bone_streams, segment.num_bones);

		deallocate_type_array(allocator, clip_context.segments, clip_context.num_segments);
	}
}
