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

#include "acl/core/compressed_tracks.h"
#include "acl/core/error_result.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/output_stats.h"
#include "acl/compression/track_array.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Compresses a track array with uniform sampling.
	//
	// This compression algorithm is the simplest by far and as such it offers
	// the fastest compression and decompression. Every sample is retained and
	// every track has the same number of samples playing back at the same
	// sample rate. This means that when we sample at a particular time within
	// the clip, we can trivially calculate the offsets required to read the
	// desired data. All the data is sorted in order to ensure all reads are
	// as contiguous as possible for optimal cache locality during decompression.
	//
	//    allocator:				The allocator instance to use to allocate and free memory.
	//    track_list:				The track list to compress.
	//    settings:					The compression settings to use.
	//    out_compressed_tracks:	The resulting compressed tracks. The caller owns the returned memory and must free it.
	//    out_stats:				Stat output structure.
	//////////////////////////////////////////////////////////////////////////
	error_result compress_track_list(iallocator& allocator, const track_array& track_list, const compression_settings& settings,
		compressed_tracks*& out_compressed_tracks, output_stats& out_stats);

	//////////////////////////////////////////////////////////////////////////
	// Compresses a transform track array using its additive base with uniform sampling.
	//
	// This compression algorithm is the simplest by far and as such it offers
	// the fastest compression and decompression. Every sample is retained and
	// every track has the same number of samples playing back at the same
	// sample rate. This means that when we sample at a particular time within
	// the clip, we can trivially calculate the offsets required to read the
	// desired data. All the data is sorted in order to ensure all reads are
	// as contiguous as possible for optimal cache locality during decompression.
	//
	//    allocator:				The allocator instance to use to allocate and free memory.
	//    track_list:				The track list to compress.
	//    settings:					The compression settings to use.
	//    out_compressed_tracks:	The resulting compressed tracks. The caller owns the returned memory and must free it.
	//    out_stats:				Stat output structure.
	//////////////////////////////////////////////////////////////////////////
	error_result compress_track_list(iallocator& allocator, const track_array_qvvf& track_list, const compression_settings& settings,
		const track_array_qvvf& additive_base_track_list, additive_clip_format8 additive_format,
		compressed_tracks*& out_compressed_tracks, output_stats& out_stats);

	//////////////////////////////////////////////////////////////////////////
	// Takes a list of compressed track instances that contain the contributing error metadata and uses their data to build
	// a new database instance. Each compressed track instance will be duplicated and split between a new instance and the
	// database.
	//
	//    allocator:						The allocator instance to use
	//    settings:							The settings to use when creating the database
	//    compressed_tracks_list:			The list of compressed tracks to build the database with (must have contributing error metadata)
	//    num_compressed_tracks:			The number of compressed track instances in the above list
	//    out_compressed_tracks:			The output list of compressed tracks bound to the output database (array allocated by the caller (must be large enough); compressed_tracks instances allocated by the function)
	//    out_database:						The output database (allocated by the function)
	//////////////////////////////////////////////////////////////////////////
	error_result build_database(iallocator& allocator, const compression_database_settings& settings,
		const compressed_tracks* const* compressed_tracks_list, uint32_t num_compressed_tracks,
		compressed_tracks** out_compressed_tracks, compressed_database*& out_database);

	//////////////////////////////////////////////////////////////////////////
	// Takes a compressed database with inline bulk data and duplicates it into
	// a new database instance where the bulk data lives in separate buffers.
	//
	//    allocator:						The allocator instance to use to allocate the new database and its bulk data.
	//    database:							The source database to split with inline bulk data.
	//    out_split_database:				The new database without inline bulk data.
	//    out_bulk_data_medium:				The new database's bulk data for the medium importance tier.
	//    out_bulk_data_low:				The new database's bulk data for the low importance tier.
	//////////////////////////////////////////////////////////////////////////
	error_result split_compressed_database_bulk_data(iallocator& allocator, const compressed_database& database, compressed_database*& out_split_database, uint8_t*& out_bulk_data_medium, uint8_t*& out_bulk_data_low);

	//////////////////////////////////////////////////////////////////////////
	// A pair of pointers to a compressed tracks instance and its database.
	//////////////////////////////////////////////////////////////////////////
	struct database_merge_mapping
	{
		// The compressed tracks and its associated compressed database
		compressed_tracks* tracks;
		const compressed_database* database;

		// Checks if the mapping is valid
		error_result is_valid() const;
	};

	//////////////////////////////////////////////////////////////////////////
	// Merges the provided compressed databases together into a new instance.
	// The input databases are left unchanged (read only) and will no longer be
	// referenced by the compressed_tracks instances provided.
	// The input compressed_tracks instances will be updated to point to the new merged
	// database.
	//
	//    allocator:						The allocator instance to use to allocate the new database.
	//    settings:							The compression database settings to use
	//    merge_mappings:					The mappings to merge together into our new database.
	//    num_merge_mappings:				The number of mappings to merge together.
	//    out_merged_compressed_database:	The resulting merged database. The caller owns the returned memory and must free it.
	//////////////////////////////////////////////////////////////////////////
	error_result merge_compressed_databases(iallocator& allocator, const compression_database_settings& settings, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, compressed_database*& out_merged_compressed_database);
}

#include "acl/compression/impl/compress.scalar.impl.h"
#include "acl/compression/impl/compress.database.impl.h"
#include "acl/compression/impl/compress.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
