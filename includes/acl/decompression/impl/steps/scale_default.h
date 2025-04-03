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

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL step_unpack_default_scales(
			step_context_t& step_context, rtm::vector4f_arg0 default_scale_,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_scales())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_scale_mode();
			if (default_mode == default_sub_track_mode::skipped)
				return;

			const packed_sub_track_types* scale_sub_track_types = step_context.scale_sub_track_types;
			const uint32_t last_entry_index = step_context.last_entry_index;
			const uint32_t padding_mask = step_context.padding_mask;
			const void** prefetch_queue_ptr = step_context.prefetch_queue_ptr;

			// Cache the next prefetch ptr to avoid reloading it each loop iteration
			// This way, the branch can easily be predicted because once we are done
			// prefetching every entry, the ptr will remain forever null and this
			// the branch is always constant: not zero for some time, then forever zero
			const void* next_prefetch_ptr = *prefetch_queue_ptr;

			// Grab our constant default scale if we have one, otherwise init with some value
			rtm::vector4f default_scale;
			if (default_mode == default_sub_track_mode::constant)
				default_scale = writer.get_constant_default_scale();
			else if (default_mode == default_sub_track_mode::legacy)
				default_scale = default_scale_;
			else
				default_scale = rtm::vector_zero();

			// Tuned to give us the right number of instructions per sub-step
			// On ARM64, 4 yields 50-71 instructions:
			//    - 3 instructions for the outer loop condition
			//    - 10 instructions for prefetching (optional)
			//    - 11 instructions to fetch the next entry (optional)
			//    - 11 instructions per set bit when bit scanning (4x unrolled)
			//    - 3 instructions for the inner bit scan loop
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

					packed_entry = scale_sub_track_types[entry_index].types;

					// Mask out everything but default sub-tracks, this way we can early out when we iterate
					// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
					// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
					// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
					// Finally, we mask out everything but the second bit for each sub-track
					// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
					// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
					packed_entry = (~packed_entry - 0x55555555) & 0xAAAAAAAA;

					// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
					const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
					packed_entry &= entry_padding_mask;

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
					num_unpacked++;

					// We have 2 bits per sub-track
					const uint32_t track_index = curr_entry_track_index + (set_bit_index / 2);

					if (!writer.skip_track_scale(track_index))
					{
						if (default_mode == default_sub_track_mode::variable)
							writer.write_scale(track_index, writer.get_variable_default_scale(track_index));
						else
							writer.write_scale(track_index, default_scale);
					}
				}
			}

			step_context.prefetch_queue_ptr = prefetch_queue_ptr;
		}

		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL step_set_default_scales(
			step_context_t& step_context, rtm::vector4f_arg0 default_scale_,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_scales())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_scale_mode();
			if (default_mode == default_sub_track_mode::skipped)
				return;

			const uint32_t num_tracks = step_context.num_tracks;
			const void** prefetch_queue_ptr = step_context.prefetch_queue_ptr;

			// Cache the next prefetch ptr to avoid reloading it each loop iteration
			// This way, the branch can easily be predicted because once we are done
			// prefetching every entry, the ptr will remain forever null and this
			// the branch is always constant: not zero for some time, then forever zero
			const void* next_prefetch_ptr = *prefetch_queue_ptr;

			// Grab our constant default scale if we have one, otherwise init with some value
			rtm::vector4f scale;
			if (default_mode == default_sub_track_mode::constant)
				scale = writer.get_constant_default_scale();
			else if (default_mode == default_sub_track_mode::legacy)
				scale = default_scale_;
			else
				scale = rtm::vector_zero();

			// Tuned to give us the right number of instructions per sub-step
			// On ARM64:
			//    - 11 instructions for prefetching (optional)
			//    - 6 instructions per track index

			// Inner loop is 6 instructions per track index
			// Prefetch every 8 tracks (~48 instructions)
			constexpr uint32_t k_prefetch_multiple = 8;

			// No scale present, everything is just the default value
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				// Our step takes about 50 instructions and so we want to prefetch
				// 4 cache lines into the L2
				if (next_prefetch_ptr && (track_index % k_prefetch_multiple) == 0)
				{
					memory_prefetch_into_L2(next_prefetch_ptr);
					memory_prefetch_into_L2(prefetch_queue_ptr[1]);
					memory_prefetch_into_L2(prefetch_queue_ptr[2]);
					memory_prefetch_into_L2(prefetch_queue_ptr[3]);

					prefetch_queue_ptr += 4;
					next_prefetch_ptr = *prefetch_queue_ptr;
				}

				if (!writer.skip_track_scale(track_index))
				{
					if (default_mode == default_sub_track_mode::variable)
						writer.write_scale(track_index, writer.get_variable_default_scale(track_index));
					else
						writer.write_scale(track_index, scale);
				}
			}

			step_context.prefetch_queue_ptr = prefetch_queue_ptr;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
