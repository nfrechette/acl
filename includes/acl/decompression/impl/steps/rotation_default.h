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
// 0: ACL 2.1 (baseline + minor tweaks) (aka unrolled)
// 1: Hybrid unrolled/CLZ bit scanning
#define ACL_IMPL_STEP_CURRENT_VARIANT 1

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
#if ACL_IMPL_STEP_CURRENT_VARIANT == 0 // ACL 2.1 (baseline + minor tweaks)
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK
		void step_unpack_default_rotations(
			const packed_sub_track_types* rotation_sub_track_types,
			uint32_t last_entry_index,
			uint32_t padding_mask,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_rotation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for rotations");
			if (default_mode == default_sub_track_mode::skipped)
				return;

			const packed_sub_track_types* rotation_sub_track_types_last = rotation_sub_track_types + last_entry_index;

			// Grab our constant default rotation if we have one, otherwise init with some value
			const rtm::quatf default_rotation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_rotation() : rtm::quat_identity();

			uint32_t track_index = 0;

			while (rotation_sub_track_types <= rotation_sub_track_types_last)
			{
				uint32_t packed_entry = rotation_sub_track_types->types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = ~packed_entry - 0x55555555;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = rotation_sub_track_types == rotation_sub_track_types_last ? padding_mask : 0xAAAAAAAA;
				packed_entry &= entry_padding_mask;

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;
				rotation_sub_track_types++;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0xAA000000) == 0)
						continue;	// This group contains no default sub-tracks, skip it

					if ((packed_group & 0x80000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						if (!writer.skip_track_rotation(track_index0))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index0, writer.get_variable_default_rotation(track_index0));
							else
								writer.write_rotation(track_index0, default_rotation);
						}
					}

					if ((packed_group & 0x20000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						if (!writer.skip_track_rotation(track_index1))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index1, writer.get_variable_default_rotation(track_index1));
							else
								writer.write_rotation(track_index1, default_rotation);
						}
					}

					if ((packed_group & 0x08000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						if (!writer.skip_track_rotation(track_index2))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index2, writer.get_variable_default_rotation(track_index2));
							else
								writer.write_rotation(track_index2, default_rotation);
						}
					}

					if ((packed_group & 0x02000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						if (!writer.skip_track_rotation(track_index3))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index3, writer.get_variable_default_rotation(track_index3));
							else
								writer.write_rotation(track_index3, default_rotation);
						}
					}
				}
			}
		}
#elif ACL_IMPL_STEP_CURRENT_VARIANT == 1 // 1: Hybrid unrolled/CLZ bit scanning
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK
		void step_unpack_default_rotations(
			const packed_sub_track_types* rotation_sub_track_types,
			uint32_t last_entry_index,
			uint32_t padding_mask,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_rotation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for rotations");
			if (default_mode == default_sub_track_mode::skipped)
				return;

			const packed_sub_track_types* rotation_sub_track_types_last = rotation_sub_track_types + last_entry_index;

			// Grab our constant default rotation if we have one, otherwise init with some value
			const rtm::quatf default_rotation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_rotation() : rtm::quat_identity();

			uint32_t track_index = 0;

			while (rotation_sub_track_types <= rotation_sub_track_types_last)
			{
				uint32_t packed_entry = rotation_sub_track_types->types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = ~packed_entry - 0x55555555;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = rotation_sub_track_types == rotation_sub_track_types_last ? padding_mask : 0xAAAAAAAA;
				packed_entry &= entry_padding_mask;

				// We have 2 bits per sub-track
				uint32_t curr_entry_track_index = track_index;
				track_index += 16;
				rotation_sub_track_types++;

				const uint32_t num_set_bits = acl::count_set_bits(packed_entry);
				// 26 / 32 = 81.25%, we use half of that since we have 2 bits per sub-track
				if (num_set_bits >= 13)
				{
					// High density, use reference impl
					// Process 4 sub-tracks at a time
					while (packed_entry != 0)
					{
						// Requires that entries be packed LSB to MSB
						const uint32_t packed_group = packed_entry;
						const uint32_t curr_group_track_index = curr_entry_track_index;

						// Move to the next group
						packed_entry <<= 8;
						curr_entry_track_index += 4;

						if ((packed_group & 0x80000000) != 0)
						{
							const uint32_t track_index0 = curr_group_track_index + 0;

							if (!writer.skip_track_rotation(track_index0))
							{
								if (default_mode == default_sub_track_mode::variable)
									writer.write_rotation(track_index0, writer.get_variable_default_rotation(track_index0));
								else
									writer.write_rotation(track_index0, default_rotation);
							}
						}

						if ((packed_group & 0x20000000) != 0)
						{
							const uint32_t track_index1 = curr_group_track_index + 1;

							if (!writer.skip_track_rotation(track_index1))
							{
								if (default_mode == default_sub_track_mode::variable)
									writer.write_rotation(track_index1, writer.get_variable_default_rotation(track_index1));
								else
									writer.write_rotation(track_index1, default_rotation);
							}
						}

						if ((packed_group & 0x08000000) != 0)
						{
							const uint32_t track_index2 = curr_group_track_index + 2;

							if (!writer.skip_track_rotation(track_index2))
							{
								if (default_mode == default_sub_track_mode::variable)
									writer.write_rotation(track_index2, writer.get_variable_default_rotation(track_index2));
								else
									writer.write_rotation(track_index2, default_rotation);
							}
						}

						if ((packed_group & 0x02000000) != 0)
						{
							const uint32_t track_index3 = curr_group_track_index + 3;

							if (!writer.skip_track_rotation(track_index3))
							{
								if (default_mode == default_sub_track_mode::variable)
									writer.write_rotation(track_index3, writer.get_variable_default_rotation(track_index3));
								else
									writer.write_rotation(track_index3, default_rotation);
							}
						}
					}
				}
				else
				{
					// Low density, use ctz impl
					while (packed_entry != 0)
					{
						// Requires that entries be packed MSB to LSB
						const uint32_t set_bit_index = acl::count_leading_zeros(packed_entry);
						const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

						// Mask out the bit we just consumed
						packed_entry ^= highest_set_bit;

						// We have 2 bits per sub-track
						const uint32_t curr_track_index = curr_entry_track_index + (set_bit_index / 2);

						if (!writer.skip_track_rotation(curr_track_index))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(curr_track_index, writer.get_variable_default_rotation(curr_track_index));
							else
								writer.write_rotation(curr_track_index, default_rotation);
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
