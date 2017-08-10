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

#include "acl/core/ialgorithm.h"
#include "acl/algorithm/uniformly_sampled/encoder.h"
#include "acl/algorithm/uniformly_sampled/decoder.h"

namespace acl
{
	class UniformlySampledAlgorithm final : public IAlgorithm
	{
	public:
		UniformlySampledAlgorithm(RotationFormat8 rotation_format, VectorFormat8 translation_format, RangeReductionFlags8 range_reduction, bool use_segmenting)
			: m_compression_settings()
		{
			m_compression_settings.rotation_format = rotation_format;
			m_compression_settings.translation_format = translation_format;
			m_compression_settings.range_reduction = range_reduction;
			m_compression_settings.segmenting.enabled = use_segmenting;
		}

		virtual CompressedClip* compress_clip(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton) override
		{
			return uniformly_sampled::compress_clip(allocator, clip, skeleton, m_compression_settings);
		}

		virtual void* allocate_decompression_context(Allocator& allocator, const CompressedClip& clip) override
		{
			uniformly_sampled::DecompressionSettings settings;
			return uniformly_sampled::allocate_decompression_context(allocator, settings, clip);
		}

		virtual void deallocate_decompression_context(Allocator& allocator, void* context) override
		{
			return uniformly_sampled::deallocate_decompression_context(allocator, context);
		}

		virtual void decompress_pose(const CompressedClip& clip, void* context, float sample_time, Transform_32* out_transforms, uint16_t num_transforms) override
		{
			uniformly_sampled::DecompressionSettings settings;
			AlgorithmOutputWriterImpl writer(out_transforms, num_transforms);
			uniformly_sampled::decompress_pose(settings, clip, context, sample_time, writer);
		}

		virtual void decompress_bone(const CompressedClip& clip, void* context, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation) override
		{
			uniformly_sampled::DecompressionSettings settings;
			uniformly_sampled::decompress_bone(settings, clip, context, sample_time, sample_bone_index, out_rotation, out_translation);
		}

		virtual void print_stats(const CompressedClip& clip, std::FILE* file) override
		{
			uniformly_sampled::print_stats(clip, file, m_compression_settings);
		}

	private:
		uniformly_sampled::CompressionSettings m_compression_settings;
	};
}
