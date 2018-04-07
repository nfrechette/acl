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
#include "acl/core/error.h"
#include "acl/math/vector4_32.h"
#include "acl/compression/stream/clip_context.h"

#include <stdint.h>

namespace acl
{
	inline void compact_constant_streams(IAllocator& allocator, ClipContext& clip_context, float rotation_threshold, float translation_threshold, float scale_threshold)
	{
		ACL_ASSERT(clip_context.num_segments == 1, "ClipContext must contain a single segment!");
		SegmentContext& segment = clip_context.segments[0];

		const uint16_t num_bones = clip_context.num_bones;
		uint16_t num_default_bone_scales = 0;

		// When a stream is constant, we only keep the first sample
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = segment.bone_streams[bone_index];
			BoneRanges& bone_range = clip_context.ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", bone_stream.translations.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", bone_stream.scales.get_sample_size(), sizeof(Vector4_32));

			if (bone_range.rotation.is_constant(rotation_threshold))
			{
				RotationTrackStream constant_stream(allocator, 1, bone_stream.rotations.get_sample_size(), bone_stream.rotations.get_sample_rate(), bone_stream.rotations.get_rotation_format());
				Vector4_32 rotation = bone_stream.rotations.get_raw_sample<Vector4_32>(0);
				constant_stream.set_raw_sample(0, rotation);

				bone_stream.rotations = std::move(constant_stream);
				bone_stream.is_rotation_constant = true;
				bone_stream.is_rotation_default = quat_near_identity(vector_to_quat(rotation));

				bone_range.rotation = TrackStreamRange(rotation, rotation);
			}

			if (bone_range.translation.is_constant(translation_threshold))
			{
				TranslationTrackStream constant_stream(allocator, 1, bone_stream.translations.get_sample_size(), bone_stream.translations.get_sample_rate(), bone_stream.translations.get_vector_format());
				Vector4_32 translation = bone_stream.translations.get_raw_sample<Vector4_32>(0);
				constant_stream.set_raw_sample(0, translation);

				bone_stream.translations = std::move(constant_stream);
				bone_stream.is_translation_constant = true;
				bone_stream.is_translation_default = vector_all_near_equal3(translation, vector_zero_32());

				bone_range.translation = TrackStreamRange(translation, translation);
			}

			if (bone_range.scale.is_constant(scale_threshold))
			{
				ScaleTrackStream constant_stream(allocator, 1, bone_stream.scales.get_sample_size(), bone_stream.scales.get_sample_rate(), bone_stream.scales.get_vector_format());
				Vector4_32 scale = bone_stream.scales.get_raw_sample<Vector4_32>(0);
				constant_stream.set_raw_sample(0, scale);

				bone_stream.scales = std::move(constant_stream);
				bone_stream.is_scale_constant = true;
				bone_stream.is_scale_default = vector_all_near_equal3(scale, vector_set(1.0f));

				bone_range.scale = TrackStreamRange(scale, scale);

				num_default_bone_scales += bone_stream.is_scale_default ? 1 : 0;
			}
		}

		clip_context.has_scale = num_default_bone_scales != num_bones;
	}
}
