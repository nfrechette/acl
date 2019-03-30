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
#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/compression/stream/clip_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	inline Vector4_32 ACL_SIMD_CALL convert_rotation(Vector4_32Arg0 rotation, RotationFormat8 from, RotationFormat8 to)
	{
		ACL_ASSERT(from == RotationFormat8::Quat_128, "Source rotation format must be a full precision quaternion");
		(void)from;

		const RotationFormat8 high_precision_format = get_rotation_variant(to) == RotationVariant8::Quat ? RotationFormat8::Quat_128 : RotationFormat8::QuatDropW_96;
		switch (high_precision_format)
		{
		case RotationFormat8::Quat_128:
			// Original format, nothing to do
			return rotation;
		case RotationFormat8::QuatDropW_96:
			// Drop W, we just ensure it is positive and write it back, the W component can be ignored afterwards
			return quat_to_vector(quat_ensure_positive_w(vector_to_quat(rotation)));
		default:
			ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(to));
			return rotation;
		}
	}

	inline void convert_rotation_streams(IAllocator& allocator, SegmentContext& segment, RotationFormat8 rotation_format)
	{
		const RotationFormat8 high_precision_format = get_rotation_variant(rotation_format) == RotationVariant8::Quat ? RotationFormat8::Quat_128 : RotationFormat8::QuatDropW_96;

		for (BoneStreams& bone_stream : segment.bone_iterator())
		{
			// We convert our rotation stream in place. We assume that the original format is Quat_128 stored at Quat_32
			// For all other formats, we keep the same sample size and either keep Quat_32 or use Vector4_32
			ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(Quat_32), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Quat_32));

			const uint32_t num_samples = bone_stream.rotations.get_num_samples();
			const float sample_rate = bone_stream.rotations.get_sample_rate();
			RotationTrackStream converted_stream(allocator, num_samples, sizeof(Quat_32), sample_rate, high_precision_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Quat_32 rotation = bone_stream.rotations.get_raw_sample<Quat_32>(sample_index);

				switch (high_precision_format)
				{
				case RotationFormat8::Quat_128:
					// Original format, nothing to do
					break;
				case RotationFormat8::QuatDropW_96:
					// Drop W, we just ensure it is positive and write it back, the W component can be ignored afterwards
					rotation = quat_ensure_positive_w(rotation);
					break;
				default:
					ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(high_precision_format));
					break;
				}

				converted_stream.set_raw_sample(sample_index, rotation);
			}

			bone_stream.rotations = std::move(converted_stream);
		}
	}

	inline void convert_rotation_streams(IAllocator& allocator, ClipContext& clip_context, RotationFormat8 rotation_format)
	{
		for (SegmentContext& segment : clip_context.segment_iterator())
			convert_rotation_streams(allocator, segment, rotation_format);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
