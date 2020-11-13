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

// Included only once from compress.h

#include "acl/core/iallocator.h"
#include "acl/core/compressed_database.h"
#include "acl/core/error_result.h"
#include "acl/core/hash.h"

#include <cstdint>

namespace acl
{
	inline error_result split_compressed_database_bulk_data(iallocator& allocator, const compressed_database& database, compressed_database*& out_split_database, uint8_t*& out_bulk_data)
	{
		using namespace acl_impl;

		const error_result result = database.is_valid(true);
		if (result.any())
			return result;

		if (!database.is_bulk_data_inline())
			return error_result("Bulk data is not inline in source database");

		const uint32_t total_size = database.get_total_size();
		const uint32_t bulk_data_size = database.get_bulk_data_size();
		const uint32_t db_size = total_size - bulk_data_size;

		// Allocate and setup our new database
		uint8_t* database_buffer = allocate_type_array_aligned<uint8_t>(allocator, db_size, alignof(compressed_database));
		out_split_database = reinterpret_cast<compressed_database*>(database_buffer);

		std::memcpy(database_buffer, &database, db_size);

		raw_buffer_header* database_buffer_header = safe_ptr_cast<raw_buffer_header>(database_buffer);
		database_buffer += sizeof(raw_buffer_header);

		database_header* db_header = safe_ptr_cast<database_header>(database_buffer);
		database_buffer += sizeof(database_header);

		db_header->bulk_data_offset = 0;
		db_header->set_is_bulk_data_inline(false);

		database_buffer_header->size = db_size;
		database_buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(db_header), db_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header
		ACL_ASSERT(out_split_database->is_valid(true).empty(), "Failed to split database");

		// Allocate and setup our new bulk data
		uint8_t* bulk_data_buffer = allocate_type_array_aligned<uint8_t>(allocator, bulk_data_size, alignof(compressed_database));
		out_bulk_data = bulk_data_buffer;

		std::memcpy(bulk_data_buffer, database.get_bulk_data(), bulk_data_size);

		const uint32_t bulk_data_hash = hash32(bulk_data_buffer, bulk_data_size);
		ACL_ASSERT(bulk_data_hash == database.get_bulk_data_hash(), "Bulk data hash mismatch");

		return error_result();
	}

	inline error_result database_merge_mapping::is_valid() const
	{
		if (tracks == nullptr)
			return error_result("No compressed tracks provided");

		if (tracks->is_valid(false).any())
			return error_result("Compressed tracks aren't valid");

		if (database == nullptr)
			return error_result("No compressed database provided");

		if (database->is_valid(false).any())
			return error_result("Compressed database isn't valid");

		if (!database->contains(*tracks))
			return error_result("Compressed database doesn't contain the compressed tracks");

		if (!database->is_bulk_data_inline())
			return error_result("Compressed database does not have inline bulk data");

		if (database->get_num_clips() != 1)
			return error_result("Compressed database already contains more than 1 clip");

		return error_result();
	}

	namespace acl_impl
	{
		// Returns the number of chunks written
		inline uint32_t write_database_chunk_descriptions(const compression_database_settings& settings, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, database_chunk_description* chunk_descriptions)
		{
			const uint32_t max_chunk_size = settings.max_chunk_size;
			const uint32_t simd_padding = 15;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_size = sizeof(database_chunk_header);
			uint32_t num_chunks = 0;

			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_database& database = *merge_mappings[mapping_index].database;
				const database_header& header = get_database_header(database);
				const database_chunk_description* db_chunk_descriptions = header.get_chunk_descriptions();
				const uint32_t num_db_chunks = header.num_chunks;
				for (uint32_t chunk_index = 0; chunk_index < num_db_chunks; ++chunk_index)
				{
					const database_chunk_description& chunk_description = db_chunk_descriptions[chunk_index];
					if (chunk_description.size >= max_chunk_size)
					{
						// Chunk is already full, add it as-is
						if (chunk_descriptions != nullptr)
						{
							chunk_descriptions[num_chunks].size = chunk_description.size;
							chunk_descriptions[num_chunks].offset = bulk_data_offset;
						}

						bulk_data_offset += chunk_description.size;
						num_chunks++;
						continue;
					}

					// The last chunk already has SIMD padding, remove it
					const uint32_t last_chunk_padding = (chunk_index == (num_db_chunks - 1)) ? simd_padding : 0;
					const uint32_t new_chunk_size = chunk_size + chunk_description.size + simd_padding - last_chunk_padding;
					if (new_chunk_size >= max_chunk_size)
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

					// Make sure we aren't larger than the new chunk we just created
					ACL_ASSERT((chunk_size + chunk_description.size + simd_padding - last_chunk_padding) < max_chunk_size, "Chunk size is too large");

					// Update our chunk size and remove the padding if present and the chunk header since we have our own
					chunk_size += chunk_description.size - last_chunk_padding - sizeof(database_chunk_header);
				}
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

		struct merged_db_metadata
		{
			uint32_t num_chunks = 0;
			uint32_t num_clips = 0;
			uint32_t num_segments = 0;
		};

		inline uint32_t merged_database_segment_index_to_mapping_index(uint32_t merged_database_segment_index, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t merged_segment_count = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

				if (merged_database_segment_index >= merged_segment_count && merged_database_segment_index < merged_segment_count + transform_header.num_segments)
					return mapping_index;

				merged_segment_count += transform_header.num_segments;
			}

			ACL_ASSERT(false, "Failed to find mapping index");
			return ~0U;
		}

		inline uint32_t merged_database_segment_index_to_runtime_clip_offset(uint32_t merged_database_segment_index, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t merged_segment_count = 0;
			uint32_t runtime_offset = 0;
			uint32_t clip_runtime_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

				clip_runtime_offset = runtime_offset;
				runtime_offset += sizeof(database_runtime_clip_header);

				if (merged_database_segment_index >= merged_segment_count && merged_database_segment_index < merged_segment_count + transform_header.num_segments)
					return clip_runtime_offset;

				runtime_offset += transform_header.num_segments * sizeof(database_runtime_segment_header);
				merged_segment_count += transform_header.num_segments;
			}

			ACL_ASSERT(false, "Failed to find mapping index");
			return ~0U;
		}

		inline uint32_t merged_database_segment_index_to_runtime_segment_offset(uint32_t merged_database_segment_index, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t merged_segment_count = 0;
			uint32_t runtime_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

				runtime_offset += sizeof(database_runtime_clip_header);

				if (merged_database_segment_index >= merged_segment_count && merged_database_segment_index < merged_segment_count + transform_header.num_segments)
				{
					const uint32_t clip_segment_index = merged_database_segment_index - merged_segment_count;
					return runtime_offset + clip_segment_index * sizeof(database_runtime_segment_header);
				}

				runtime_offset += transform_header.num_segments * sizeof(database_runtime_segment_header);
				merged_segment_count += transform_header.num_segments;
			}

			ACL_ASSERT(false, "Failed to find mapping index");
			return ~0U;
		}

		// Returns the size of the bulk data
		inline uint32_t write_database_bulk_data(iallocator& allocator, const compression_database_settings& settings, const merged_db_metadata& db_metadata, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, uint8_t* bulk_data)
		{
			(void)db_metadata;

			// TODO: If the last chunk is too small, merge it with the previous chunk?

			const uint32_t max_chunk_size = settings.max_chunk_size;
			const uint32_t simd_padding = 15;

			uint8_t* tmp_chunk_data = nullptr;

			database_chunk_header* tmp_chunk_header = nullptr;
			database_chunk_segment_header* tmp_segment_chunk_headers = nullptr;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_sample_data_offset = 0;
			uint32_t tmp_chunk_size = sizeof(database_chunk_header);

			if (bulk_data != nullptr)
			{
				// Allocate a temporary chunk so we can append to it while we build the final bulk data
				tmp_chunk_data = allocate_type_array<uint8_t>(allocator, max_chunk_size);

				// Reset our temporary chunk
				std::memset(tmp_chunk_data, 0, max_chunk_size);

				// Setup our chunk pointers
				tmp_chunk_header = safe_ptr_cast<database_chunk_header>(tmp_chunk_data);
				tmp_segment_chunk_headers = tmp_chunk_header->get_segment_headers();

				// Our temporary chunk is used to hold partial chunks, we set an invalid chunk index to be able to identify partial chunks
				// in our second step (detailed below). Whole chunks will have a valid chunk index that is fixed up later.
				tmp_chunk_header->index = ~0U;
			}

			// The merge process of the bulk data is performed in three steps:
			//    - We first find where the chunks break down by appending whole chunks as-is and by merging partial chunks
			//      when possible. Whole chunks will require minor fixup later.
			//    - Once we know where the chunks start and end, we copy the partial chunk data and fixup our offsets
			//    - With our chunk data in place in the bulk data buffer, we can fixup our headers
			//
			// Whole chunks we copy have their sample data offsets relative to the original bulk data them belong into.
			// We convert it to relative offsets in the first step and back to absolute offsets in the second step.
			//
			// To simplify iteration in the second step, we re-use the chunk segment clip's header offset to map where the segment data
			// lives in the source chunk. We setup the offset in the first step for partial chunks, copy in the second step and set
			// the final clip header offset value in the third step. The sample offset alone isn't enough to know which database it
			// comes from as such as also re-purpose the clip hash to be the merged database segment index.
			// We also re-purpose the clip hash for whole chunks to properly fixup the runtime header offsets in the third step.
			// The clip hash will be properly set in the third step.
			//
			// We similarly re-use the chunk segment segment's header offset to contain the segment data size to copy. We set this for
			// partial chunks in the first step, use it to copy our data in the second step. We set its final segment offset value
			// in the third step.
			//
			// This avoids the need to allocate and manage separate metadata.

			{
				uint32_t merged_database_segment_index = 0;

				// First step: we iterate to find our chunk delimitations and write our headers
				for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
				{
					const compressed_database& database = *merge_mappings[mapping_index].database;
					const database_header& header = get_database_header(database);
					const database_chunk_description* db_chunk_descriptions = header.get_chunk_descriptions();
					const uint32_t num_db_chunks = header.num_chunks;

					for (uint32_t chunk_index = 0; chunk_index < num_db_chunks; ++chunk_index)
					{
						const database_chunk_description& db_chunk_description = db_chunk_descriptions[chunk_index];
						if (db_chunk_description.size >= max_chunk_size)
						{
							// Chunk is already full, add it as-is
							if (bulk_data != nullptr)
							{
								// Append our new chunk right away
								std::memcpy(bulk_data + bulk_data_offset, db_chunk_description.get_chunk_header(database.get_bulk_data()), db_chunk_description.size);

								// Chunk indices will be fixed up later

								// Fixup our offsets
								database_chunk_header* new_chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_offset);
								database_chunk_segment_header* new_segment_chunk_headers = new_chunk_header->get_segment_headers();

								const uint32_t db_chunk_offset = db_chunk_description.offset;
								const uint32_t chunk_header_size = sizeof(database_chunk_header) + new_chunk_header->num_segments * sizeof(database_chunk_segment_header);

								// Update our chunk headers
								for (uint32_t db_chunk_segment_index = 0; db_chunk_segment_index < new_chunk_header->num_segments; ++db_chunk_segment_index)
								{
									database_chunk_segment_header& segment_chunk_header = new_segment_chunk_headers[db_chunk_segment_index];

									// Original samples_offset is relative to the start of the bulk data but we need it relative
									// to the start of the chunk for now
									segment_chunk_header.samples_offset = segment_chunk_header.samples_offset - db_chunk_offset - chunk_header_size;

									// See comment at the top of this function, we re-purpose this value
									segment_chunk_header.clip_hash = merged_database_segment_index;

									merged_database_segment_index++;
								}
							}

							bulk_data_offset += db_chunk_description.size;
							continue;
						}

						// The last chunk already has SIMD padding, remove it
						const uint32_t last_chunk_padding = (chunk_index == (num_db_chunks - 1)) ? simd_padding : 0;
						const uint32_t new_chunk_size = tmp_chunk_size + db_chunk_description.size + simd_padding - last_chunk_padding;
						if (new_chunk_size >= max_chunk_size)
						{
							// Chunk is full

							// Finalize our chunk header
							if (bulk_data != nullptr)
							{
								tmp_chunk_header->size = max_chunk_size;

								// Copy our temporary chunk into its final location in the bulk data
								std::memcpy(bulk_data + bulk_data_offset, tmp_chunk_header, max_chunk_size);

								// Reset our temporary chunk
								std::memset(tmp_chunk_data, 0, max_chunk_size);
								tmp_chunk_header->index = ~0U;
							}

							// Start a new one
							bulk_data_offset += max_chunk_size;
							chunk_sample_data_offset = 0;
							tmp_chunk_size = sizeof(database_chunk_header);
						}

						// Make sure we aren't larger than the new chunk we just created
						ACL_ASSERT((tmp_chunk_size + db_chunk_description.size + simd_padding - last_chunk_padding) < max_chunk_size, "Chunk size is too large");

						if (bulk_data != nullptr)
						{
							// TODO: Should we skip segments with no data?

							// Update our chunk headers
							const database_chunk_header* db_chunk_header = db_chunk_description.get_chunk_header(database.get_bulk_data());
							const database_chunk_segment_header* db_segment_chunk_headers = db_chunk_header->get_segment_headers();
							for (uint32_t db_chunk_segment_index = 0; db_chunk_segment_index < db_chunk_header->num_segments; ++db_chunk_segment_index)
							{
								const database_chunk_segment_header& db_segment_chunk_header = db_segment_chunk_headers[db_chunk_segment_index];
								database_chunk_segment_header& segment_chunk_header = tmp_segment_chunk_headers[tmp_chunk_header->num_segments];

								segment_chunk_header.sample_indices = db_segment_chunk_header.sample_indices;
								segment_chunk_header.samples_offset = chunk_sample_data_offset;	// Relative to start of the sample data for now

								uint32_t segment_data_size;
								if (db_chunk_segment_index + 1 < db_chunk_header->num_segments)
								{
									// Not the last segment, use the offset from the next one to calculate our size
									segment_data_size = db_segment_chunk_headers[db_chunk_segment_index + 1].samples_offset - db_segment_chunk_header.samples_offset;
								}
								else
								{
									// Last segment, use the end of the chunk to calculate our size
									segment_data_size = db_chunk_description.offset + db_chunk_description.size - last_chunk_padding - db_segment_chunk_header.samples_offset;
								}

								// See comment at the top of this function, we re-purpose these offsets/values
								segment_chunk_header.clip_hash = merged_database_segment_index;
								segment_chunk_header.clip_header_offset = static_cast<uint32_t>(db_segment_chunk_header.samples_offset);
								segment_chunk_header.segment_header_offset = segment_data_size;

								chunk_sample_data_offset += segment_data_size;
								merged_database_segment_index++;
								tmp_chunk_header->num_segments++;
							}
						}

						// Update our chunk size and remove the padding if present and the chunk header since we have our own
						tmp_chunk_size += db_chunk_description.size - last_chunk_padding - sizeof(database_chunk_header);
					}
				}

				if (tmp_chunk_size != 0)
				{
					if (bulk_data != nullptr)
					{
						// Finalize our chunk header
						tmp_chunk_header->size = tmp_chunk_size + simd_padding;

						// Copy our temporary chunk into its final location in the bulk data
						std::memcpy(bulk_data + bulk_data_offset, tmp_chunk_header, tmp_chunk_header->size);
					}

					bulk_data_offset += tmp_chunk_size + simd_padding;
				}

				ACL_ASSERT(bulk_data == nullptr || merged_database_segment_index == db_metadata.num_segments, "Unexpected segment count");
			}

			// Now that our chunk headers are written, write our sample data and do the final fixup for the headers
			if (bulk_data != nullptr)
			{
				uint32_t bulk_data_update_offset = 0;

				// Second step: copy our partial chunk data into its final location
				while (bulk_data_update_offset < bulk_data_offset)
				{
					database_chunk_header* chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_update_offset);
					database_chunk_segment_header* segment_chunk_headers = chunk_header->get_segment_headers();

					// Move onto the next chunk
					bulk_data_update_offset += chunk_header->size;

					for (uint32_t chunk_segment_index = 0; chunk_segment_index < chunk_header->num_segments; ++chunk_segment_index)
					{
						database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_segment_index];

						// See comment at the top of this function, we re-purpose these offsets/values
						const uint32_t merged_database_segment_index = segment_chunk_header.clip_hash;

						// Calculate the finale offset for our chunk's data relative to the bulk data start and the final header size
						const uint32_t chunk_data_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(chunk_header) - bulk_data);
						const uint32_t chunk_header_size = sizeof(database_chunk_header) + chunk_header->num_segments * sizeof(database_chunk_segment_header);

						// Update the sample offset from being relative to the start of the sample data to the start of the bulk data
						segment_chunk_header.samples_offset = chunk_data_offset + chunk_header_size + segment_chunk_header.samples_offset;

						// Copy our partial chunk data
						if (chunk_header->index == ~0U)
						{
							// See comment at the top of this function, we re-purpose these offsets/values
							const uint32_t segment_data_size = segment_chunk_header.segment_header_offset;
							const ptr_offset32<const uint8_t> src_samples_offset = static_cast<uint32_t>(segment_chunk_header.clip_header_offset);

							const uint32_t mapping_index = merged_database_segment_index_to_mapping_index(merged_database_segment_index, merge_mappings, num_merge_mappings);
							const compressed_database& database = *merge_mappings[mapping_index].database;

							const uint8_t* src_animated_data = src_samples_offset.add_to(database.get_bulk_data());

							uint8_t* dst_animated_data = segment_chunk_header.samples_offset.add_to(bulk_data);

							std::memcpy(dst_animated_data, src_animated_data, segment_data_size);
						}
					}
				}

				// Reset iteration
				bulk_data_update_offset = 0;

				uint32_t chunk_index = 0;

				// Third step: fixup our headers
				while (bulk_data_update_offset < bulk_data_offset)
				{
					database_chunk_header* chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_update_offset);
					database_chunk_segment_header* segment_chunk_headers = chunk_header->get_segment_headers();

					// Move onto the next chunk
					bulk_data_update_offset += chunk_header->size;

					// Set our chunk index
					chunk_header->index = chunk_index++;

					for (uint32_t chunk_segment_index = 0; chunk_segment_index < chunk_header->num_segments; ++chunk_segment_index)
					{
						database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_segment_index];

						// See comment at the top of this function, we re-purpose this value
						const uint32_t merged_database_segment_index = segment_chunk_header.clip_hash;

						const uint32_t mapping_index = merged_database_segment_index_to_mapping_index(merged_database_segment_index, merge_mappings, num_merge_mappings);
						const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
						const uint32_t clip_hash = tracks.get_hash();

						// Set our final values now that they are known
						segment_chunk_header.clip_hash = clip_hash;
						segment_chunk_header.clip_header_offset = merged_database_segment_index_to_runtime_clip_offset(merged_database_segment_index, merge_mappings, num_merge_mappings);
						segment_chunk_header.segment_header_offset = merged_database_segment_index_to_runtime_segment_offset(merged_database_segment_index, merge_mappings, num_merge_mappings);
					}
				}
			}

			deallocate_type_array(allocator, tmp_chunk_data, max_chunk_size);

			return bulk_data_offset;
		}

		inline void write_database_clip_metadata(const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, database_clip_metadata* clip_metadata)
		{
			uint32_t clip_index = 0;
			uint32_t runtime_header_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const compressed_database& database = *merge_mappings[mapping_index].database;

				uint32_t bulk_data_offset = 0;
				uint32_t last_clip_hash = 0;
				while (bulk_data_offset < database.get_bulk_data_size())
				{
					const database_chunk_header* chunk_header = safe_ptr_cast<const database_chunk_header>(database.get_bulk_data() + bulk_data_offset);
					const database_chunk_segment_header* segment_headers = chunk_header->get_segment_headers();
					for (uint32_t segment_index = 0; segment_index < chunk_header->num_segments; ++segment_index)
					{
						const database_chunk_segment_header& segment_header = segment_headers[segment_index];

						if (last_clip_hash != segment_header.clip_hash)
						{
							// New clip
							clip_metadata[clip_index].clip_hash = tracks.get_hash();
							clip_metadata[clip_index].clip_header_offset = runtime_header_offset;

							runtime_header_offset += sizeof(database_runtime_clip_header);

							last_clip_hash = segment_header.clip_hash;
							clip_index++;
						}

						runtime_header_offset += sizeof(database_runtime_segment_header);
					}

					bulk_data_offset += chunk_header->size;
				}
			}
		}

		inline void update_input_mappings(const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings)
		{
			uint32_t clip_index = 0;
			uint32_t runtime_header_offset = 0;
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
				const compressed_database& database = *merge_mappings[mapping_index].database;

				// Update our metadata
				transform_tracks_header& transform_header = get_transform_tracks_header(tracks);
				tracks_database_header* tracks_db_header = transform_header.get_database_header();
				tracks_db_header->clip_header_offset = runtime_header_offset;

				// Recalculate our hash
				raw_buffer_header* buffer_header = safe_ptr_cast<raw_buffer_header>(&tracks);
				uint8_t* tracks_header = add_offset_to_ptr<uint8_t>(&tracks, sizeof(raw_buffer_header));
				buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(tracks_header), buffer_header->size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

				// Update our header offset
				uint32_t bulk_data_offset = 0;
				uint32_t last_clip_hash = 0;
				while (bulk_data_offset < database.get_bulk_data_size())
				{
					const database_chunk_header* chunk_header = safe_ptr_cast<const database_chunk_header>(database.get_bulk_data() + bulk_data_offset);
					const database_chunk_segment_header* segment_headers = chunk_header->get_segment_headers();
					for (uint32_t segment_index = 0; segment_index < chunk_header->num_segments; ++segment_index)
					{
						const database_chunk_segment_header& segment_header = segment_headers[segment_index];

						runtime_header_offset += sizeof(database_runtime_segment_header);

						if (last_clip_hash != segment_header.clip_hash)
						{
							// New clip
							runtime_header_offset += sizeof(database_runtime_clip_header);

							last_clip_hash = segment_header.clip_hash;
							clip_index++;
						}
					}

					bulk_data_offset += chunk_header->size;
				}
			}
		}
	}

	inline error_result merge_compressed_databases(iallocator& allocator, const compression_database_settings& settings, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, compressed_database*& out_merged_compressed_database)
	{
		using namespace acl_impl;

		const error_result settings_result = settings.is_valid();
		if (settings_result.any())
			return settings_result;

		if (merge_mappings == nullptr || num_merge_mappings == 0)
			return error_result("No merge mappings provided");

		for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
		{
			const error_result result = merge_mappings[mapping_index].is_valid();
			if (result.any())
				return result;

			const acl_impl::database_header& header = acl_impl::get_database_header(*merge_mappings[mapping_index].database);
			if (settings.max_chunk_size < header.max_chunk_size)
				return error_result("Cannot merge databases into smaller chunks");
		}

		// Since we'll create a new merged database, the input mappings need to be updated to point to the new merged database
		// Do so now since it'll change our hash which we need later for safe binding
		update_input_mappings(merge_mappings, num_merge_mappings);

		merged_db_metadata db_metadata;

		for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
		{
			db_metadata.num_clips += merge_mappings[mapping_index].database->get_num_clips();
			db_metadata.num_segments += merge_mappings[mapping_index].database->get_num_segments();
		}

		db_metadata.num_chunks = write_database_chunk_descriptions(settings, merge_mappings, num_merge_mappings, nullptr);
		const uint32_t bulk_data_size = write_database_bulk_data(allocator, settings, db_metadata, merge_mappings, num_merge_mappings, nullptr);

		uint32_t database_buffer_size = 0;
		database_buffer_size += sizeof(raw_buffer_header);							// Header
		database_buffer_size += sizeof(database_header);							// Header

		database_buffer_size = align_to(database_buffer_size, 4);					// Align chunk descriptions
		database_buffer_size += db_metadata.num_chunks * sizeof(database_chunk_description);	// Chunk descriptions

		database_buffer_size = align_to(database_buffer_size, 4);					// Align clip hashes
		database_buffer_size += db_metadata.num_clips * sizeof(database_clip_metadata);			// Clip metadata (only one when we compress)

		database_buffer_size = align_to(database_buffer_size, 8);					// Align bulk data
		database_buffer_size += bulk_data_size;										// Bulk data

		uint8_t* database_buffer = allocate_type_array_aligned<uint8_t>(allocator, database_buffer_size, alignof(compressed_database));
		std::memset(database_buffer, 0, database_buffer_size);

		const uint8_t* database_buffer_start = database_buffer;
		out_merged_compressed_database = reinterpret_cast<compressed_database*>(database_buffer);

		raw_buffer_header* database_buffer_header = safe_ptr_cast<raw_buffer_header>(database_buffer);
		database_buffer += sizeof(raw_buffer_header);

		uint8_t* db_header_start = database_buffer;
		database_header* db_header = safe_ptr_cast<database_header>(database_buffer);
		database_buffer += sizeof(database_header);

		// Write our header
		db_header->tag = static_cast<uint32_t>(buffer_tag32::compressed_database);
		db_header->version = compressed_tracks_version16::latest;
		db_header->num_chunks = db_metadata.num_chunks;
		db_header->max_chunk_size = settings.max_chunk_size;
		db_header->num_clips = db_metadata.num_clips;
		db_header->num_segments = db_metadata.num_segments;
		db_header->bulk_data_size = bulk_data_size;
		db_header->set_is_bulk_data_inline(true);	// Data is always inline when merging

		database_buffer = align_to(database_buffer, 4);									// Align chunk descriptions
		database_buffer += db_metadata.num_chunks * sizeof(database_chunk_description);	// Chunk descriptions

		database_buffer = align_to(database_buffer, 4);									// Align clip hashes
		db_header->clip_metadata_offset = database_buffer - db_header_start;			// Clip metadata (only one when we compress)
		database_buffer += db_metadata.num_clips * sizeof(database_clip_metadata);		// Clip metadata

		database_buffer = align_to(database_buffer, 8);									// Align bulk data
		db_header->bulk_data_offset = database_buffer - db_header_start;				// Bulk data
		database_buffer += bulk_data_size;												// Bulk data

		// Write our chunk descriptions
		const uint32_t num_written_chunks = write_database_chunk_descriptions(settings, merge_mappings, num_merge_mappings, db_header->get_chunk_descriptions());
		ACL_ASSERT(num_written_chunks == db_metadata.num_chunks, "Unexpected amount of data written"); (void)num_written_chunks;

		// Write our clip metadata
		write_database_clip_metadata(merge_mappings, num_merge_mappings, db_header->get_clip_metadatas());

		// Write our bulk data
		const uint32_t written_bulk_data_size = write_database_bulk_data(allocator, settings, db_metadata, merge_mappings, num_merge_mappings, db_header->get_bulk_data());
		ACL_ASSERT(written_bulk_data_size == bulk_data_size, "Unexpected amount of data written"); (void)written_bulk_data_size;
		db_header->bulk_data_hash = hash32(db_header->get_bulk_data(), bulk_data_size);

		ACL_ASSERT(uint32_t(database_buffer - database_buffer_start) == database_buffer_size, "Unexpected amount of data written");

#if defined(ACL_HAS_ASSERT_CHECKS)
		// Make sure nobody overwrote our padding
		for (const uint8_t* padding = database_buffer - 15; padding < database_buffer; ++padding)
			ACL_ASSERT(*padding == 0, "Padding was overwritten");
#endif

		// Finish the raw buffer header
		database_buffer_header->size = database_buffer_size;
		database_buffer_header->hash = hash32(safe_ptr_cast<const uint8_t>(db_header), database_buffer_size - sizeof(raw_buffer_header));	// Hash everything but the raw buffer header

		return error_result();
	}
}
