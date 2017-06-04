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

#include "acl/algorithm/ialgorithm.h"
#include "acl/algorithm/uniformly_sampled/fixed_quantization_encoder.h"
#include "acl/algorithm/uniformly_sampled/fixed_quantization_decoder.h"

namespace acl
{
	namespace uniformly_sampled
	{
		class FixedQuantizationAlgorithm final : public IAlgorithm
		{
		public:
			virtual CompressedClip* encode(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, RotationFormat8 rotation_format) override
			{
				return fixed_quantization_encoder(allocator, clip, skeleton, rotation_format);
			}

			virtual void decode_pose(const CompressedClip& clip, float sample_time, Transform_32* out_transforms, uint16_t num_transforms) override
			{
				AlgorithmOutputWriterImpl writer(out_transforms, num_transforms);
				fixed_quantization_decoder(clip, sample_time, writer);
			}

			virtual void decode_bone(const CompressedClip& clip, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation) override
			{
				fixed_quantization_decoder(clip, sample_time, sample_bone_index, out_rotation, out_translation);
			}
		};
	}
}
