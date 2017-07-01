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
#include "acl/math/vector4_64.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline void compact_constant_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, double threshold)
	{
		// When a stream is constant, we only keep the first sample

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_64)
			ACL_ENSURE(bone_stream.rotations.get_sample_size() == sizeof(Vector4_64), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Vector4_64));
			ACL_ENSURE(bone_stream.translations.get_sample_size() == sizeof(Vector4_64), "Unexpected translation sample size. %u != %u", bone_stream.translations.get_sample_size(), sizeof(Vector4_64));

			if (bone_stream.rotation_range.is_constant(threshold))
			{
				RotationTrackStream constant_stream(allocator, 1, bone_stream.rotations.get_sample_size(), bone_stream.rotations.get_sample_rate(), bone_stream.rotations.get_rotation_format());
				Vector4_64 rotation = bone_stream.rotations.get_raw_sample<Vector4_64>(0);
				constant_stream.set_raw_sample(0, rotation);

				bone_stream.rotations = std::move(constant_stream);
				bone_stream.rotation_range = TrackStreamRange(rotation, rotation);
				bone_stream.is_rotation_constant = true;
				bone_stream.is_rotation_default = quat_near_identity(quat_cast(vector_to_quat(rotation)));
			}

			if (bone_stream.translation_range.is_constant(threshold))
			{
				TranslationTrackStream constant_stream(allocator, 1, bone_stream.translations.get_sample_size(), bone_stream.translations.get_sample_rate(), bone_stream.translations.get_vector_format());
				Vector4_64 translation = bone_stream.translations.get_raw_sample<Vector4_64>(0);
				constant_stream.set_raw_sample(0, translation);

				bone_stream.translations = std::move(constant_stream);
				bone_stream.translation_range = TrackStreamRange(translation, translation);
				bone_stream.is_translation_constant = true;
				bone_stream.is_translation_default = vector_near_equal(vector_cast(translation), vector_zero_32());
			}
		}
	}
}
