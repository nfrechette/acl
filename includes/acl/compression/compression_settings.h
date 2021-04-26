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
	// Encapsulates all the compression settings related to database usage.
	struct compression_database_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// What proportions we should use when distributing our frames based on
		// their importance to the overall error contribution. If a sample doesn't
		// go into the medium or low importance tiers, it will end up in the high
		// importance tier stored within each compressed track instance.
		// Proportion values must be between 0.0 and 1.0 and their sum as well.
		// If the sum is less than 1.0, remaining frames are considered to have high
		// importance. A low importance proportion of 30% means that the least important
		// 30% of frames will end up in that corresponding database tier.
		// Note that only movable frames can end up in the database as some frames must remain
		// within the compressed track instance. A frame is movable if it isn't the first or last
		// frame of its segment.
		// Defaults to '0.0' (the medium importance tier is empty)
		float medium_importance_tier_proportion = 0.0F;

		//////////////////////////////////////////////////////////////////////////
		// See above for details.
		// Defaults to '0.5' (the least important 50% of frames are moved to the database)
		float low_importance_tier_proportion = 0.5F;

		//////////////////////////////////////////////////////////////////////////
		// How large should each chunk be, in bytes.
		// This value must be at least 4 KB and ideally it should be a multiple of
		// the virtual memory page size used on the platform that will decompress
		// from the database.
		// Defaults to '1 MB'
		uint32_t max_chunk_size = 1 * 1024 * 1024;

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const;

		//////////////////////////////////////////////////////////////////////////
		// Checks if everything is valid and if it isn't, returns an error string.
		// Returns nullptr if the settings are valid.
		error_result is_valid() const;
	};

	//////////////////////////////////////////////////////////////////////////
	// Encapsulates all the optional metadata compression settings.
	struct compression_metadata_settings
	{
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
		// Whether to include the optional metadata for the contributing error
		// of each frame. These are sorted from lowest to largest error.
		// This is required when the compressed tracks will later be merged into
		// a database.
		// Transform tracks only
		// Defaults to 'false'
		bool include_contributing_error = false;

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
		// The error metric to use.
		// Defaults to 'null', this value must be set manually!
		// Transform tracks only.
		itransform_error_metric* error_metric = nullptr;

		//////////////////////////////////////////////////////////////////////////
		// Whether or not to enable database support on the output compressed clip.
		// This enables the required metadata which will later be stripped once
		// the database is built.
		// Transform tracks only.
		bool enable_database_support = false;

		//////////////////////////////////////////////////////////////////////////
		// These are optional metadata that can be added to compressed clips.
		compression_metadata_settings metadata;

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
