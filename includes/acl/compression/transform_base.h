#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/version.h"
#include "acl/core/additive_utils.h"
#include "acl/core/error_result.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_base_adapter.h"
#include "acl/compression/transform_space.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// Describes the base of transform tracks: how each sample combines with the base.
	// The transform tracks hold their space, see 'track_array::get_transform_space'.
	// Compression combines each sample with the base first, in the space of the transform tracks.
	// Then it puts the result in object space, where it measures the error.
	// The default value describes transform tracks that have no base.
	//////////////////////////////////////////////////////////////////////////
	struct transform_base_desc_t
	{
		//////////////////////////////////////////////////////////////////////////
		// How a sample combines with the base, see 'additive_clip_format8'.
		// With 'none', the clip has no base and 'tracks' must be null.
		// Defaults to 'none'.
		additive_clip_format8 format = additive_clip_format8::none;

		//////////////////////////////////////////////////////////////////////////
		// The base transform tracks (vqvf, vqf, or vqsf). Their transform space must be 'local'.
		// In mesh space, compression puts the base in mesh space with 'rtm::vqv_mul' (the scale of a parent
		// does not shear its children), then it combines each sample with the base in mesh space.
		// In mesh rotation space, compression puts the rotations of the base in mesh space, then it combines
		// each sample with the base: the rotations in mesh space, the translations and scales in local space.
		// The base must have the same transform hierarchy as the clip.
		// The base can have a different transform type and a different number of samples than the clip.
		// A clip sample uses the base sample at the same normalized time, rounded to the nearest sample.
		// Required when 'format' is not 'none'. The caller owns the base.
		// Defaults to null.
		const track_array* tracks = nullptr;

		//////////////////////////////////////////////////////////////////////////
		// The base adapter that combines each sample with the base, see 'itransform_base_adapter_t'.
		// Required when 'format' is 'custom', and must be null otherwise.
		// The base adapter receives the base in the transform space of the clip.
		// The legacy error metric ('compression_settings::error_metric') does not support it.
		// The caller owns the base adapter.
		// Defaults to null.
		const itransform_base_adapter_t* adapter = nullptr;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not the clip has a base.
		bool has_base() const { return format != additive_clip_format8::none; }

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a base description is valid or not, for a clip in the specified transform space.
		// It is valid if:
		//    - The base format is a valid value
		//    - The mesh rotation space does not use the 'relative' base format
		//    - With a base format, the base is a valid and non-empty list of transform tracks in local space
		//    - Without a base format, the base is null
		//    - With the 'custom' base format, the base adapter is not null and its default scale is 0.0 or 1.0
		//    - With another base format, the base adapter is null
		error_result is_valid(transform_space_t space) const;
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

#include "acl/compression/impl/transform_base.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
