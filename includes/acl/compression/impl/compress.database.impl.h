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

					chunk_size += chunk_description.size - last_chunk_padding;
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

		// Returns the size of the bulk data
		inline uint32_t write_database_bulk_data(const compression_database_settings& settings, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, uint8_t* bulk_data)
		{
			// TODO: If the last chunk is too small, merge it with the previous chunk?

			const uint32_t max_chunk_size = settings.max_chunk_size;
			const uint32_t simd_padding = 15;

			database_chunk_header* chunk_header = nullptr;
			database_chunk_segment_header* segment_chunk_headers = nullptr;

			uint32_t bulk_data_offset = 0;
			uint32_t chunk_sample_data_offset = 0;
			uint32_t chunk_size = sizeof(database_chunk_header);

			if (bulk_data != nullptr)
			{
				// Setup our chunk headers
				chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data);
				chunk_header->index = 0;	// Temporary, built later
				chunk_header->size = 0;
				chunk_header->num_segments = 0;

				segment_chunk_headers = chunk_header->get_segment_headers();
			}

			// TODO: Fixup chunk header indices afterwards

			// We first iterate to find our chunk delimitations and write our headers
			for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
			{
				const compressed_tracks& tracks = *merge_mappings[mapping_index].tracks;
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
							std::memcpy(bulk_data + bulk_data_offset, db_chunk_description.get_chunk_header(database.get_bulk_data()), db_chunk_description.size);

						bulk_data_offset += db_chunk_description.size;
						continue;
					}

					// The last chunk already has SIMD padding, remove it
					const uint32_t last_chunk_padding = (chunk_index == (num_db_chunks - 1)) ? simd_padding : 0;
					const uint32_t new_chunk_size = chunk_size + db_chunk_description.size + simd_padding - last_chunk_padding;
					if (new_chunk_size >= max_chunk_size)
					{
						// Finalize our chunk header
						if (bulk_data != nullptr)
							chunk_header->size = max_chunk_size;

						// Chunk is full, start a new one
						bulk_data_offset += max_chunk_size;
						chunk_sample_data_offset = 0;
						chunk_size = sizeof(database_chunk_header);

						// Setup our chunk headers
						if (bulk_data != nullptr)
						{
							chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + bulk_data_offset);
							chunk_header->index = 0;	// Temporary, built later
							chunk_header->size = 0;
							chunk_header->num_segments = 0;

							segment_chunk_headers = chunk_header->get_segment_headers();
						}
					}

					if (bulk_data != nullptr)
					{
						// TODO: Should we skip segments with no data?

						// Update our chunk headers
						const database_chunk_header* db_chunk_header = db_chunk_description.get_chunk_header(database.get_bulk_data());
						const database_chunk_segment_header* db_segment_chunk_headers = db_chunk_header->get_segment_headers();
						for (uint32_t segment_index = 0; segment_index < db_chunk_header->num_segments; ++segment_index)
						{
							const database_chunk_segment_header& db_segment_chunk_header = db_segment_chunk_headers[segment_index];
							database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_header->num_segments];
							segment_chunk_header.clip_hash = tracks.get_hash();
							segment_chunk_header.sample_indices = db_segment_chunk_header.sample_indices;
							segment_chunk_header.samples_offset = chunk_sample_data_offset;	// Relative to start of the sample data for now

							// Temporarily store relative to the clip header, we'll update things later to their final offset
							segment_chunk_header.clip_header_offset = 0;
							segment_chunk_header.segment_header_offset = sizeof(database_runtime_clip_header) + segment_index * sizeof(database_runtime_segment_header);

							chunk_header->num_segments++;

							uint32_t segment_data_size;
							if (segment_index + 1 < db_chunk_header->num_segments)
							{
								// Not the last segment, use the offset from the next one to calculate our size
								segment_data_size = db_segment_chunk_headers[segment_index + 1].samples_offset - db_segment_chunk_header.samples_offset;
							}
							else
							{
								// Last segment, use the end of the chunk to calculate our size
								segment_data_size = db_chunk_description.size - last_chunk_padding - db_segment_chunk_header.samples_offset;
							}

							chunk_sample_data_offset += segment_data_size;
						}
					}

					chunk_size += db_chunk_description.size - last_chunk_padding;
				}
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

			// Now that our chunk headers are written, write our sample data and do the final fixup for the headers
			if (bulk_data != nullptr)
			{
				// Reset our header pointers
				chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data);
				segment_chunk_headers = chunk_header->get_segment_headers();

				uint32_t chunk_segment_index = 0;

				// Write the sample data
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
							continue;	// Chunk is already added

						// The last chunk already has SIMD padding, remove it
						const uint32_t last_chunk_padding = (chunk_index == (num_db_chunks - 1)) ? simd_padding : 0;

						const database_chunk_header* db_chunk_header = db_chunk_description.get_chunk_header(database.get_bulk_data());
						const database_chunk_segment_header* db_segment_chunk_headers = db_chunk_header->get_segment_headers();
						for (uint32_t segment_index = 0; segment_index < db_chunk_header->num_segments; ++segment_index)
						{
							if (chunk_segment_index >= chunk_header->num_segments)
							{
								// We hit the next chunk, update our pointers
								chunk_header = add_offset_to_ptr<database_chunk_header>(chunk_header, chunk_header->size);
								segment_chunk_headers = chunk_header->get_segment_headers();
								chunk_segment_index = 0;
							}

							const database_chunk_segment_header& db_segment_chunk_header = db_segment_chunk_headers[segment_index];

							uint32_t segment_data_size;
							if (segment_index + 1 < db_chunk_header->num_segments)
							{
								// Not the last segment, use the offset from the next one to calculate our size
								segment_data_size = db_segment_chunk_headers[segment_index + 1].samples_offset - db_segment_chunk_header.samples_offset;
							}
							else
							{
								// Last segment, use the end of the chunk to calculate our size
								segment_data_size = db_chunk_description.size - last_chunk_padding - db_segment_chunk_header.samples_offset;
							}

							// Calculate the finale offset for our chunk's data relative to the bulk data start and the final header size
							const uint32_t chunk_data_offset = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(chunk_header) - bulk_data);
							const uint32_t chunk_header_size = sizeof(database_chunk_header) + chunk_header->num_segments * sizeof(database_chunk_segment_header);

							// Update the sample offset from being relative to the start of the sample data to the start of the bulk data
							database_chunk_segment_header& segment_chunk_header = segment_chunk_headers[chunk_segment_index];
							segment_chunk_header.samples_offset = chunk_data_offset + chunk_header_size + segment_chunk_header.samples_offset;

							// Copy our data
							const uint8_t* src_animated_data = db_segment_chunk_header.samples_offset.add_to(database.get_bulk_data());

							uint8_t* dst_animated_data = segment_chunk_header.samples_offset.add_to(bulk_data);

							std::memcpy(dst_animated_data, src_animated_data, segment_data_size);

							chunk_segment_index++;
						}
					}
				}

				// Fixup our headers
				uint32_t offset = 0;
				uint32_t chunk_index = 0;
				uint32_t runtime_header_offset = 0;
				uint32_t clip_header_offset = 0;
				uint32_t last_clip_hash = 0;
				while (offset < bulk_data_offset)
				{
					chunk_header = safe_ptr_cast<database_chunk_header>(bulk_data + offset);
					chunk_header->index = chunk_index++;

					segment_chunk_headers = chunk_header->get_segment_headers();
					for (uint32_t segment_index = 0; segment_index < chunk_header->num_segments; ++segment_index)
					{
						database_chunk_segment_header& header = segment_chunk_headers[segment_index];

						if (header.clip_hash != last_clip_hash)
						{
							// Added a new clip
							clip_header_offset = runtime_header_offset;

							runtime_header_offset += sizeof(database_runtime_clip_header);
							last_clip_hash = header.clip_hash;
						}

						header.clip_header_offset = clip_header_offset;
						header.segment_header_offset = header.segment_header_offset + clip_header_offset;

						runtime_header_offset += sizeof(database_runtime_segment_header);
					}

					offset += chunk_header->size;
				}
			}

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

		uint32_t num_clips = 0;
		uint32_t num_segments = 0;
		for (uint32_t mapping_index = 0; mapping_index < num_merge_mappings; ++mapping_index)
		{
			num_clips += merge_mappings[mapping_index].database->get_num_clips();
			num_segments += merge_mappings[mapping_index].database->get_num_segments();
		}

		const uint32_t num_chunks = write_database_chunk_descriptions(settings, merge_mappings, num_merge_mappings, nullptr);
		const uint32_t bulk_data_size = write_database_bulk_data(settings, merge_mappings, num_merge_mappings, nullptr);

		uint32_t database_buffer_size = 0;
		database_buffer_size += sizeof(raw_buffer_header);							// Header
		database_buffer_size += sizeof(database_header);							// Header

		database_buffer_size = align_to(database_buffer_size, 4);					// Align chunk descriptions
		database_buffer_size += num_chunks * sizeof(database_chunk_description);	// Chunk descriptions

		database_buffer_size = align_to(database_buffer_size, 4);					// Align clip hashes
		database_buffer_size += num_clips * sizeof(database_clip_metadata);			// Clip metadata (only one when we compress)

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
		db_header->num_chunks = num_chunks;
		db_header->max_chunk_size = settings.max_chunk_size;
		db_header->num_clips = num_clips;
		db_header->num_segments = num_segments;
		db_header->bulk_data_size = bulk_data_size;
		db_header->set_is_bulk_data_inline(true);	// Data is always inline when merging

		database_buffer = align_to(database_buffer, 4);								// Align chunk descriptions
		database_buffer += num_chunks * sizeof(database_chunk_description);			// Chunk descriptions

		database_buffer = align_to(database_buffer, 4);								// Align clip hashes
		db_header->clip_metadata_offset = database_buffer - db_header_start;		// Clip metadata (only one when we compress)
		database_buffer += num_clips * sizeof(database_clip_metadata);				// Clip metadata

		database_buffer = align_to(database_buffer, 8);								// Align bulk data
		db_header->bulk_data_offset = database_buffer - db_header_start;			// Bulk data
		database_buffer += bulk_data_size;											// Bulk data

		// Write our chunk descriptions
		const uint32_t num_written_chunks = write_database_chunk_descriptions(settings, merge_mappings, num_merge_mappings, db_header->get_chunk_descriptions());
		ACL_ASSERT(num_written_chunks == num_chunks, "Unexpected amount of data written"); (void)num_written_chunks;

		// Write our clip metadata
		write_database_clip_metadata(merge_mappings, num_merge_mappings, db_header->get_clip_metadatas());

		// Write our bulk data
		const uint32_t written_bulk_data_size = write_database_bulk_data(settings, merge_mappings, num_merge_mappings, db_header->get_bulk_data());
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
