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

	inline error_result merge_compressed_databases(iallocator& allocator, const database_merge_mapping* merge_mappings, uint32_t num_merge_mappings, compressed_database*& out_merged_compressed_database)
	{
		(void)allocator;
		(void)merge_mappings;
		(void)num_merge_mappings;
		out_merged_compressed_database = nullptr;
		// TODO!
		return error_result("not implemented");
	}
}
