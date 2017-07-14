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
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/math/transform_32.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, Transform_32* out_local_pose)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			Quat_32 rotation;
			if (bone_stream.is_rotation_animated())
			{
				uint32_t num_samples = bone_stream.rotations.get_num_samples();
				float duration = bone_stream.rotations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Quat_32 sample0 = bone_stream.get_rotation_sample(key0);
				Quat_32 sample1 = bone_stream.get_rotation_sample(key1);
				rotation = quat_lerp(sample0, sample1, interpolation_alpha);
			}
			else
			{
				rotation = bone_stream.get_rotation_sample(0);
			}

			Vector4_32 translation;
			if (bone_stream.is_translation_animated())
			{
				uint32_t num_samples = bone_stream.translations.get_num_samples();
				float duration = bone_stream.translations.get_duration();

				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				calculate_interpolation_keys(num_samples, duration, sample_time, key0, key1, interpolation_alpha);

				Vector4_32 sample0 = bone_stream.get_translation_sample(key0);
				Vector4_32 sample1 = bone_stream.get_translation_sample(key1);
				translation = vector_lerp(sample0, sample1, interpolation_alpha);
			}
			else
			{
				translation = bone_stream.get_translation_sample(0);
			}

			out_local_pose[bone_index] = transform_set(rotation, translation);
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, uint32_t sample_index, Transform_32* out_local_pose)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			uint32_t rotation_sample_index = bone_stream.is_rotation_animated() ? sample_index : 0;
			Quat_32 rotation = bone_stream.get_rotation_sample(rotation_sample_index);

			uint32_t translation_sample_index = bone_stream.is_translation_animated() ? sample_index : 0;
			Vector4_32 translation = bone_stream.get_translation_sample(translation_sample_index);

			out_local_pose[bone_index] = transform_set(rotation, translation);
		}
	}
}
