#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include <rtm/types.h>
#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Describes how sub-tracks are processed.
	// Sub-tracks can be skipped and not written (assumes that the caller knows
	// what they are doing, probably pre-fills the output buffer), they can be
	// constant (e.g. identity) or they can vary per sub-track (e.g. bind pose).
	//////////////////////////////////////////////////////////////////////////
	enum class default_sub_track_mode
	{
		//////////////////////////////////////////////////////////////////////////
		// Sub-tracks are skipped entirely (e.g. caller pre-fills the output buffer)
		skipped,

		//////////////////////////////////////////////////////////////////////////
		// Sub-tracks have a constant default value (e.g. identity)
		constant,

		//////////////////////////////////////////////////////////////////////////
		// Sub-tracks have a variable default value (e.g. bind pose)
		variable,

		//////////////////////////////////////////////////////////////////////////
		// Scale sub-tracks have the pre ACL 2.1 behavior and will be constant
		// and default to 1.0 for additive0 and 0.0 for additive1
		// See additive_clip_format8
		// This is only for backwards compatibility and will be deprecated/removed for ACL 3.0
		// To handle additive scale properly, use the correct sub-track mode and
		// ensure the track_writer returns the correct value
		// USED FOR SCALE SUB-TRACKS ONLY
		//ACL_DEPRECATED("Use 'constant' instead and make sure the track_writer returns the correct value")
		legacy,
	};

	//////////////////////////////////////////////////////////////////////////
	// We use a struct like this to allow an arbitrary format on the end user side.
	// Since our decode function is templated on this type implemented by the user,
	// the callbacks can trivially be inlined and customized.
	// Only called functions need to be overridden and implemented.
	//////////////////////////////////////////////////////////////////////////
	struct track_writer
	{
		//////////////////////////////////////////////////////////////////////////
		// Scalar track writing

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void RTM_SIMD_CALL write_float1(uint32_t track_index, rtm::scalarf_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void RTM_SIMD_CALL write_float2(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void RTM_SIMD_CALL write_float3(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void RTM_SIMD_CALL write_float4(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a value for a specified track index.
		void RTM_SIMD_CALL write_vector4(uint32_t track_index, rtm::vector4f_arg0 value)
		{
			(void)track_index;
			(void)value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Transform track writing

#ifdef ACL_BIND_POSE

		//////////////////////////////////////////////////////////////////////////
		// If default sub-tracks aren't skipped, a value must be written. Either
		// they are constant for every sub-track (e.g. identity) or they vary per
		// sub-track (e.g. bind pose).
		// By default, default sub-tracks are constant and the identity.
		// Must be static constexpr!
		static constexpr default_sub_track_mode get_default_rotation_mode() { return default_sub_track_mode::constant; }
		static constexpr default_sub_track_mode get_default_translation_mode() { return default_sub_track_mode::constant; }
		static constexpr default_sub_track_mode get_default_scale_mode() { return default_sub_track_mode::legacy; }

		//////////////////////////////////////////////////////////////////////////
		// If default sub-tracks are constant, these functions return their value.
		rtm::quatf RTM_SIMD_CALL get_constant_default_rotation() const { return rtm::quat_identity(); }
		rtm::vector4f RTM_SIMD_CALL get_constant_default_translation() const { return rtm::vector_zero(); }
		rtm::vector4f RTM_SIMD_CALL get_constant_default_scale() const { return rtm::vector_set(1.0F); }

		//////////////////////////////////////////////////////////////////////////
		// If default sub-tracks are variable, these functions return their value.
		rtm::quatf RTM_SIMD_CALL get_variable_default_rotation(uint32_t /*track_index*/) const { return rtm::quat_identity(); }
		rtm::vector4f RTM_SIMD_CALL get_variable_default_translation(uint32_t /*track_index*/) const { return rtm::vector_zero(); }
		rtm::vector4f RTM_SIMD_CALL get_variable_default_scale(uint32_t /*track_index*/) const { return rtm::vector_set(1.0F); }

		//////////////////////////////////////////////////////////////////////////
		// These allow the caller of decompress_pose to substitute its own bind transforms as defaults
		// instead of identity transforms.
		// Must be non-static member functions!
		bool skip_default_rotation(uint32_t /*track_index*/) { return false; }
		bool skip_default_translation(uint32_t /*track_index*/) { return false; }
		bool skip_default_scale(uint32_t /*track_index*/) { return false; }

#endif

		//////////////////////////////////////////////////////////////////////////
		// These allow the caller of decompress_pose to control which track types they are interested in.
		// This information allows the codecs to avoid unpacking values that are not needed.
		// Must be static constexpr!
		static constexpr bool skip_all_rotations() { return false; }
		static constexpr bool skip_all_translations() { return false; }
		static constexpr bool skip_all_scales() { return false; }

		//////////////////////////////////////////////////////////////////////////
		// These allow the caller of decompress_pose to control which tracks they are interested in.
		// This information allows the codecs to avoid unpacking values that are not needed.
		// Must be non-static member functions!
		constexpr bool skip_track_rotation(uint32_t /*track_index*/) const { return false; }
		constexpr bool skip_track_translation(uint32_t /*track_index*/) const { return false; }
		constexpr bool skip_track_scale(uint32_t /*track_index*/) const { return false; }

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a quaternion rotation value for a specified bone index.
		void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation)
		{
			(void)track_index;
			(void)rotation;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a translation value for a specified bone index.
		void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation)
		{
			(void)track_index;
			(void)translation;
		}

		//////////////////////////////////////////////////////////////////////////
		// Called by the decoder to write out a scale value for a specified bone index.
		void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale)
		{
			(void)track_index;
			(void)scale;
		}
	};
}

ACL_IMPL_FILE_PRAGMA_POP
