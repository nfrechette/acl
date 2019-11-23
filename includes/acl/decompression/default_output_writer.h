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

#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/output_writer.h"

#include <rtm/types.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// A simple output writer implementation that simply writes out to a
	// Transform_32 array.
	//////////////////////////////////////////////////////////////////////////
	struct DefaultOutputWriter : OutputWriter
	{
		DefaultOutputWriter(rtm::qvvf* transforms, uint16_t num_transforms)
			: m_transforms(transforms)
			, m_num_transforms(num_transforms)
		{
			ACL_ASSERT(transforms != nullptr, "Transforms array cannot be null");
			ACL_ASSERT(num_transforms != 0, "Transforms array cannot be empty");
		}

		void RTM_SIMD_CALL write_bone_rotation(uint16_t bone_index, rtm::quatf_arg0 rotation)
		{
			ACL_ASSERT(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
			m_transforms[bone_index].rotation = rotation;
		}

		void RTM_SIMD_CALL write_bone_translation(uint16_t bone_index, rtm::vector4f_arg0 translation)
		{
			ACL_ASSERT(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
			m_transforms[bone_index].translation = translation;
		}

		void RTM_SIMD_CALL write_bone_scale(uint16_t bone_index, rtm::vector4f_arg0 scale)
		{
			ACL_ASSERT(bone_index < m_num_transforms, "Invalid bone index. %u >= %u", bone_index, m_num_transforms);
			m_transforms[bone_index].scale = scale;
		}

		rtm::qvvf* m_transforms;
		uint16_t m_num_transforms;
	};
}

ACL_IMPL_FILE_PRAGMA_POP
