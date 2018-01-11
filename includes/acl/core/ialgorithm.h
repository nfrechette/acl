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
#include "acl/core/compressed_clip.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/decompression/output_writer.h"
#include "acl/math/transform_32.h"

namespace acl
{
	struct CompressionSettings;
	struct OutputStats;

	// This interface serves to make unit testing and manipulating algorithms easier
	class IAlgorithm
	{
	public:
		virtual ~IAlgorithm() {}

		virtual CompressedClip* compress_clip(IAllocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, OutputStats& stats) = 0;

		virtual void* allocate_decompression_context(IAllocator& allocator, const CompressedClip& clip) = 0;
		virtual void deallocate_decompression_context(IAllocator& allocator, void* context) = 0;

		virtual void decompress_pose(const CompressedClip& clip, void* context, float sample_time, Transform_32* out_transforms, uint16_t num_transforms) = 0;
		virtual void decompress_bone(const CompressedClip& clip, void* context, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation, Vector4_32* out_scale) = 0;

		virtual const CompressionSettings& get_compression_settings() const = 0;
		virtual uint32_t get_uid() const = 0;
	};
}
