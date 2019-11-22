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

#include "acl/core/compiler_utils.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// We use a struct like this to allow an arbitrary format on the end user side.
	// Since our decode function is templated on this type implemented by the user,
	// the callbacks can trivially be inlined and customized.
	//////////////////////////////////////////////////////////////////////////
	struct OutputWriter
	{
		//////////////////////////////////////////////////////////////////////////
		// These allow the caller of decompress_pose to control which track types they are interested in.
		// This information allows the codecs to avoid unpacking values that are not needed.
		constexpr bool skip_all_bone_rotations() const { return false; }
		constexpr bool skip_all_bone_translations() const { return false; }
		constexpr bool skip_all_bone_scales() const { return false; }

		//////////////////////////////////////////////////////////////////////////
		// These allow the caller of decompress_pose to control which tracks they are interested in.
		// This information allows the codecs to avoid unpacking values that are not needed.
		constexpr bool skip_bone_rotation(uint16_t bone_index) const { return (void)bone_index, false; }
		constexpr bool skip_bone_translation(uint16_t bone_index) const { return (void)bone_index, false; }
		constexpr bool skip_bone_scale(uint16_t bone_index) const { return (void)bone_index, false; }

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a quaternion rotation value for a specified bone index.
		void RTM_SIMD_CALL write_bone_rotation(uint16_t bone_index, rtm::quatf_arg0 rotation)
		{
			(void)bone_index;
			(void)rotation;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a translation value for a specified bone index.
		void RTM_SIMD_CALL write_bone_translation(uint16_t bone_index, rtm::vector4f_arg0 translation)
		{
			(void)bone_index;
			(void)translation;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a scale value for a specified bone index.
		void RTM_SIMD_CALL write_bone_scale(uint16_t bone_index, rtm::vector4f_arg0 scale)
		{
			(void)bone_index;
			(void)scale;
		}
	};
}

ACL_IMPL_FILE_PRAGMA_POP
