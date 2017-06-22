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
#include "acl/core/compressed_clip.h"
#include "acl/core/algorithm_types.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/decompression/output_writer.h"
#include "acl/math/transform_32.h"

#include <cstdio>

namespace acl
{
	// This interface serves to make unit testing and manipulating algorithms easier
	class IAlgorithm
	{
	public:
		virtual ~IAlgorithm() {}

		virtual CompressedClip* compress_clip(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton) = 0;

		virtual void decompress_pose(const CompressedClip& clip, float sample_time, Transform_32* out_transforms, uint16_t num_transforms) = 0;
		virtual void decompress_bone(const CompressedClip& clip, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation) = 0;

		virtual void print_stats(const CompressedClip& clip, std::FILE* file) {}

	protected:
		struct AlgorithmOutputWriterImpl : public OutputWriter
		{
			AlgorithmOutputWriterImpl(Transform_32* transforms, uint16_t num_transforms)
				: m_transforms(transforms)
				, m_num_transforms(num_transforms)
			{
				ACL_ENSURE(transforms != nullptr, "Transforms array cannot be null");
				ACL_ENSURE(num_transforms != 0, "Transforms array cannot be empty");
			}

			void write_bone_rotation(uint32_t bone_index, const acl::Quat_32& rotation)
			{
				ACL_ENSURE(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
				m_transforms[bone_index].rotation = rotation;
			}

			void write_bone_translation(uint32_t bone_index, const acl::Vector4_32& translation)
			{
				ACL_ENSURE(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
				m_transforms[bone_index].translation = translation;
			}

			Transform_32* m_transforms;
			uint16_t m_num_transforms;
		};
	};
}
