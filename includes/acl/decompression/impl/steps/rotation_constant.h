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
#if 0
		template<class decompression_settings_type>
		RTM_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void unpack_constant_rotation_group(
			const persistent_transform_decompression_context_v0& context,
			const uint8_t*& constant_data_rotations,
			uint32_t& num_left_to_unpack,
			rtm::vector4f out_result[4])
		{
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(context.rotation_format);
			const uint8_t* constant_track_data = constant_data_rotations;

			uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
			num_left_to_unpack = num_left_to_unpack - num_to_unpack;

			if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
			{
				switch (num_to_unpack)
				{
					case 4:
						out_result[3] = unpack_quat_128(constant_track_data + sizeof(rtm::float4f) * 3);
					case 3:
						out_result[2] = unpack_quat_128(constant_track_data + sizeof(rtm::float4f) * 2);
					case 2:
						out_result[1] = unpack_quat_128(constant_track_data + sizeof(rtm::float4f) * 1);
					case 1:
					default:
						out_result[0] = unpack_quat_128(constant_track_data + sizeof(rtm::float4f) * 0);
				}

				constant_data_rotations += sizeof(rtm::float4f) * num_to_unpack;
			}
			else
			{
				// Unpack
				// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
				// The last group contains no padding so we have to make to align our reads properly
				const uint32_t load_size = num_to_unpack * sizeof(float);

				rtm::vector4f xxxx = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 0));
				rtm::vector4f yyyy = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 1));
				rtm::vector4f zzzz = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 2));

				// Update our read ptr
				constant_data_rotations += load_size * 3;

				rtm::vector4f wwww = quat_from_positive_w_x4(xxxx, yyyy, zzzz);

				// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
				// isn't very accurate on small inputs, we need to normalize
				if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
					quat_normalize_x4(xxxx, yyyy, zzzz, wwww);

#if defined(RTM_NEON_INTRINSICS) && 0
				// This is much more compact, assemly wise, however it appears to be slower
				// Store interleaved
				// TODO: Measure perf with/without
				const float32x4x4_t result = { xxxx, yyyy, zzzz, wwww };
				vst4q_f32(bit_cast<float*>(out_result), result);
#else
				rtm::vector4f sample0;
				rtm::vector4f sample1;
				rtm::vector4f sample2;
				rtm::vector4f sample3;
				RTM_MATRIXF_TRANSPOSE_4X4(xxxx, yyyy, zzzz, wwww, sample0, sample1, sample2, sample3);

				// Cache
				out_result[0] = rtm::vector_to_quat(sample0);
				out_result[1] = rtm::vector_to_quat(sample1);
				out_result[2] = rtm::vector_to_quat(sample2);
				out_result[3] = rtm::vector_to_quat(sample3);
#endif
			}
		}
#endif

#if ACL_IMPL_STEP_CURRENT_VARIANT == 0 // ACL 2.1 (baseline + minor tweaks)
		template<class decompression_settings_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK
		void step_unpack_constant_rotations(
			const packed_sub_track_types* rotation_sub_track_types,
			uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& decomp_context,
			constant_track_cache_v0& constant_track_cache,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			const packed_sub_track_types* rotation_sub_track_types_last = rotation_sub_track_types + last_entry_index;

			uint32_t track_index = 0;

			constant_track_cache_read_cursor_v0 read_cursor;

			while (rotation_sub_track_types <= rotation_sub_track_types_last)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, rotation_sub_track_types->types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;
				rotation_sub_track_types++;

				// Unpack our next 16 tracks
				constant_track_cache.unpack_rotation_group<decompression_settings_type>(decomp_context, read_cursor);

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
						const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!writer.skip_track_rotation(track_index0))
							writer.write_rotation(track_index0, rotation);
					}

					if ((packed_group & 0x10000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;
						const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!writer.skip_track_rotation(track_index1))
							writer.write_rotation(track_index1, rotation);
					}

					if ((packed_group & 0x04000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;
						const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!writer.skip_track_rotation(track_index2))
							writer.write_rotation(track_index2, rotation);
					}

					if ((packed_group & 0x01000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;
						const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!writer.skip_track_rotation(track_index3))
							writer.write_rotation(track_index3, rotation);
					}
				}
			}
		}
#elif ACL_IMPL_STEP_CURRENT_VARIANT == 1 // 1: Hybrid unrolled/CLZ bit scanning
		template<class decompression_settings_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL step_unpack_constant_rotations(
			const packed_sub_track_types* rotation_sub_track_types,
			uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& decomp_context,
			constant_track_cache_v0& constant_track_cache,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			const packed_sub_track_types* rotation_sub_track_types_last = rotation_sub_track_types + last_entry_index;

			uint32_t track_index = 0;

			constant_track_cache_read_cursor_v0 read_cursor;

			while (rotation_sub_track_types <= rotation_sub_track_types_last)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, rotation_sub_track_types->types);

				// We have 2 bits per sub-track
				uint32_t curr_entry_track_index = track_index;
				track_index += 16;
				rotation_sub_track_types++;

				// Unpack our next 16 tracks
				constant_track_cache.unpack_rotation_group<decompression_settings_type>(decomp_context, read_cursor);

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
							const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
							ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
							ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

							if (!writer.skip_track_rotation(track_index0))
								writer.write_rotation(track_index0, rotation);
						}

						if ((packed_group & 0x10000000) != 0)
						{
							const uint32_t track_index1 = curr_group_track_index + 1;
							const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
							ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
							ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

							if (!writer.skip_track_rotation(track_index1))
								writer.write_rotation(track_index1, rotation);
						}

						if ((packed_group & 0x04000000) != 0)
						{
							const uint32_t track_index2 = curr_group_track_index + 2;
							const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
							ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
							ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

							if (!writer.skip_track_rotation(track_index2))
								writer.write_rotation(track_index2, rotation);
						}

						if ((packed_group & 0x01000000) != 0)
						{
							const uint32_t track_index3 = curr_group_track_index + 3;
							const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
							ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
							ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

							if (!writer.skip_track_rotation(track_index3))
								writer.write_rotation(track_index3, rotation);
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
						const rtm::quatf& rotation = constant_track_cache.consume_rotation(read_cursor);
						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!writer.skip_track_rotation(curr_track_index))
							writer.write_rotation(curr_track_index, rotation);
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
