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
#include "acl/math/quat_32.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_64.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline BoneStreams* convert_clip_to_streams(Allocator& allocator, const AnimationClip& clip)
	{
		uint16_t num_bones = clip.get_num_bones();
		uint32_t num_samples = clip.get_num_samples();
		uint32_t sample_rate = clip.get_sample_rate();
		const AnimatedBone* bones = clip.get_bones();

		ACL_ENSURE(num_bones > 0, "Clip has no bones!");
		ACL_ENSURE(num_samples > 0, "Clip has no samples!");

		BoneStreams* bone_streams = allocate_type_array<BoneStreams>(allocator, num_bones);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const AnimatedBone& bone = bones[bone_index];
			BoneStreams& bone_stream = bone_streams[bone_index];

			bone_stream.rotations = RotationTrackStream(allocator, num_samples, sizeof(Quat_32), sample_rate, RotationFormat8::Quat_128);
			bone_stream.translations = TranslationTrackStream(allocator, num_samples, sizeof(Vector4_32), sample_rate, VectorFormat8::Vector3_96);

			Vector4_32 rotation_min = vector_set(1e10f);
			Vector4_32 rotation_max = vector_set(-1e10f);
			Vector4_32 translation_min = vector_set(1e10f);
			Vector4_32 translation_max = vector_set(-1e10f);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Quat_32 rotation = quat_normalize(quat_cast(bone.rotation_track.get_sample(sample_index)));
				bone_stream.rotations.set_raw_sample(sample_index, rotation);

				Vector4_32 translation = vector_cast(bone.translation_track.get_sample(sample_index));
				bone_stream.translations.set_raw_sample(sample_index, translation);

				rotation_min = vector_min(rotation_min, quat_to_vector(rotation));
				rotation_max = vector_max(rotation_max, quat_to_vector(rotation));
				translation_min = vector_min(translation_min, translation);
				translation_max = vector_max(translation_max, translation);
			}

			bone_stream.rotation_range = TrackStreamRange(rotation_min, rotation_max);
			bone_stream.translation_range = TrackStreamRange(translation_min, translation_max);
			bone_stream.is_rotation_constant = num_samples == 1;
			bone_stream.is_rotation_default = bone_stream.is_rotation_constant && quat_near_identity(quat_cast(bone.rotation_track.get_sample(0)));
			bone_stream.is_translation_constant = num_samples == 1;
			bone_stream.is_translation_default = bone_stream.is_translation_constant && vector_near_equal(vector_cast(bone.translation_track.get_sample(0)), vector_zero_32());
			bone_stream.are_rotations_normalized = false;
			bone_stream.are_translations_normalized = false;
		}

		return bone_streams;
	}
}
