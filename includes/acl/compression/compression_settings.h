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
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/track_formats.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/compression/compression_level.h"
#include "acl/compression/transform_error_metrics.h"

#include <rtm/scalarf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Encapsulates all the compression settings related to segmenting.
	// Segmenting ensures that large clips are split into smaller segments and
	// compressed independently to allow a smaller memory footprint as well as
	// faster compression and decompression.
	// See also: https://nfrechette.github.io/2016/11/10/anim_compression_uniform_segmenting/
	struct segmenting_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// How many samples to try and fit in our segments
		// Defaults to '16'
		uint32_t ideal_num_samples = 16;

		//////////////////////////////////////////////////////////////////////////
		// Maximum number of samples per segment
		// Defaults to '31'
		uint32_t max_num_samples = 31;

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const;

		//////////////////////////////////////////////////////////////////////////
		// Checks if everything is valid and if it isn't, returns an error string.
		// Returns nullptr if the settings are valid.
		error_result is_valid() const;
	};

	//////////////////////////////////////////////////////////////////////////
	// Encapsulates all the compression settings.
	struct compression_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// The compression level determines how aggressively we attempt to reduce the memory
		// footprint. Higher levels will try more permutations and bit rates. The higher
		// the level, the slower the compression but the smaller the memory footprint.
		// Transform tracks only.
		compression_level8 level = compression_level8::low;

		//////////////////////////////////////////////////////////////////////////
		// The rotation, translation, and scale formats to use. See functions get_rotation_format(..) and get_vector_format(..)
		// Defaults to raw: 'quatf_full' and 'vector3f_full'
		// Transform tracks only.
		rotation_format8 rotation_format = rotation_format8::quatf_full;
		vector_format8 translation_format = vector_format8::vector3f_full;
		vector_format8 scale_format = vector_format8::vector3f_full;

		//////////////////////////////////////////////////////////////////////////
		// Segmenting settings, if used
		// Transform tracks only.
		segmenting_settings segmenting;

		//////////////////////////////////////////////////////////////////////////
		// The error metric to use.
		// Defaults to 'null', this value must be set manually!
		// Transform tracks only.
		itransform_error_metric* error_metric = nullptr;

		//////////////////////////////////////////////////////////////////////////
		// Whether to include the optional metadata for the track list name
		// Defaults to 'false'
		bool include_track_list_name = false;

		//////////////////////////////////////////////////////////////////////////
		// Whether to include the optional metadata for track names
		// Defaults to 'false'
		bool include_track_names = false;

		//////////////////////////////////////////////////////////////////////////
		// Whether to include the optional metadata for parent track indices
		// Transform tracks only
		// Defaults to 'false'
		bool include_parent_track_indices = false;

		//////////////////////////////////////////////////////////////////////////
		// Whether to include the optional metadata for track descriptions
		// For transforms, also enables the parent track indices metadata
		// Defaults to 'false'
		bool include_track_descriptions = false;

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const;

		//////////////////////////////////////////////////////////////////////////
		// Checks if everything is valid and if it isn't, returns an error string.
		error_result is_valid() const;
	};

	//////////////////////////////////////////////////////////////////////////
	// Returns raw compression settings. No compression is performed and
	// samples are all retained with full precision.
	compression_settings get_raw_compression_settings();

	//////////////////////////////////////////////////////////////////////////
	// Returns the recommended and default compression settings. These have
	// been tested in a wide range of scenarios and perform best overall.
	compression_settings get_default_compression_settings();
}

#include "acl/compression/impl/compression_settings.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
