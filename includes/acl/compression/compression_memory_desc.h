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
#include "acl/core/sample_looping_policy.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/transform_space.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// The value of 'compression_memory_desc_t::max_hierarchy_depth' when the depth is not known.
	constexpr uint32_t k_unknown_hierarchy_depth = 0xFFFFFFFFU;

	//////////////////////////////////////////////////////////////////////////
	// Describes the size of a track list (and its base) without its samples.
	// 'estimate_compression_peak_memory_usage' uses it to estimate the memory that compression needs
	// before the tracks are loaded. For example, a scheduler can fill it from the metadata of an asset.
	//
	// Except for 'total_name_length', the default value of each optional member gives the largest estimate.
	// A value that is closer to the real track list gives a smaller estimate.
	struct compression_memory_desc_t
	{
		//////////////////////////////////////////////////////////////////////////
		// The type of the tracks (scalar or transform).
		track_type8 track_type = track_type8::vqvf;

		//////////////////////////////////////////////////////////////////////////
		// The number of tracks.
		uint32_t num_tracks = 0;

		//////////////////////////////////////////////////////////////////////////
		// The number of samples of each track.
		uint32_t num_samples_per_track = 0;

		//////////////////////////////////////////////////////////////////////////
		// Optional: the looping policy of the tracks.
		// With the 'clamp' policy, the looping optimization can remove the last sample.
		sample_looping_policy looping_policy = sample_looping_policy::clamp;

		//////////////////////////////////////////////////////////////////////////
		// Optional: the space of the transform tracks, see 'transform_space_t'.
		transform_space_t transform_space = transform_space_t::local;

		//////////////////////////////////////////////////////////////////////////
		// Optional: the largest number of ancestors of a transform (a root transform has a depth of 0).
		// With 'k_unknown_hierarchy_depth', the estimate uses a single chain of transforms:
		// some buffers then grow with the square of the number of transforms.
		uint32_t max_hierarchy_depth = k_unknown_hierarchy_depth;

		//////////////////////////////////////////////////////////////////////////
		// Optional: the sum of the lengths of the track list name and of the track names, in bytes
		// and without the null terminators. Compression uses it only when the metadata settings include the names:
		// then set it, the default value of 0 does not include the names in the estimate.
		uint32_t total_name_length = 0;

		//////////////////////////////////////////////////////////////////////////
		// Optional: the format of the base of transform tracks, see 'transform_base_desc_t'.
		// The tracks have no base with 'additive_clip_format8::none'.
		additive_clip_format8 base_format = additive_clip_format8::none;

		//////////////////////////////////////////////////////////////////////////
		// Optional: the number of tracks of the base.
		uint32_t num_base_tracks = 0;

		//////////////////////////////////////////////////////////////////////////
		// Optional: the number of samples of each track of the base.
		uint32_t num_base_samples_per_track = 0;

		//////////////////////////////////////////////////////////////////////////
		// Optional: whether or not the scale of the base is not uniform.
		// With 'vqsf' tracks, a base with a uniform scale lets compression use smaller transforms.
		bool base_has_non_uniform_scale = true;
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
