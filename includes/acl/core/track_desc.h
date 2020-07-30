#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/error_result.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// This structure describes the various settings for floating point scalar tracks.
	// Used by: float1f, float2f, float3f, float4f, vector4f
	struct track_desc_scalarf
	{
		//////////////////////////////////////////////////////////////////////////
		// The track category for this description.
		static constexpr track_category8 category = track_category8::scalarf;

		//////////////////////////////////////////////////////////////////////////
		// The track output index. When writing out the compressed data stream, this index
		// will be used instead of the track index. This allows custom reordering for things
		// like LOD sorting or skeleton remapping. A value of 'k_invalid_track_index' will strip the track
		// from the compressed data stream. Output indices must be unique and contiguous.
		uint32_t output_index = k_invalid_track_index;

		//////////////////////////////////////////////////////////////////////////
		// The per component precision threshold to try and attain when optimizing the bit rate.
		// If the error is below the precision threshold, we will remove bits until we reach it without
		// exceeding it. If the error is above the precision threshold, we will add more bits until
		// we lower it underneath.
		// Defaults to '0.00001'
		float precision = 0.00001F;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a scalar track description is valid or not.
		// It is valid if:
		//    - The precision is positive or zero and finite
		error_result is_valid() const;
	};

	//////////////////////////////////////////////////////////////////////////
	// This structure describes the various settings for transform tracks.
	// Used by: quatf, qvvf
	struct track_desc_transformf
	{
		//////////////////////////////////////////////////////////////////////////
		// The track category for this description.
		static constexpr track_category8 category = track_category8::transformf;

		//////////////////////////////////////////////////////////////////////////
		// The track output index. When writing out the compressed data stream, this index
		// will be used instead of the track index. This allows custom reordering for things
		// like LOD sorting or skeleton remapping. A value of 'k_invalid_track_index' will strip the track
		// from the compressed data stream. Output indices must be unique and contiguous.
		uint32_t output_index = k_invalid_track_index;

		//////////////////////////////////////////////////////////////////////////
		// The index of the parent transform track or `k_invalid_track_index` if it has no parent.
		uint32_t parent_index = k_invalid_track_index;

		//////////////////////////////////////////////////////////////////////////
		// The shell precision threshold to try and attain when optimizing the bit rate.
		// If the error is below the precision threshold, we will remove bits until we reach it without
		// exceeding it. If the error is above the precision threshold, we will add more bits until
		// we lower it underneath.
		// Note that you will need to change this value if your units are not in centimeters.
		// Defaults to '0.01' centimeters
		float precision = 0.01F;

		//////////////////////////////////////////////////////////////////////////
		// The error is measured on a rigidly deformed shell around every transform at the specified distance.
		// Defaults to '3.0' centimeters
		float shell_distance = 3.0F;

		//////////////////////////////////////////////////////////////////////////
		// TODO: Use the precision and shell distance?

		//////////////////////////////////////////////////////////////////////////
		// Threshold angle when detecting if rotation tracks are constant or default.
		// See the rtm::quatf quat_near_identity for details about how the default threshold
		// was chosen. You will typically NEVER need to change this, the value has been
		// selected to be as safe as possible and is independent of game engine units.
		// Defaults to '0.00284714461' radians
		float constant_rotation_threshold_angle = 0.00284714461F;

		//////////////////////////////////////////////////////////////////////////
		// Threshold value to use when detecting if translation tracks are constant or default.
		// Note that you will need to change this value if your units are not in centimeters.
		// Defaults to '0.001' centimeters.
		float constant_translation_threshold = 0.001F;

		//////////////////////////////////////////////////////////////////////////
		// Threshold value to use when detecting if scale tracks are constant or default.
		// There are no units for scale as such a value that was deemed safe was selected
		// as a default.
		// Defaults to '0.00001'
		float constant_scale_threshold = 0.00001F;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a transform track description is valid or not.
		// It is valid if:
		//    - The precision is positive or zero and finite
		//    - The shell distance is positive or zero and finite
		//    - The constant rotation threshold angle is positive or zero and finite
		//    - The constant translation threshold is positive or zero and finite
		//    - The constant scale threshold is positive or zero and finite
		error_result is_valid() const;
	};
}

#include "acl/core/impl/track_desc.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
