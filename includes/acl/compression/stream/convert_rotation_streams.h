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
#include "acl/math/quat_64.h"
#include "acl/math/vector4_64.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline void convert_rotation_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, RotationFormat8 rotation_format)
	{
		if (num_bones == 0)
			return;

		if (rotation_format == RotationFormat8::Quat_128)
			return;	// Original format, nothing to do

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];

			// We convert our rotation stream in place. We assume that the original format is Quat_128 stored at Quat_64
			// For all other formats, we keep the same sample size and either keep Quat_64 or use Vector4_64
			ACL_ENSURE(bone_stream.rotations.get_sample_size() == sizeof(Quat_64), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Quat_64));

			Vector4_64 rotation_min = vector_set(1e10);
			Vector4_64 rotation_max = vector_set(-1e10);

			uint32_t num_samples = bone_stream.rotations.get_num_samples();
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Quat_64 rotation = bone_stream.rotations.get_sample<Quat_64>(sample_index);

				switch (rotation_format)
				{
				case RotationFormat8::Quat_128:
					// Original format, nothing to do
					break;
				case RotationFormat8::QuatDropW_96:
				case RotationFormat8::QuatDropW_48:
				case RotationFormat8::QuatDropW_32:
					// Drop W, we just ensure it is positive and write it back, the W component can be ignored afterwards
					rotation = quat_ensure_positive_w(rotation);
					bone_stream.rotations.set_sample(sample_index, rotation);
					break;
				default:
					ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(rotation_format));
					break;
				}

				rotation_min = vector_min(rotation_min, quat_to_vector(rotation));
				rotation_max = vector_max(rotation_max, quat_to_vector(rotation));
			}

			bone_stream.rotation_range = TrackStreamRange(rotation_min, rotation_max);
		}
	}
}
