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

// Included only once from database.h

#include "acl/core/bitset.h"
#include "acl/core/compressed_database.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/database/impl/database_context.h"

#include <cstdint>

namespace acl
{
	namespace acl_impl
	{
		inline uint32_t calculate_runtime_data_size(const compressed_database& database)
		{
			const uint32_t num_chunks = database.get_num_chunks();
			const uint32_t num_clips = database.get_num_clips();
			const uint32_t num_segments = database.get_num_segments();

			const bitset_description desc = bitset_description::make_from_num_bits(num_chunks);

			uint32_t runtime_data_size = 0;
			runtime_data_size += desc.get_num_bytes();	// Loaded chunks
			runtime_data_size += desc.get_num_bytes();	// Streaming chunks
			runtime_data_size += num_clips * sizeof(database_runtime_clip_header);
			runtime_data_size += num_segments * sizeof(database_runtime_segment_header);

			return runtime_data_size;
		}
	}

	template<class database_settings_type>
	inline database_context<database_settings_type>::database_context()
		: m_context()
	{
		m_context.reset();

		static_assert(offsetof(database_context<database_settings_type>, m_context) == 0, "m_context must be the first data member!");
	}

	template<class database_settings_type>
	inline database_context<database_settings_type>::~database_context()
	{
		reset();
	}

	template<class database_settings_type>
	inline const compressed_database* database_context<database_settings_type>::get_compressed_database() const { return m_context.get_compressed_database(); }

	template<class database_settings_type>
	inline bool database_context<database_settings_type>::initialize(iallocator& allocator, const compressed_database& database)
	{
		const bool is_valid = database.is_valid(false).empty();
		ACL_ASSERT(is_valid, "Invalid compressed database instance");
		if (!is_valid)
			return false;

		ACL_ASSERT(database.get_version() == compressed_tracks_version16::v02_00_00, "Unsupported version");
		if (database.get_version() != compressed_tracks_version16::v02_00_00)
			return false;

		ACL_ASSERT(database.is_bulk_data_inline(), "Bulk data must be inline when initializing without a streamer");
		if (!database.is_bulk_data_inline())
			return false;

		ACL_ASSERT(!is_initialized(), "Cannot initialize database twice");
		if (is_initialized())
			return false;

		m_context.db = &database;
		m_context.allocator = &allocator;
		m_context.bulk_data = database.get_bulk_data();
		m_context.streamer = nullptr;

		const uint32_t num_chunks = database.get_num_chunks();
		const bitset_description desc = bitset_description::make_from_num_bits(num_chunks);

		// Allocate a single buffer for everything we need. This is faster to allocate and it ensures better virtual
		// memory locality which should help reduce the cost of TLB misses.
		const uint32_t runtime_data_size = acl_impl::calculate_runtime_data_size(database);
		uint8_t* runtime_data_buffer = allocate_type_array<uint8_t>(allocator, runtime_data_size);

		// Initialize everything to 0
		std::memset(runtime_data_buffer, 0, runtime_data_size);

		m_context.loaded_chunks = reinterpret_cast<uint32_t*>(runtime_data_buffer);
		runtime_data_buffer += desc.get_num_bytes();

		m_context.streaming_chunks = reinterpret_cast<uint32_t*>(runtime_data_buffer);
		runtime_data_buffer += desc.get_num_bytes();

		m_context.clip_segment_headers = runtime_data_buffer;

		// Copy our clip hashes to setup our headers
		const acl_impl::database_header& header = acl_impl::get_database_header(database);
		const uint32_t num_clips = header.num_clips;
		const acl_impl::database_clip_metadata* clip_metadatas = header.get_clip_metadatas();
		for (uint32_t clip_index = 0; clip_index < num_clips; ++clip_index)
		{
			const acl_impl::database_clip_metadata& clip_metadata = clip_metadatas[clip_index];
			acl_impl::database_runtime_clip_header* clip_header = clip_metadata.get_clip_header(m_context.clip_segment_headers);
			clip_header->clip_hash = clip_metadata.clip_hash;
		}

		// Bulk data is inline so stream everything in right away
		const acl_impl::database_chunk_description* chunk_descriptions = header.get_chunk_descriptions();
		for (uint32_t chunk_index = 0; chunk_index < num_chunks; ++chunk_index)
		{
			const acl_impl::database_chunk_description& chunk_description = chunk_descriptions[chunk_index];
			const acl_impl::database_chunk_header* chunk_header = chunk_description.get_chunk_header(m_context.bulk_data);
			ACL_ASSERT(chunk_header->index == chunk_index, "Unexpected chunk index");

			const acl_impl::database_chunk_segment_header* chunk_segment_headers = chunk_header->get_segment_headers();
			const uint32_t num_segments = chunk_header->num_segments;
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				const acl_impl::database_chunk_segment_header& chunk_segment_header = chunk_segment_headers[segment_index];

				acl_impl::database_runtime_clip_header* clip_header = chunk_segment_header.get_clip_header(m_context.clip_segment_headers);
				ACL_ASSERT(clip_header->clip_hash == chunk_segment_header.clip_hash, "Unexpected clip hash"); (void)clip_header;

				acl_impl::database_runtime_segment_header* segment_header = chunk_segment_header.get_segment_header(m_context.clip_segment_headers);
				if (segment_header->tier_metadata[0].load(std::memory_order::memory_order_relaxed) == 0)
					segment_header->tier_metadata[0].store((uint64_t(chunk_segment_header.samples_offset) << 32) | chunk_segment_header.sample_indices, std::memory_order::memory_order_relaxed);
				else
					segment_header->tier_metadata[1].store((uint64_t(chunk_segment_header.samples_offset) << 32) | chunk_segment_header.sample_indices, std::memory_order::memory_order_relaxed);
			}

			bitset_set(m_context.loaded_chunks, desc, chunk_index, true);
		}

		return true;
	}

	template<class database_settings_type>
	inline bool database_context<database_settings_type>::initialize(iallocator& allocator, const compressed_database& database, idatabase_streamer& streamer)
	{
		const bool is_valid = database.is_valid(false).empty();
		ACL_ASSERT(is_valid, "Invalid compressed database instance");
		if (!is_valid)
			return false;

		ACL_ASSERT(database.get_version() == compressed_tracks_version16::v02_00_00, "Unsupported version");
		if (database.get_version() != compressed_tracks_version16::v02_00_00)
			return false;

		ACL_ASSERT(streamer.is_initialized(), "Streamer must be initialized");
		if (!streamer.is_initialized())
			return false;

		const uint8_t* bulk_data = streamer.get_bulk_data();
		ACL_ASSERT(bulk_data != nullptr, "Streamer must have bulk data allocated");
		if (bulk_data == nullptr)
			return false;

		ACL_ASSERT(!is_initialized(), "Cannot initialize database twice");
		if (is_initialized())
			return false;

		m_context.db = &database;
		m_context.allocator = &allocator;
		m_context.bulk_data = bulk_data;
		m_context.streamer = &streamer;

		const uint32_t num_chunks = database.get_num_chunks();
		const bitset_description desc = bitset_description::make_from_num_bits(num_chunks);

		// Allocate a single buffer for everything we need. This is faster to allocate and it ensures better virtual
		// memory locality which should help reduce the cost of TLB misses.
		const uint32_t runtime_data_size = acl_impl::calculate_runtime_data_size(database);
		uint8_t* runtime_data_buffer = allocate_type_array<uint8_t>(allocator, runtime_data_size);

		// Initialize everything to 0
		std::memset(runtime_data_buffer, 0, runtime_data_size);

		m_context.loaded_chunks = reinterpret_cast<uint32_t*>(runtime_data_buffer);
		runtime_data_buffer += desc.get_num_bytes();

		m_context.streaming_chunks = reinterpret_cast<uint32_t*>(runtime_data_buffer);
		runtime_data_buffer += desc.get_num_bytes();

		m_context.clip_segment_headers = runtime_data_buffer;

		// Copy our clip hashes to setup our headers
		const acl_impl::database_header& header = acl_impl::get_database_header(database);
		const uint32_t num_clips = header.num_clips;
		const acl_impl::database_clip_metadata* clip_metadatas = header.get_clip_metadatas();
		for (uint32_t clip_index = 0; clip_index < num_clips; ++clip_index)
		{
			const acl_impl::database_clip_metadata& clip_metadata = clip_metadatas[clip_index];
			acl_impl::database_runtime_clip_header* clip_header = clip_metadata.get_clip_header(m_context.clip_segment_headers);
			clip_header->clip_hash = clip_metadata.clip_hash;
		}

		return true;
	}

	template<class database_settings_type>
	inline bool database_context<database_settings_type>::is_initialized() const { return m_context.is_initialized(); }

	template<class database_settings_type>
	inline void database_context<database_settings_type>::reset()
	{
		if (!is_initialized())
			return;	// Nothing to do

		// TODO: Assert we aren't streaming in progress, otherwise behavior is undefined we'll access freed memory

		const uint32_t runtime_data_size = acl_impl::calculate_runtime_data_size(*m_context.db);
		deallocate_type_array(*m_context.allocator, m_context.loaded_chunks, runtime_data_size);

		// Just reset the DB pointer, this will mark us as no longer initialized indicating everything is stale
		m_context.db = nullptr;
	}

	template<class database_settings_type>
	inline bool database_context<database_settings_type>::contains(const compressed_tracks& tracks) const
	{
		ACL_ASSERT(is_initialized(), "Database isn't initialized");
		if (!is_initialized())
			return false;

		if (!tracks.has_database())
			return false;	// Clip not bound to anything

		const acl_impl::transform_tracks_header& transform_header = acl_impl::get_transform_tracks_header(tracks);
		const acl_impl::tracks_database_header* tracks_db_header = transform_header.get_database_header();
		ACL_ASSERT(tracks_db_header != nullptr, "Expected a 'tracks_database_header'");

		if (!tracks_db_header->clip_header_offset.is_valid())
			return false;	// Invalid clip header offset

		const uint32_t num_clips = m_context.db->get_num_clips();
		const uint32_t num_segments = m_context.db->get_num_segments();

		uint32_t largest_offset = 0;
		largest_offset += num_clips * sizeof(acl_impl::database_runtime_clip_header);
		largest_offset += num_segments * sizeof(acl_impl::database_runtime_segment_header);

		// We can't read past the end of the last entry
		largest_offset -= sizeof(acl_impl::database_runtime_segment_header);

		if (tracks_db_header->clip_header_offset > largest_offset)
			return false;	// Invalid clip header offset

		const acl_impl::database_runtime_clip_header* db_clip_header = tracks_db_header->get_clip_header(m_context.clip_segment_headers);
		if (db_clip_header->clip_hash != tracks.get_hash())
			return false;	// Clip not bound to this database instance

		// All good
		return true;
	}

	template<class database_settings_type>
	inline bool database_context<database_settings_type>::is_streamed_in() const
	{
		ACL_ASSERT(is_initialized(), "Database isn't initialized");
		if (!is_initialized())
			return false;

		const uint32_t num_chunks = m_context.db->get_num_chunks();
		const bitset_description desc = bitset_description::make_from_num_bits(num_chunks);

		const uint32_t num_loaded_chunks = bitset_count_set_bits(m_context.loaded_chunks, desc);

		return num_loaded_chunks == num_chunks;
	}

	template<class database_settings_type>
	inline bool database_context<database_settings_type>::is_streaming() const
	{
		ACL_ASSERT(is_initialized(), "Database isn't initialized");
		if (!is_initialized())
			return false;

		const uint32_t num_chunks = m_context.db->get_num_chunks();
		const bitset_description desc = bitset_description::make_from_num_bits(num_chunks);

		const uint32_t num_streaming_chunks = bitset_count_set_bits(m_context.streaming_chunks, desc);

		return num_streaming_chunks != 0;
	}

	template<class database_settings_type>
	inline database_stream_request_result database_context<database_settings_type>::stream_in(uint32_t num_chunks_to_stream)
	{
		ACL_ASSERT(is_initialized(), "Database isn't initialized");
		if (!is_initialized())
			return database_stream_request_result::not_initialized;

		if (is_streaming())
			return database_stream_request_result::streaming;	// Can't stream while we are streaming

		const acl_impl::database_header& header = acl_impl::get_database_header(*m_context.db);
		const acl_impl::database_chunk_description* chunk_descriptions = header.get_chunk_descriptions();

		const uint32_t num_chunks = m_context.db->get_num_chunks();
		const bitset_description desc = bitset_description::make_from_num_bits(num_chunks);
		const uint32_t max_chunk_size = header.max_chunk_size;

		uint32_t first_chunk_index = ~0U;

		// Clamp the total number of chunks we can stream
		num_chunks_to_stream = std::min<uint32_t>(num_chunks_to_stream, num_chunks);

		// Look for chunks that aren't loaded yet and aren't streaming yet
		const uint32_t num_entries = desc.get_size();
		for (uint32_t entry_index = 0; entry_index < num_entries; ++entry_index)
		{
			const uint32_t maybe_loaded = m_context.loaded_chunks[entry_index];
			const uint32_t num_pending_chunks = count_trailing_zeros(maybe_loaded);
			if (num_pending_chunks != 0)
			{
				first_chunk_index = (entry_index * 32) + (32 - num_pending_chunks);
				break;
			}
		}

		if (first_chunk_index == ~0U)
			return database_stream_request_result::done;	// Everything is streamed in or streaming, nothing to do

		// Calculate and clamp our last chunk index (and handle wrapping for safety)
		const uint64_t last_chunk_index64 = uint64_t(first_chunk_index) + uint64_t(num_chunks_to_stream) - 1;
		const uint32_t last_chunk_index = last_chunk_index64 >= uint64_t(num_chunks) ? (num_chunks - 1) : uint32_t(last_chunk_index64);

		// Calculate our stream size and account for the fact that the last chunk doesn't have the same size
		const uint32_t num_streaming_chunks = last_chunk_index - first_chunk_index + 1;

		if (num_streaming_chunks == 0)
			return database_stream_request_result::done;	// Nothing more to stream

		// Find the stream start offset from our first chunk's offset
		const acl_impl::database_chunk_description& first_chunk_description = chunk_descriptions[first_chunk_index];
		const uint32_t stream_start_offset = first_chunk_description.offset;

		const acl_impl::database_chunk_description& last_chunk_description = chunk_descriptions[last_chunk_index];
		const uint32_t stream_size = ((num_streaming_chunks - 1) * max_chunk_size) + last_chunk_description.size;

		// Mark chunks as in-streaming
		bitset_set_range(m_context.streaming_chunks, desc, first_chunk_index, num_streaming_chunks, true);

		acl_impl::database_context_v0& context = m_context;
		auto continuation = [&context, first_chunk_index, num_streaming_chunks](bool success)
		{
			const uint32_t num_chunks_ = context.db->get_num_chunks();
			const bitset_description desc_ = bitset_description::make_from_num_bits(num_chunks_);

			if (success)
			{
				// Register our new chunks
				const acl_impl::database_header& header_ = acl_impl::get_database_header(*context.db);
				const acl_impl::database_chunk_description* chunk_descriptions_ = header_.get_chunk_descriptions();
				const uint32_t end_chunk_index = first_chunk_index + num_streaming_chunks;
				for (uint32_t chunk_index = first_chunk_index; chunk_index < end_chunk_index; ++chunk_index)
				{
					const acl_impl::database_chunk_description& chunk_description = chunk_descriptions_[chunk_index];
					const acl_impl::database_chunk_header* chunk_header = chunk_description.get_chunk_header(context.bulk_data);
					ACL_ASSERT(chunk_header->index == chunk_index, "Unexpected chunk index");

					const acl_impl::database_chunk_segment_header* chunk_segment_headers = chunk_header->get_segment_headers();
					const uint32_t num_segments = chunk_header->num_segments;
					for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
					{
						const acl_impl::database_chunk_segment_header& chunk_segment_header = chunk_segment_headers[segment_index];

						acl_impl::database_runtime_clip_header* clip_header = chunk_segment_header.get_clip_header(context.clip_segment_headers);
						ACL_ASSERT(clip_header->clip_hash == chunk_segment_header.clip_hash, "Unexpected clip hash"); (void)clip_header;

						acl_impl::database_runtime_segment_header* segment_header = chunk_segment_header.get_segment_header(context.clip_segment_headers);
						if (segment_header->tier_metadata[0].load(std::memory_order::memory_order_relaxed) == 0)
							segment_header->tier_metadata[0].store((uint64_t(chunk_segment_header.samples_offset) << 32) | chunk_segment_header.sample_indices, std::memory_order::memory_order_relaxed);
						else
							segment_header->tier_metadata[1].store((uint64_t(chunk_segment_header.samples_offset) << 32) | chunk_segment_header.sample_indices, std::memory_order::memory_order_relaxed);
					}
				}

				// Mark chunks as done streaming
				bitset_set_range(context.loaded_chunks, desc_, first_chunk_index, num_streaming_chunks, true);
			}

			// Mark chunks as no longer streaming
			bitset_set_range(context.streaming_chunks, desc_, first_chunk_index, num_streaming_chunks, false);
		};

		m_context.streamer->stream_in(stream_start_offset, stream_size, continuation);

		return database_stream_request_result::dispatched;
	}

	template<class database_settings_type>
	inline database_stream_request_result database_context<database_settings_type>::stream_out(uint32_t num_chunks_to_stream)
	{
		ACL_ASSERT(is_initialized(), "Database isn't initialized");
		if (!is_initialized())
			return database_stream_request_result::not_initialized;

		if (is_streaming())
			return database_stream_request_result::streaming;	// Can't stream while we are streaming

		const acl_impl::database_header& header = acl_impl::get_database_header(*m_context.db);
		const acl_impl::database_chunk_description* chunk_descriptions = header.get_chunk_descriptions();

		const uint32_t num_chunks = m_context.db->get_num_chunks();
		const bitset_description desc = bitset_description::make_from_num_bits(num_chunks);
		const uint32_t max_chunk_size = header.max_chunk_size;

		// Clamp the total number of chunks we can stream
		num_chunks_to_stream = std::min<uint32_t>(num_chunks_to_stream, num_chunks);

		uint32_t first_chunk_index = ~0U;

		// Look for chunks that aren't unloaded yet and aren't streaming yet
		const uint32_t num_entries = desc.get_size();
		for (uint32_t entry_index = 0; entry_index < num_entries; ++entry_index)
		{
			const uint32_t maybe_loaded = m_context.loaded_chunks[entry_index];
			const uint32_t num_pending_chunks = count_leading_zeros(maybe_loaded);
			if (num_pending_chunks != 32)
			{
				first_chunk_index = (entry_index * 32) + num_pending_chunks;
				break;
			}
		}

		if (first_chunk_index == ~0U)
			return database_stream_request_result::done;	// Everything is streamed out or streaming, nothing to do

		// Calculate and clamp our last chunk index (and handle wrapping for safety)
		const uint64_t last_chunk_index64 = uint64_t(first_chunk_index) + uint64_t(num_chunks_to_stream) - 1;
		const uint32_t last_chunk_index = last_chunk_index64 >= uint64_t(num_chunks) ? (num_chunks - 1) : uint32_t(last_chunk_index64);

		// Calculate our stream size and account for the fact that the last chunk doesn't have the same size
		const uint32_t num_streaming_chunks = last_chunk_index - first_chunk_index + 1;

		if (num_streaming_chunks == 0)
			return database_stream_request_result::done;	// Nothing more to stream

		// Find the stream start offset from our first chunk's offset
		const acl_impl::database_chunk_description& first_chunk_description = chunk_descriptions[first_chunk_index];
		const uint32_t stream_start_offset = first_chunk_description.offset;

		const acl_impl::database_chunk_description& last_chunk_description = chunk_descriptions[last_chunk_index];
		const uint32_t stream_size = ((num_streaming_chunks - 1) * max_chunk_size) + last_chunk_description.size;

		// Mark chunks as in-streaming
		bitset_set_range(m_context.streaming_chunks, desc, first_chunk_index, num_streaming_chunks, true);

		// Unregister our chunks
		const uint32_t end_chunk_index = first_chunk_index + num_streaming_chunks;
		for (uint32_t chunk_index = first_chunk_index; chunk_index < end_chunk_index; ++chunk_index)
		{
			const acl_impl::database_chunk_description& chunk_description = chunk_descriptions[chunk_index];
			const acl_impl::database_chunk_header* chunk_header = chunk_description.get_chunk_header(m_context.bulk_data);
			ACL_ASSERT(chunk_header->index == chunk_index, "Unexpected chunk index");

			const acl_impl::database_chunk_segment_header* chunk_segment_headers = chunk_header->get_segment_headers();
			const uint32_t num_segments = chunk_header->num_segments;
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				const acl_impl::database_chunk_segment_header& chunk_segment_header = chunk_segment_headers[segment_index];

				acl_impl::database_runtime_clip_header* clip_header = chunk_segment_header.get_clip_header(m_context.clip_segment_headers);
				ACL_ASSERT(clip_header->clip_hash == chunk_segment_header.clip_hash, "Unexpected clip hash"); (void)clip_header;

				acl_impl::database_runtime_segment_header* segment_header = chunk_segment_header.get_segment_header(m_context.clip_segment_headers);
				const uint64_t tier_metadata = (uint64_t(chunk_segment_header.samples_offset) << 32) | chunk_segment_header.sample_indices;
				if (segment_header->tier_metadata[0].load(std::memory_order::memory_order_relaxed) == tier_metadata)
					segment_header->tier_metadata[0].store(0, std::memory_order::memory_order_relaxed);
				else
					segment_header->tier_metadata[1].store(0, std::memory_order::memory_order_relaxed);
			}
		}

		acl_impl::database_context_v0& context = m_context;
		auto continuation = [&context, first_chunk_index, num_streaming_chunks](bool success)
		{
			const uint32_t num_chunks_ = context.db->get_num_chunks();
			const bitset_description desc_ = bitset_description::make_from_num_bits(num_chunks_);

			if (success)
			{
				// Mark chunks as done streaming out
				bitset_set_range(context.loaded_chunks, desc_, first_chunk_index, num_streaming_chunks, false);
			}

			// Mark chunks as no longer streaming
			bitset_set_range(context.streaming_chunks, desc_, first_chunk_index, num_streaming_chunks, false);
		};

		m_context.streamer->stream_out(stream_start_offset, stream_size, continuation);

		return database_stream_request_result::dispatched;
	}
}
