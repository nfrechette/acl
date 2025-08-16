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
		void step_unpack_constant_translations(
			const packed_sub_track_types* translation_sub_track_types,
			uint32_t last_entry_index,
			const uint8_t*	constant_data_translations,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_translations())
				return;

			const packed_sub_track_types* translation_sub_track_types_last = translation_sub_track_types + last_entry_index;

			uint32_t track_index = 0;

			while (translation_sub_track_types <= translation_sub_track_types_last)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, translation_sub_track_types->types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;
				translation_sub_track_types++;

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

						const uint8_t* translation_ptr = constant_data_translations;
						constant_data_translations += sizeof(rtm::float3f);

						if (!writer.skip_track_translation(track_index0))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index0, translation);
						}
					}

					if ((packed_group & 0x10000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						const uint8_t* translation_ptr = constant_data_translations;
						constant_data_translations += sizeof(rtm::float3f);

						if (!writer.skip_track_translation(track_index1))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index1, translation);
						}
					}

					if ((packed_group & 0x04000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						const uint8_t* translation_ptr = constant_data_translations;
						constant_data_translations += sizeof(rtm::float3f);

						if (!writer.skip_track_translation(track_index2))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index2, translation);
						}
					}

					if ((packed_group & 0x01000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						const uint8_t* translation_ptr = constant_data_translations;
						constant_data_translations += sizeof(rtm::float3f);

						if (!writer.skip_track_translation(track_index3))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index3, translation);
						}
					}
				}
			}
		}
#elif ACL_IMPL_STEP_CURRENT_VARIANT == 1 // 1: Hybrid unrolled/CLZ bit scanning
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK
		void RTM_SIMD_CALL step_unpack_constant_translations(
			const packed_sub_track_types* translation_sub_track_types,
			uint32_t last_entry_index,
			const uint8_t*	constant_data_translations,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_translations())
				return;

			const packed_sub_track_types* translation_sub_track_types_last = translation_sub_track_types + last_entry_index;

			uint32_t track_index = 0;

			while (translation_sub_track_types <= translation_sub_track_types_last)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, translation_sub_track_types->types);

				// We have 2 bits per sub-track
				uint32_t curr_entry_track_index = track_index;
				track_index += 16;
				translation_sub_track_types++;

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

						if ((packed_group & 0x40000000) != 0)
						{
							const uint32_t track_index0 = curr_group_track_index + 0;

							const uint8_t* translation_ptr = constant_data_translations;
							constant_data_translations += sizeof(rtm::float3f);

							if (!writer.skip_track_translation(track_index0))
							{
								const rtm::vector4f translation = rtm::vector_load(translation_ptr);
								ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

								writer.write_translation(track_index0, translation);
							}
						}

						if ((packed_group & 0x10000000) != 0)
						{
							const uint32_t track_index1 = curr_group_track_index + 1;

							const uint8_t* translation_ptr = constant_data_translations;
							constant_data_translations += sizeof(rtm::float3f);

							if (!writer.skip_track_translation(track_index1))
							{
								const rtm::vector4f translation = rtm::vector_load(translation_ptr);
								ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

								writer.write_translation(track_index1, translation);
							}
						}

						if ((packed_group & 0x04000000) != 0)
						{
							const uint32_t track_index2 = curr_group_track_index + 2;

							const uint8_t* translation_ptr = constant_data_translations;
							constant_data_translations += sizeof(rtm::float3f);

							if (!writer.skip_track_translation(track_index2))
							{
								const rtm::vector4f translation = rtm::vector_load(translation_ptr);
								ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

								writer.write_translation(track_index2, translation);
							}
						}

						if ((packed_group & 0x01000000) != 0)
						{
							const uint32_t track_index3 = curr_group_track_index + 3;

							const uint8_t* translation_ptr = constant_data_translations;
							constant_data_translations += sizeof(rtm::float3f);

							if (!writer.skip_track_translation(track_index3))
							{
								const rtm::vector4f translation = rtm::vector_load(translation_ptr);
								ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

								writer.write_translation(track_index3, translation);
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

						const uint8_t* translation_ptr = constant_data_translations;
						constant_data_translations += sizeof(rtm::float3f);

						if (!writer.skip_track_translation(curr_track_index))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(curr_track_index, translation);
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
