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
#include "acl/core/track_desc.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track.h"

#include <rtm/vector4f.h>
#include <rtm/vqf.h>
#include <rtm/vqvf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// A VQ track has no scale. ACL uses the default scale of the additive format for each of its
		// samples, see 'get_additive_default_scale'. An additive base has no additive format, thus its scale is 1.0.

		//////////////////////////////////////////////////////////////////////////
		// Returns the description of a transform track.
		// For a VQ track, the scale of the default value is 'vq_scale'.
		inline track_desc_transformf get_transform_track_desc(const track& track_, rtm::vector4f_arg0 vq_scale)
		{
			ACL_ASSERT(is_transform_track_type(track_.get_type()), "Expected a transform track");

			track_desc_transformf desc = track_.get_description<track_desc_transformf>();
			if (track_.get_type() == track_type8::vqf)
				desc.default_value.scale = vq_scale;

			return desc;
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns a sample of a transform track as a VQV transform.
		// For a VQ track, the scale is 'vq_scale'.
		inline rtm::vqvf RTM_SIMD_CALL get_transform_track_sample(const track& track_, uint32_t sample_index, rtm::vector4f_arg0 vq_scale)
		{
			ACL_ASSERT(is_transform_track_type(track_.get_type()), "Expected a transform track");

			if (track_.get_type() == track_type8::vqf)
			{
				const rtm::vqf& sample = *static_cast<const rtm::vqf*>(track_[sample_index]);
				return rtm::vqv_set(sample.rotation, sample.translation, vq_scale);
			}

			return *static_cast<const rtm::vqvf*>(track_[sample_index]);
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
