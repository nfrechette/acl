#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2025 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/impl/steps/step_context.h"

#include <rtm/math.h>

#include <cstdint>

// Controls which variant to use for this decompression step
// 0: ACL 2.1 (baseline + minor tweaks)
#define ACL_IMPL_STEP_CURRENT_VARIANT 0

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
#if 0
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL step_unpack_constant_scales(
			step_context_t& step_context,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_scales())
				return;

			const packed_sub_track_types* scale_sub_track_types = step_context.scale_sub_track_types;
			const uint8_t* constant_data_scales = step_context.constant_data_scales;
			const uint32_t last_entry_index = step_context.last_entry_index;
			const void** prefetch_queue_ptr = step_context.prefetch_queue_ptr;

			// Cache the next prefetch ptr to avoid reloading it each loop iteration
			// This way, the branch can easily be predicted because once we are done
			// prefetching every entry, the ptr will remain forever null and this
			// the branch is always constant: not zero for some time, then forever zero
			const void* next_prefetch_ptr = *prefetch_queue_ptr;

			// Tuned to give us the right number of instructions per sub-step
			// On ARM64, 4 yields 60-79 instructions:
			//    - 8 instructions for inner loop
			//    - 9 instructions to fetch the next entry (optional)
			//    - 10 instructions for prefetching (optional)
			//    - 13 instructions per set bit when bit scanning (4x unrolled)
			constexpr uint32_t k_sub_step_unpack_count = 4;

			uint32_t num_unpacked = 0;
			uint32_t curr_entry_track_index = 0;
			uint32_t entry_index = 0;
			uint32_t packed_entry = 0;

			// This is our sub-step loop
			while (true)
			{
				// If our entry is empty, grab the next one
				if (packed_entry == 0)
				{
					if (entry_index > last_entry_index)
						break;	// We are done

					// Mask out everything but constant sub-tracks, this way we can early out when we iterate
					// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
					packed_entry = and_not(~0x55555555U, scale_sub_track_types[entry_index].types);

					// We have 2 bits per sub-track
					curr_entry_track_index = entry_index * 16;

					entry_index++;
				}

				// Reset our unpack count if it is k_sub_step_unpack_count
				num_unpacked %= k_sub_step_unpack_count;

				// Our sub-step loop takes about 60 instructions and so we want to prefetch
				// 4 cache lines into the L2 each iteration
				if (next_prefetch_ptr && num_unpacked == 0)
				{
					memory_prefetch_into_L2(next_prefetch_ptr);
					memory_prefetch_into_L2(prefetch_queue_ptr[1]);
					memory_prefetch_into_L2(prefetch_queue_ptr[2]);
					memory_prefetch_into_L2(prefetch_queue_ptr[3]);

					prefetch_queue_ptr += 4;
					next_prefetch_ptr = *prefetch_queue_ptr;
				}

				// While we have entries, unpack up to k_sub_step_unpack_count
				while (packed_entry != 0 && num_unpacked < k_sub_step_unpack_count)
				{
					const uint32_t set_bit_index = count_leading_zeros(packed_entry);
					const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

					// Mask out the bit we just consumed
					packed_entry ^= highest_set_bit;

					// We have 2 bits per sub-track
					const uint32_t track_index = curr_entry_track_index + (set_bit_index / 2);

					const uint8_t* scale_ptr = constant_data_scales;
					constant_data_scales += sizeof(rtm::float3f);

					if (!writer.skip_track_scale(track_index))
						writer.write_scale(track_index, rtm::vector_load(scale_ptr));

					num_unpacked++;
				}
			}

			step_context.prefetch_queue_ptr = prefetch_queue_ptr;
		}
#elif ACL_IMPL_STEP_CURRENT_VARIANT == 0 // ACL 2.1 (baseline + minor tweaks)
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL step_unpack_constant_scales(
			step_context_t& step_context,
			constant_track_cache_v0& constant_track_cache,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_scales())
				return;

			const packed_sub_track_types* scale_sub_track_types = step_context.scale_sub_track_types;
			const uint32_t last_entry_index = step_context.last_entry_index;
			const uint8_t* constant_data_scales = constant_track_cache.constant_data_scales;

			const packed_sub_track_types* scale_sub_track_types_last = scale_sub_track_types + last_entry_index;

			uint32_t track_index = 0;

			while (scale_sub_track_types <= scale_sub_track_types_last)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, scale_sub_track_types->types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;
				scale_sub_track_types++;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0x55000000) == 0)
						continue;	// This group contains no constant sub-tracks, skip it

					if ((packed_group & 0x40000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						const uint8_t* scale_ptr = constant_data_scales;
						constant_data_scales += sizeof(rtm::float3f);

						if (!writer.skip_track_scale(track_index0))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index0, scale);
						}
					}

					if ((packed_group & 0x10000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						const uint8_t* scale_ptr = constant_data_scales;
						constant_data_scales += sizeof(rtm::float3f);

						if (!writer.skip_track_scale(track_index1))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index1, scale);
						}
					}

					if ((packed_group & 0x04000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						const uint8_t* scale_ptr = constant_data_scales;
						constant_data_scales += sizeof(rtm::float3f);

						if (!writer.skip_track_scale(track_index2))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index2, scale);
						}
					}

					if ((packed_group & 0x01000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						const uint8_t* scale_ptr = constant_data_scales;
						constant_data_scales += sizeof(rtm::float3f);

						if (!writer.skip_track_scale(track_index3))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index3, scale);
						}
					}
				}
			}
		}
#endif
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

#undef ACL_IMPL_STEP_CURRENT_VARIANT

ACL_IMPL_FILE_PRAGMA_POP
