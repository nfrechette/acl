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

#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/segment_context.h"
#include "acl/compression/impl/write_stream_data.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	// TODO: Add 32 bit integer per segment header with which sample is dropped
	// TODO: Add 32 bit segment index into database
	// TODO: Database needs an array of segment metadata to track which segment samples are loaded
	// TODO: Database chunks need an array of segment indices contained and the samples contained
	// TODO: Samples loaded can be AND/ORed together easily for quick scanning
	// TODO: Each database asset contains data from only one tier and is split into chunks
	// TODO: Each chunk contains multiple clips/segments, chunks are contiguous

	// TODO: To decompress, grab clip UID, segment index, find loaded chunk that contains the samples we need
	//       if not loaded, check nearest samples
	//       From the chunk and local segment/sample indices, we can find where the data begins

	//       When we load a chunk, iterate over every clip/segment and update the loaded samples mapping

	//       Mapping needs a 32 bit offset per chunk, a 32 bit set sample list per chunk, these are per clip/segment

	//       With the clip ID, we find the list of segment metadata. Each segment can be split into at most 2-4 tiers
	//       and thus contains at most 2-4 chunk offsets (keep in mind tier 0 is the clip itself).
	//       Using our sample index, find the nearest loaded sample for each chunk (clz, ctz, popcount). Pick best.
	//       Calculate the frame offset with pop-count and the pose bit size stored in the clip.
	//       Try and fit 2-3 segments per cache line, or 32 bytes. This helps minimize cache misses even if we straddle
	//       segments. When we look for which chunks are loaded, we'll hit a single cache line for the segment header.
	//       We'll also need a cache miss for the database itself for the pointer we read from but it might be hot from
	//       some other decompression call to another clip from the same database.

	//       Clip ID should be unique for database and is the offset into the segment headers for the first header.

	//       Database should be a single blob with optional padding for region that can be memmapped. Everything is inline
	//       for simplicity. Loading can write directly in the database or memmap the region directly.
	//       Chunks will be 1 MB max (user controlled size?).

	//       Database chunks are sorted by tier, lower tiers stream first (with 0 always in memory in the clip itself).
	//       Database is split into two parts, the metadata + headers, and the bulk tier data. The metadata must always be loaded.
	//       Optionally, the bulk tier data could be loaded into a separate buffer.

	//       A database context is used at runtime for decompression and streaming management. The compressed database is read only
	//       and contains metadata and bulk data. UE4 will separate them. The metadata into the UObject package and the bulk data aside
	//       for async streaming. We'll allocate everything up front.

	//       We need a function to split the database into the metadata and bulk data so they can be serialized separately.
	//       The database context is thus bound with a pointer to the metadata and another (optional) to the bulk data
	//       and we can assert/fail init if the two mismatch or the bulk data is expected or not.

	//       When we look up the clip metadata in the database, can we check the clip hash matches the database entry for safety?

	// Compressed database contains the following:
	//    - Buffer headers/metadata for safety (tag, hash, etc)
	//    - Chunk metadata list: offset, size in bulk data per chunk

	// Bulk data contains the following:
	//    - Multiple chunks, max size 1 MB
	//        - Each chunk has a header: num segments, chunk index
	//        - A list of segment metadata contained in the chunk: clip ID, sample indices, chunk offset (sample data), clip header offset (runtime clip header), segment header offset (runtime segment header)
	//        - The frame data for each segment

	// Runtime data contains the following:
	//    - A bit set for loaded chunks
	//    - A bit set for in progress chunk loads
	//    - Clip and segment headers: clip ID, sample indices, chunk offsets for tier 1 and 2

	// Runtime context has a istreamer interface pointer. When we request to load a chunk on the context, it will validate invariants
	// and call the appropriate method on the streamer: load_chunk(chunk id, offset, size, write_ptr, continuation)
	// When the load is complete, the streamer calls the continuation with the same arguments passed to load_chunk along with a status result
	// with success/failure (error_result?). Runtime context will atomically update bit sets and after the load we parse the chunk metadata
	// to update atomically the loaded chunks: write chunk offset first (no need for atomic), then sample indices (no need for atomic, but ordered after offset)

	// We can have a sync_file_streamer, async_file_streamer, mmap_streamer, ue4_streamer, etc

	// If a database hasn't streamed in a given tier, we shouldn't pay the memory cost (unmap memory, free tier data)
	// To do this, add metadata to track how many chunks each tier contains, where the data starts, etc

	namespace acl_impl
	{
		inline void partition_into_database(iallocator& allocator, clip_context& clip)
		{
			for (SegmentContext& segment : clip.segment_iterator())
			{
				if (segment.sample_tiers == nullptr)
					segment.sample_tiers = allocate_type_array<database_tier8>(allocator, segment.num_samples);

				for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
					segment.sample_tiers[sample_index] = database_tier8::high_importance;

				// If we have an even number of samples, we can't remove every other one because we always retain the first and last sample
				if ((segment.num_samples % 2) == 0)
					continue;

				for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
				{
					// Drop every odd sample
					if ((sample_index % 2) == 1)
						segment.sample_tiers[sample_index] = database_tier8::low_importance;
				}
			}
		}

		// Returns the number of chunks written
		inline uint32_t write_database_chunk_descriptions(const clip_context& clip, database_chunk_description* chunk_descriptions)
		{
			if (clip.segments->sample_tiers == nullptr)
				return 0;	// No tiered sample data

			const uint32_t max_chunk_size = 1 * 1024 * 1024;
			const uint32_t simd_padding = 15;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_size = sizeof(database_chunk_header);
			uint32_t num_chunks = 0;

			for (const SegmentContext& segment : clip.segment_iterator())
			{
				uint32_t total_bit_size = 0;
				for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
				{
					if (segment.sample_tiers[sample_index] == database_tier8::low_importance)
						total_bit_size += segment.animated_pose_bit_size;
				}

				const uint32_t segment_data_size = (total_bit_size + 7) / 8;
				const uint32_t new_chunk_size = chunk_size + segment_data_size + simd_padding + sizeof(database_chunk_segment_header);
				if (new_chunk_size > max_chunk_size)
				{
					// Chunk is full, write it out and start a new one
					if (chunk_descriptions != nullptr)
					{
						chunk_descriptions[num_chunks].size = max_chunk_size;
						chunk_descriptions[num_chunks].offset = bulk_data_offset;
					}

					bulk_data_offset += max_chunk_size;
					chunk_size = sizeof(database_chunk_header);
					num_chunks++;
				}

				chunk_size += segment_data_size + sizeof(database_chunk_segment_header);
			}

			if (chunk_size != 0)
			{
				if (chunk_descriptions != nullptr)
				{
					chunk_descriptions[num_chunks].size = chunk_size + simd_padding;
					chunk_descriptions[num_chunks].offset = bulk_data_offset;
				}

				num_chunks++;
			}

			return num_chunks;
		}

		// Returns the size of the bulk data
		inline uint32_t write_database_bulk_data(const clip_context& clip, uint32_t clip_hash, uint8_t* bulk_data, const uint32_t* output_bone_mapping, uint32_t num_output_bones)
		{
			if (clip.segments->sample_tiers == nullptr)
				return 0;	// No tiered sample data

			// TODO: If the last chunk is too small, merge it with the previous chunk

			const uint32_t max_chunk_size = 1 * 1024 * 1024;
			const uint32_t simd_padding = 15;
			const bitset_description desc = bitset_description::make_from_num_bits<32>();

			database_chunk_header* chunk_header = nullptr;
			database_chunk_segment_header* segment_chunk_headers = nullptr;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_sample_data_offset = 0;
			uint32_t chunk_size = sizeof(database_chunk_header);
			uint32_t chunk_index = 0;

			if (bulk_data != nullptr)
			{
				// Setup our chunk headers
				chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data);
				chunk_header->index = chunk_index;
				chunk_header->size = 0;
				chunk_header->num_segments = 0;

				segment_chunk_headers = chunk_header->get_segment_headers();
			}

			// We first iterate to find our chunk delimitations and write our headers
			for (const SegmentContext& segment : clip.segment_iterator())
			{
				uint32_t sample_indices = 0;	// Default to false

				uint32_t num_samples_at_tier = 0;
				for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
				{
					if (segment.sample_tiers[sample_index] == database_tier8::low_importance)
					{
						bitset_set(&sample_indices, desc, sample_index, true);
						num_samples_at_tier++;
					}
				}

				const uint32_t segment_bit_size = segment.animated_pose_bit_size * num_samples_at_tier;
				const uint32_t segment_data_size = (segment_bit_size + 7) / 8;
				const uint32_t new_chunk_size = chunk_size + segment_data_size + simd_padding + sizeof(database_chunk_segment_header);
				if (new_chunk_size > max_chunk_size)
				{
					// Finalize our chunk header
					if (bulk_data != nullptr)
						chunk_header->size = max_chunk_size;

					// Chunk is full, start a new one
					bulk_data_offset += max_chunk_size;
					chunk_sample_data_offset = 0;
					chunk_size = sizeof(database_chunk_header);
					chunk_index++;

					// Setup our chunk headers
					if (bulk_data != nullptr)
					{
						chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_offset);
						chunk_header->index = chunk_index;
						chunk_header->size = 0;
						chunk_header->num_segments = 0;
					}

					segment_chunk_headers = chunk_header->get_segment_headers();
				}

				if (bulk_data != nullptr)
				{
					// TODO: Should we skip segments with no data?

					// Update our chunk headers
					database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_header->num_segments];
					segment_chunk_header.clip_hash = clip_hash;
					segment_chunk_header.sample_indices = sample_indices;
					segment_chunk_header.samples_offset = chunk_sample_data_offset;	// Relative to start of sample data for now

					// Fixed when we compress, updated when we merge databases
					segment_chunk_header.clip_header_offset = 0;
					segment_chunk_header.segment_header_offset = sizeof(database_runtime_clip_header) + chunk_header->num_segments * sizeof(database_runtime_segment_header);

					chunk_header->num_segments++;
				}

				chunk_size += segment_data_size + sizeof(database_chunk_segment_header);
				chunk_sample_data_offset += segment_data_size;
			}

			if (chunk_size != 0)
			{
				if (bulk_data != nullptr)
				{
					// Finalize our chunk header
					chunk_header->size = chunk_size + simd_padding;
				}

				bulk_data_offset += chunk_size + simd_padding;
			}

			// Now that our chunk headers are written, write our sample data
			if (bulk_data != nullptr)
			{
				// Reset our header pointers
				chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data);
				segment_chunk_headers = chunk_header->get_segment_headers();

				uint32_t chunk_segment_index = 0;
				for (const SegmentContext& segment : clip.segment_iterator())
				{
					uint32_t num_samples_at_tier = 0;
					for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
					{
						if (segment.sample_tiers[sample_index] == database_tier8::low_importance)
							num_samples_at_tier++;
					}

					const uint32_t segment_bit_size = segment.animated_pose_bit_size * num_samples_at_tier;
					const uint32_t segment_data_size = (segment_bit_size + 7) / 8;

					if (chunk_segment_index >= chunk_header->num_segments)
					{
						// We hit the next chunk, update our pointers
						chunk_header = add_offset_to_ptr<database_chunk_header>(chunk_header, chunk_header->size);
						segment_chunk_headers = chunk_header->get_segment_headers();
						chunk_segment_index = 0;
					}

					// Calculate the finale offset for our chunk's data relative to the bulk data start and the final header size
					const uint32_t chunk_data_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(chunk_header) - bulk_data);
					const uint32_t chunk_header_size = sizeof(database_chunk_header) + chunk_header->num_segments * sizeof(database_chunk_segment_header);

					// Update the sample offset from being relative to the start of the sample data to the start of the bulk data
					database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_segment_index];
					segment_chunk_header.samples_offset = chunk_data_offset + chunk_header_size + segment_chunk_header.samples_offset;

					uint8_t* animated_data = segment_chunk_header.samples_offset.add_to(bulk_data);
					const uint32_t size = write_animated_track_data(segment, database_tier8::low_importance, animated_data, segment_data_size, output_bone_mapping, num_output_bones);
					ACL_ASSERT(size == segment_data_size, "Unexpected segment data size"); (void)size;

					chunk_segment_index++;
				}
			}

			return bulk_data_offset;
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
