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
#include "acl/core/hash.h"
#include "acl/core/impl/compiler_utils.h"

#include <rtm/vqvf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// Interface for a custom base format.
	// A base adapter combines each transform of a clip sample with the base, when the base
	// format is 'custom'. See 'additive_clip_format8::custom' and 'transform_base_desc_t'.
	//
	// Compression puts the base in the transform space of the clip first. Thus the base transform
	// and the clip transform that the adapter receives are in the same space. Then compression
	// puts the combined transforms in object space with the transform space of the clip.
	// The adapter combines one transform at a time: it does not use the transform hierarchy.
	//
	// Compression calls 'apply_base' in its inner loops, with the raw samples and with the lossy samples.
	// The same input must always give the same output.
	//////////////////////////////////////////////////////////////////////////
	class itransform_base_adapter_t
	{
	public:
		virtual ~itransform_base_adapter_t() = default;

		//////////////////////////////////////////////////////////////////////////
		// Returns the unique name of the base adapter.
		virtual const char* get_name() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns a unique hash to represent the base adapter.
		virtual uint32_t get_hash() const { return hash32(get_name()); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the default scale of the clip samples: 0.0 or 1.0.
		// A track without scale (for example, a 'vqf' track) gives this scale to each of its samples.
		// Decompression returns this scale when the compressed tracks have no scale.
		// The compressed data keeps one bit for the default scale, thus other values are not valid.
		// Defaults to 1.0.
		virtual float get_default_scale() const { return 1.0F; }

		//////////////////////////////////////////////////////////////////////////
		// Combines a transform of a clip sample with its base transform and returns the result.
		//    - 'transform_index': the index of the transform in the clip
		//    - 'base_transform': the base transform, in the transform space of the clip
		//    - 'transform': the transform of the clip sample, in the transform space of the clip
		// Compression normalizes the rotation of the result.
		virtual rtm::vqvf apply_base(uint32_t transform_index, const rtm::vqvf& base_transform, const rtm::vqvf& transform) const = 0;
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
