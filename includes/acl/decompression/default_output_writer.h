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

#include "acl/decompression/output_writer.h"
#include "acl/math/math_types.h"

#include <stdint.h>

namespace acl
{
	struct DefaultOutputWriter : public OutputWriter
	{
		DefaultOutputWriter(Transform_32* transforms, uint16_t num_transforms)
			: m_transforms(transforms)
			, m_num_transforms(num_transforms)
		{
			ACL_ASSERT(transforms != nullptr, "Transforms array cannot be null");
			ACL_ASSERT(num_transforms != 0, "Transforms array cannot be empty");
		}

		void write_bone_rotation(uint32_t bone_index, const acl::Quat_32& rotation)
		{
			ACL_ASSERT(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
			m_transforms[bone_index].rotation = rotation;
		}

		void write_bone_translation(uint32_t bone_index, const acl::Vector4_32& translation)
		{
			ACL_ASSERT(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
			m_transforms[bone_index].translation = translation;
		}

		void write_bone_scale(uint32_t bone_index, const acl::Vector4_32& scale)
		{
			ACL_ASSERT(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
			m_transforms[bone_index].scale = scale;
		}

		Transform_32* m_transforms;
		uint16_t m_num_transforms;
	};
}
