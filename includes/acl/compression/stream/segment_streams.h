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
#include "acl/compression/stream/clip_context.h"

#include <stdint.h>

namespace acl
{
	inline void segment_streams(Allocator& allocator, ClipContext& clip_context, const SegmentingSettings& settings)
	{
		if (!settings.enabled)
			return;

		ACL_ENSURE(clip_context.num_segments == 1, "ClipContext must have a single segment.");
		ACL_ENSURE(settings.ideal_num_samples <= settings.max_num_samples, "Invalid num samples for segmenting settings. %u > %u", settings.ideal_num_samples, settings.max_num_samples);

		if (clip_context.num_samples <= settings.max_num_samples)
			return;

		uint32_t num_segments = (clip_context.num_samples + settings.ideal_num_samples - 1) / settings.ideal_num_samples;
		uint32_t max_num_samples = num_segments * settings.ideal_num_samples;

		uint32_t original_num_segments = num_segments;
		uint32_t* num_samples_per_segment = allocate_type_array<uint32_t>(allocator, num_segments);
		std::fill(num_samples_per_segment, num_samples_per_segment + num_segments, settings.ideal_num_samples);

		uint32_t leftover_samples = settings.ideal_num_samples - (max_num_samples - clip_context.num_samples);
		if (leftover_samples != 0)
			num_samples_per_segment[num_segments - 1] = leftover_samples;

		uint32_t slack = settings.max_num_samples - settings.ideal_num_samples;
		if ((num_segments - 1) * slack >= leftover_samples)
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

		ACL_ENSURE(num_segments != 1, "Expected a number of segments greater than 1.");

		SegmentContext* clip_segment = clip_context.segments;
		clip_context.segments = allocate_type_array<SegmentContext>(allocator, num_segments);
		clip_context.num_segments = num_segments;

		uint32_t clip_sample_index = 0;
		for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
		{
			uint32_t num_samples_in_segment = num_samples_per_segment[segment_index];

			SegmentContext& segment = clip_context.segments[segment_index];
			segment.clip = &clip_context;
			segment.bone_streams = allocate_type_array<BoneStreams>(allocator, clip_context.num_bones);
			segment.num_bones = clip_context.num_bones;
			segment.num_samples = num_samples_in_segment;
			segment.clip_sample_offset = clip_sample_index;

			for (uint16_t bone_index = 0; bone_index < clip_context.num_bones; ++bone_index)
			{
				const BoneStreams& clip_bone_stream = clip_segment->bone_streams[bone_index];
				BoneStreams& segment_bone_stream = segment.bone_streams[bone_index];

				if (!clip_bone_stream.is_rotation_animated())
				{
					segment_bone_stream.rotations = clip_bone_stream.rotations.duplicate();
				}
				else
				{
					uint32_t sample_size = clip_bone_stream.rotations.get_sample_size();
					RotationTrackStream rotations(allocator, num_samples_in_segment, sample_size, clip_bone_stream.rotations.get_sample_rate(), clip_bone_stream.rotations.get_rotation_format(), clip_bone_stream.rotations.get_bit_rate());
					memcpy(rotations.get_raw_sample_ptr(0), clip_bone_stream.rotations.get_raw_sample_ptr(clip_sample_index), num_samples_in_segment * sample_size);

					segment_bone_stream.rotations = std::move(rotations);
				}

				if (!clip_bone_stream.is_translation_animated())
				{
					segment_bone_stream.translations = clip_bone_stream.translations.duplicate();
				}
				else
				{
					uint32_t sample_size = clip_bone_stream.translations.get_sample_size();
					TranslationTrackStream translations(allocator, num_samples_in_segment, sample_size, clip_bone_stream.translations.get_sample_rate(), clip_bone_stream.translations.get_vector_format(), clip_bone_stream.translations.get_bit_rate());
					memcpy(translations.get_raw_sample_ptr(0), clip_bone_stream.translations.get_raw_sample_ptr(clip_sample_index), num_samples_in_segment * sample_size);

					segment_bone_stream.translations = std::move(translations);
				}

#if 0
				Vector4_32 rotation_min = vector_set(1e10f);
				Vector4_32 rotation_max = vector_set(-1e10f);
				Vector4_32 translation_min = vector_set(1e10f);
				Vector4_32 translation_max = vector_set(-1e10f);

				for (uint32_t sample_index = 0; sample_index < segment_bone_stream.rotations.get_num_samples(); ++sample_index)
				{
					Quat_32 rotation = segment_bone_stream.rotations.get_raw_sample<Quat_32>(sample_index);

					rotation_min = vector_min(rotation_min, quat_to_vector(rotation));
					rotation_max = vector_max(rotation_max, quat_to_vector(rotation));
				}

				for (uint32_t sample_index = 0; sample_index < segment_bone_stream.translations.get_num_samples(); ++sample_index)
				{
					Vector4_32 translation = segment_bone_stream.translations.get_raw_sample<Vector4_32>(sample_index);

					translation_min = vector_min(translation_min, translation);
					translation_max = vector_max(translation_max, translation);
				}

				segment_bone_stream.rotation_range = TrackStreamRange(rotation_min, rotation_max);
				segment_bone_stream.translation_range = TrackStreamRange(translation_min, translation_max);
#endif

				segment_bone_stream.rotation_range = clip_bone_stream.rotation_range;
				segment_bone_stream.translation_range = clip_bone_stream.translation_range;

				segment_bone_stream.is_rotation_constant = clip_bone_stream.is_rotation_constant;
				segment_bone_stream.is_rotation_default = clip_bone_stream.is_rotation_default;
				segment_bone_stream.is_translation_constant = clip_bone_stream.is_translation_constant;
				segment_bone_stream.is_translation_default = clip_bone_stream.is_translation_default;
				segment_bone_stream.are_rotations_normalized = clip_bone_stream.are_rotations_normalized;
				segment_bone_stream.are_translations_normalized = clip_bone_stream.are_translations_normalized;
			}

			clip_sample_index += num_samples_in_segment;
		}

		deallocate_type_array(allocator, num_samples_per_segment, original_num_segments);
		deallocate_type_array(allocator, clip_segment, 1);
	}
}
