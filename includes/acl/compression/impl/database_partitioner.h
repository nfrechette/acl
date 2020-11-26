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

				if (segment.num_samples < 3)
					continue;	// Not enough samples, nothing to drop

				// If we have an odd number of samples, we retain the first and last samples and drop every other sample (the odd samples)
				//    e.g: [0, 1, 2, 3, 4] = high [0, 2, 4] + low [1, 3]
				// If we have an even number of samples, we retain the first two and last samples and drop every other sample (the even samples)
				//    e.g: [0, 1, 2, 3, 4, 5] = high [0, 1, 3, 5] + low [2, 4]
				const bool drop_odd_samples = (segment.num_samples % 2) != 0;

				const uint32_t first_sample = drop_odd_samples ? 0 : 1;
				for (uint32_t sample_index = first_sample; sample_index < segment.num_samples - 1; ++sample_index)
				{
					if (drop_odd_samples)
					{
						// Drop odd samples
						if ((sample_index % 2) != 0)
							segment.sample_tiers[sample_index] = database_tier8::low_importance;
					}
					else
					{
						// Drop even samples
						if ((sample_index % 2) == 0)
							segment.sample_tiers[sample_index] = database_tier8::low_importance;
					}
				}
			}
		}

		// Returns the number of chunks written
		inline uint32_t write_database_chunk_descriptions(const clip_context& clip, const compression_database_settings& settings, database_chunk_description* chunk_descriptions)
		{
			if (clip.segments->sample_tiers == nullptr)
				return 0;	// No tiered sample data

			const uint32_t max_chunk_size = settings.max_chunk_size;
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
				ACL_ASSERT(segment_data_size + simd_padding + sizeof(database_chunk_segment_header) <= max_chunk_size, "Segment is larger than our max chunk size");

				const uint32_t new_chunk_size = chunk_size + segment_data_size + simd_padding + sizeof(database_chunk_segment_header);
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

				chunk_size += segment_data_size + sizeof(database_chunk_segment_header);

				ACL_ASSERT(chunk_size <= max_chunk_size, "Expected a valid chunk size, segment is larger than max chunk size?");
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
		inline uint32_t write_database_bulk_data(const clip_context& clip, const compression_database_settings& settings, uint32_t clip_hash, uint8_t* bulk_data, const uint32_t* output_bone_mapping, uint32_t num_output_bones)
		{
			if (clip.segments->sample_tiers == nullptr)
				return 0;	// No tiered sample data

			// TODO: If the last chunk is too small, merge it with the previous chunk?

			const uint32_t max_chunk_size = settings.max_chunk_size;
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
				if (new_chunk_size >= max_chunk_size)
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

						segment_chunk_headers = chunk_header->get_segment_headers();
					}
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
					segment_chunk_header.segment_header_offset = sizeof(database_runtime_clip_header) + segment.segment_index * sizeof(database_runtime_segment_header);

					chunk_header->num_segments++;
				}

				chunk_size += segment_data_size + sizeof(database_chunk_segment_header);
				chunk_sample_data_offset += segment_data_size;

				ACL_ASSERT(chunk_size <= max_chunk_size, "Expected a valid chunk size, segment is larger than max chunk size?");
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
