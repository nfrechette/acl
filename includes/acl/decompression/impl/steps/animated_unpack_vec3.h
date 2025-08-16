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
#include "acl/decompression/impl/steps/animated_unpack_config.h"
#include "acl/decompression/impl/steps/animated_unpack_types.h"

#include <rtm/math.h>

#include <cstdint>

// Controls which variant to use for this decompression step
// 0: ACL 2.1 (baseline)
// 1: Branchless constant/raw/variable unpacking
// 2: Branchless raw/variable unpacking
#define ACL_IMPL_STEP_CURRENT_GROUP_VARIANT 2

// TODO: Unpack segment ranges in bulk
// Similar to what we do for rotations, we could swizzle the data for ranges and store in SoA form
// when we have a full group of 4 (TBD if its worth it for partial groups)
// We almost always unpack segment range data when we have segments since we are rarely constant or raw
// This would make it easy to unpack with AVX
// AVX2 adds support for _mm256_cvtepi8_epi32 which could fold the load+cvt into a single instruction
// We could interleave range unpacking for both segments
// Store unpacked range on the stack in AoS form
// Range remap would then be cheap, mul+add loading directly from memory with SSE with 2 extra loads on NEON
// We could remove the need to branch to apply it if we can somehow fixup the load ptr to point to constants
// perhaps with a cmp+cmov but I suspect the branch might still win since it'll be predicted
// For clip ranges, we almost always apply them as well meaning the branch is predicted but we could benefit
// from moving the loads earlier. They might cache miss like the sample loads and we could hide their latency.

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
#if ACL_IMPL_STEP_CURRENT_GROUP_VARIANT == 0 // ACL 2.1 (baseline)
		template<class decompression_settings_adapter_type>
		ACL_IMPL_DEBUG_FORCE_INLINE
		RTM_DISABLE_SECURITY_COOKIE_CHECK
		void unpack_grouped_animated_vector3(
			const persistent_transform_decompression_context_v0& decomp_context,
			rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack,
			const clip_animated_sampling_context_v0& clip_sampling_context,
			segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));
			const compressed_tracks_version16 version = get_version<decompression_settings_adapter_type>(decomp_context.get_version());

			// See write_format_per_track_data(..) for details
			const uint32_t num_raw_bit_rate_bits = version >= compressed_tracks_version16::v02_01_99_1 ? 31U : 32U;

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

			rtm::vector4f* output_scratch_ptr = &output_scratch[0];

			for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
			{
				// Range ignore flags are used to skip range normalization at the clip and/or segment levels
				// Each sample has two bits like so:
				//    - 0x01 = ignore segment level
				//    - 0x02 = ignore clip level
				uint32_t range_ignore_flags;

				rtm::vector4f sample;
				if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
				{
					const uint32_t num_bits_at_bit_rate = *format_per_track_data;
					format_per_track_data++;

					if (num_bits_at_bit_rate == 0)	// Constant bit rate
					{
						sample = unpack_vector3_u48_unsafe(segment_range_data);
						segment_range_data += sizeof(uint16_t) * 3;
						range_ignore_flags = 0x01;	// Skip segment only
					}
					else if (num_bits_at_bit_rate == num_raw_bit_rate_bits)	// Raw bit rate
					{
						sample = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
						animated_track_data_bit_offset += 96;
						segment_range_data += sizeof(uint16_t) * 3;	// Raw bit rates have unused range data, skip it
						range_ignore_flags = 0x03;	// Skip clip and segment
					}
					else
					{
						sample = unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, animated_track_data, animated_track_data_bit_offset);
						animated_track_data_bit_offset += num_bits_at_bit_rate * 3;
						range_ignore_flags = 0x00;	// Don't skip range reduction
					}
				}
				else if (decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full))
				{
					ACL_ASSERT(format == vector_format8::vector3f_full, "Unexpected vector format");
					sample = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
					animated_track_data_bit_offset += 96;
					range_ignore_flags = 0x03;	// Skip clip and segment
				}
				else
				{
					ACL_ASSERT(false, "Should never be reached");
					sample = rtm::vector_zero();
					range_ignore_flags = 0;
				}

				// Remap within our ranges
				if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
				{
					if (decomp_context.has_segments && (range_ignore_flags & 0x01) == 0)
					{
						// Apply segment range remapping
						const uint32_t range_entry_size = 3 * sizeof(uint8_t);
						const uint8_t* segment_range_min_ptr = segment_range_data;
						const uint8_t* segment_range_extent_ptr = segment_range_min_ptr + range_entry_size;
						segment_range_data = segment_range_extent_ptr + range_entry_size;

						const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(segment_range_min_ptr);
						const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(segment_range_extent_ptr);

						sample = rtm::vector_mul_add(sample, segment_range_extent, segment_range_min);
					}

					if ((range_ignore_flags & 0x02) == 0)
					{
						// Apply clip range remapping
						const uint32_t clip_range_min_size = 3 * sizeof(float);
						const uint8_t* clip_range_min_ptr = clip_range_data;
						const uint8_t* clip_range_extent_ptr = clip_range_data + clip_range_min_size;

						const rtm::vector4f clip_range_min = rtm::vector_load(clip_range_min_ptr);
						const rtm::vector4f clip_range_extent = rtm::vector_load(clip_range_extent_ptr);

						sample = rtm::vector_mul_add(sample, clip_range_extent, clip_range_min);
					}

					const uint32_t clip_range_data_size = 6 * sizeof(float);
					clip_range_data += clip_range_data_size;
				}

				ACL_ASSERT(rtm::vector_is_finite3(sample), "Vector3 is not valid!");

				// TODO: Fill in W component with something sensible?

				// Cache
				*output_scratch_ptr++ = sample;
			}

			// Update our pointers
			segment_sampling_context.format_per_track_data = format_per_track_data;
			segment_sampling_context.segment_range_data = segment_range_data;
			segment_sampling_context.animated_track_data_bit_offset = animated_track_data_bit_offset;

			// Prefetch the next cache line even if we don't have any data left
			// By the time we unpack again, it will have arrived in the CPU cache
			// If our format is full precision, we have at most 4 samples per cache line
			// If our format is drop W, we have at most 5.33 samples per cache line

			// If our pointer was already aligned to a cache line before we unpacked our 4 values,
			// it now points to the first byte of the next cache line. Any offset between 0-63 will fetch it.
			// If our pointer had some offset into a cache line, we might have spanned 2 cache lines.
			// If this happens, we probably already read some data from the next cache line in which
			// case we don't need to prefetch it and we can go to the next one. Any offset after the end
			// of this cache line will fetch it. For safety, we prefetch 63 bytes ahead.
			// Prefetch 4 samples ahead in all levels of the CPU cache
			ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 60);
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);
			ACL_IMPL_ANIMATED_PREFETCH(segment_range_data + 48);
		}
#elif ACL_IMPL_STEP_CURRENT_GROUP_VARIANT == 1 // 1: Branchless constant/raw/variable unpacking
		template<class decompression_settings_adapter_type>
		ACL_IMPL_DEBUG_FORCE_INLINE
		RTM_DISABLE_SECURITY_COOKIE_CHECK
		void unpack_grouped_animated_vector3(
			const persistent_transform_decompression_context_v0& decomp_context,
			rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack,
			const clip_animated_sampling_context_v0& clip_sampling_context,
			segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

			rtm::vector4f* output_scratch_ptr = &output_scratch[0];

			for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
			{
				rtm::vector4f sample;
				if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
				{
					uint32_t num_bits_at_bit_rate = *format_per_track_data;
					format_per_track_data++;

					// We unpack 3 ways and select the result we want
					// Unpacking a constant sample is cheap, from CPU L1 with a few instructions
					// Unpacking a raw sample is nearly the same as a variable sample, we can
					// do both at the same time

					// Remap 31 to 32
					// See write_format_per_track_data(..) for details
					num_bits_at_bit_rate = num_bits_at_bit_rate >= 31 ? 32 : num_bits_at_bit_rate;

					const rtm::vector4f constant_sample = unpack_vector3_u48_unsafe(segment_range_data);

					sample = unpack_vector3_mixed_unsafe(num_bits_at_bit_rate, animated_track_data, animated_track_data_bit_offset);

				#if defined(RTM_NEON64_INTRINSICS) && 0
					// This should be 2 instructions but Apple Clang generates 6 for it, disable for now
					const uint32x4_t is_constant_mask = vceqzq_u32(vmovq_n_u32(num_bits_at_bit_rate));
					sample = RTM_VECTOR4F_SELECT(is_constant_mask, constant_sample, sample);
				#elif defined(RTM_NEON_INTRINSICS)
					// Not as efficient but better than vector compare with zero which generates 6 instructions
					// This yields: sub, asr, dup, bif
					const uint32x4_t is_constant_mask = vmovq_n_u32(((int32_t)num_bits_at_bit_rate - 1) >> 31);
					sample = RTM_VECTOR4F_SELECT(is_constant_mask, constant_sample, sample);
				#else
					sample = num_bits_at_bit_rate != 0 ? sample : constant_sample;
				#endif

					animated_track_data_bit_offset += num_bits_at_bit_rate * 3;

					// Remap within our ranges
					if (decomp_context.has_segments)
					{
						// If we aren't raw or constant (32 or 0 bits)
						if ((num_bits_at_bit_rate & 0x1F) != 0)
						{
							// Apply segment range remapping
							const uint32_t range_entry_size = 3 * sizeof(uint8_t);
							const uint8_t* segment_range_min_ptr = segment_range_data;
							const uint8_t* segment_range_extent_ptr = segment_range_min_ptr + range_entry_size;

							const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(segment_range_min_ptr);
							const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(segment_range_extent_ptr);

							sample = rtm::vector_mul_add(sample, segment_range_extent, segment_range_min);
						}

						segment_range_data += sizeof(uint16_t) * 3;
					}

					// If we aren't raw (32 bits)
					if ((num_bits_at_bit_rate & 0x20) == 0)
					{
						// Apply clip range remapping
						const uint32_t clip_range_min_size = 3 * sizeof(float);
						const uint8_t* clip_range_min_ptr = clip_range_data;
						const uint8_t* clip_range_extent_ptr = clip_range_data + clip_range_min_size;

						const rtm::vector4f clip_range_min = rtm::vector_load(clip_range_min_ptr);
						const rtm::vector4f clip_range_extent = rtm::vector_load(clip_range_extent_ptr);

						sample = rtm::vector_mul_add(sample, clip_range_extent, clip_range_min);
					}

					const uint32_t clip_range_data_size = 6 * sizeof(float);
					clip_range_data += clip_range_data_size;
				}
				else if (decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full))
				{
					ACL_ASSERT(format == vector_format8::vector3f_full, "Unexpected vector format");
					sample = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
					animated_track_data_bit_offset += 96;
				}
				else
				{
					ACL_ASSERT(false, "Should never be reached");
					sample = rtm::vector_zero();
				}

				ACL_ASSERT(rtm::vector_is_finite3(sample), "Vector3 is not valid!");

				// TODO: Fill in W component with something sensible?

				// Cache
				*output_scratch_ptr++ = sample;
			}

			// Update our pointers
			segment_sampling_context.format_per_track_data = format_per_track_data;
			segment_sampling_context.segment_range_data = segment_range_data;
			segment_sampling_context.animated_track_data_bit_offset = animated_track_data_bit_offset;

			// Prefetch the next cache line even if we don't have any data left
			// By the time we unpack again, it will have arrived in the CPU cache
			// If our format is full precision, we have at most 4 samples per cache line
			// If our format is drop W, we have at most 5.33 samples per cache line

			// If our pointer was already aligned to a cache line before we unpacked our 4 values,
			// it now points to the first byte of the next cache line. Any offset between 0-63 will fetch it.
			// If our pointer had some offset into a cache line, we might have spanned 2 cache lines.
			// If this happens, we probably already read some data from the next cache line in which
			// case we don't need to prefetch it and we can go to the next one. Any offset after the end
			// of this cache line will fetch it. For safety, we prefetch 63 bytes ahead.
			// Prefetch 4 samples ahead in all levels of the CPU cache
			ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 60);
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);
			ACL_IMPL_ANIMATED_PREFETCH(segment_range_data + 48);
		}
#elif ACL_IMPL_STEP_CURRENT_GROUP_VARIANT == 2 // 2: Branchless raw/variable unpacking
		template<class decompression_settings_adapter_type>
		ACL_IMPL_DEBUG_FORCE_INLINE
		RTM_DISABLE_SECURITY_COOKIE_CHECK
		void unpack_grouped_animated_vector3(
			const persistent_transform_decompression_context_v0& decomp_context,
			rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack,
			const clip_animated_sampling_context_v0& clip_sampling_context,
			segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

			rtm::vector4f* output_scratch_ptr = &output_scratch[0];

			for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
			{
				rtm::vector4f sample;
				if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
				{
					uint32_t num_bits_at_bit_rate = *format_per_track_data;
					format_per_track_data++;

					// We retain an unlikely branch for constant unpacking as it is generally correctly
					// predicted by the processor and this yields a small improvement over a branchless
					// approach (see v1 above).
					// We removed the branch to choose between raw/variable as the instructions to unpack
					// them are quite similar and can be folded together cheaply.

					if (num_bits_at_bit_rate == 0) ACL_BRANCH_UNLIKELY
					{
						sample = unpack_vector3_u48_unsafe(segment_range_data);
					}
					else
					{
						// Remap 31 to 32
						// See write_format_per_track_data(..) for details
						num_bits_at_bit_rate = num_bits_at_bit_rate >= 31 ? 32 : num_bits_at_bit_rate;

						sample = unpack_vector3_mixed_unsafe(num_bits_at_bit_rate, animated_track_data, animated_track_data_bit_offset);
						animated_track_data_bit_offset += num_bits_at_bit_rate * 3;
					}

					// Remap within our ranges
					if (decomp_context.has_segments)
					{
						// If we aren't raw or constant (32 or 0 bits)
						if ((num_bits_at_bit_rate & 0x1F) != 0)
						{
							// Apply segment range remapping
							const uint32_t range_entry_size = 3 * sizeof(uint8_t);
							const uint8_t* segment_range_min_ptr = segment_range_data;
							const uint8_t* segment_range_extent_ptr = segment_range_min_ptr + range_entry_size;

							const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(segment_range_min_ptr);
							const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(segment_range_extent_ptr);

							sample = rtm::vector_mul_add(sample, segment_range_extent, segment_range_min);
						}

						segment_range_data += sizeof(uint16_t) * 3;
					}

					// If we aren't raw (32 bits)
					if ((num_bits_at_bit_rate & 0x20) == 0)
					{
						// Apply clip range remapping
						const uint32_t clip_range_min_size = 3 * sizeof(float);
						const uint8_t* clip_range_min_ptr = clip_range_data;
						const uint8_t* clip_range_extent_ptr = clip_range_data + clip_range_min_size;

						const rtm::vector4f clip_range_min = rtm::vector_load(clip_range_min_ptr);
						const rtm::vector4f clip_range_extent = rtm::vector_load(clip_range_extent_ptr);

						sample = rtm::vector_mul_add(sample, clip_range_extent, clip_range_min);
					}

					const uint32_t clip_range_data_size = 6 * sizeof(float);
					clip_range_data += clip_range_data_size;
				}
				else if (decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full))
				{
					ACL_ASSERT(format == vector_format8::vector3f_full, "Unexpected vector format");
					sample = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
					animated_track_data_bit_offset += 96;
				}
				else
				{
					ACL_ASSERT(false, "Should never be reached");
					sample = rtm::vector_zero();
				}

				ACL_ASSERT(rtm::vector_is_finite3(sample), "Vector3 is not valid!");

				// TODO: Fill in W component with something sensible?

				// Cache
				*output_scratch_ptr++ = sample;
			}

			// Update our pointers
			segment_sampling_context.format_per_track_data = format_per_track_data;
			segment_sampling_context.segment_range_data = segment_range_data;
			segment_sampling_context.animated_track_data_bit_offset = animated_track_data_bit_offset;

			// Prefetch the next cache line even if we don't have any data left
			// By the time we unpack again, it will have arrived in the CPU cache
			// If our format is full precision, we have at most 4 samples per cache line
			// If our format is drop W, we have at most 5.33 samples per cache line

			// If our pointer was already aligned to a cache line before we unpacked our 4 values,
			// it now points to the first byte of the next cache line. Any offset between 0-63 will fetch it.
			// If our pointer had some offset into a cache line, we might have spanned 2 cache lines.
			// If this happens, we probably already read some data from the next cache line in which
			// case we don't need to prefetch it and we can go to the next one. Any offset after the end
			// of this cache line will fetch it. For safety, we prefetch 63 bytes ahead.
			// Prefetch 4 samples ahead in all levels of the CPU cache
			ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 60);
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);
			ACL_IMPL_ANIMATED_PREFETCH(segment_range_data + 48);
		}
#endif

		template<class decompression_settings_adapter_type>
		inline RTM_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL
		unpack_single_animated_vector3(
			const persistent_transform_decompression_context_v0& decomp_context,
			uint32_t unpack_index,
			const clip_animated_sampling_context_v0& clip_sampling_context,
			const segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));
			const compressed_tracks_version16 version = get_version<decompression_settings_adapter_type>(decomp_context.get_version());

			// See write_format_per_track_data(..) for details
			const uint32_t num_raw_bit_rate_bits = version >= compressed_tracks_version16::v02_01_99_1 ? 31U : 32U;

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

			// Range ignore flags are used to skip range normalization at the clip and/or segment levels
			// Each sample has two bits like so:
			//    - 0x01 = ignore segment level
			//    - 0x02 = ignore clip level
			uint32_t range_ignore_flags;

			rtm::vector4f sample;
			if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
			{
				// Fall-through intentional
				uint32_t skip_size = 0;
				switch (unpack_index)
				{
				default:
				case 3:
				{
					// TODO: Can we do an alternate more efficient implementation? We want to increment by one if num bits == 31
					const uint32_t num_bits_at_bit_rate = format_per_track_data[2];
					skip_size += (num_bits_at_bit_rate == num_raw_bit_rate_bits) ? 32 : num_bits_at_bit_rate;
				}
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 2:
				{
					const uint32_t num_bits_at_bit_rate = format_per_track_data[1];
					skip_size += (num_bits_at_bit_rate == num_raw_bit_rate_bits) ? 32 : num_bits_at_bit_rate;
				}
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 1:
				{
					const uint32_t num_bits_at_bit_rate = format_per_track_data[0];
					skip_size += (num_bits_at_bit_rate == num_raw_bit_rate_bits) ? 32 : num_bits_at_bit_rate;
				}
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 0:
					// Nothing to skip
					(void)skip_size;
				}

				// Skip prior samples
				animated_track_data_bit_offset += skip_size * 3;
				segment_range_data += sizeof(uint8_t) * 6 * unpack_index;
				clip_range_data += sizeof(rtm::float3f) * 2 * unpack_index;

				const uint32_t num_bits_at_bit_rate = format_per_track_data[unpack_index];

				if (num_bits_at_bit_rate == 0)	// Constant bit rate
				{
					sample = unpack_vector3_u48_unsafe(segment_range_data);
					range_ignore_flags = 0x01;	// Skip segment only
				}
				else if (num_bits_at_bit_rate == num_raw_bit_rate_bits)	// Raw bit rate
				{
					sample = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
					range_ignore_flags = 0x03;	// Skip clip and segment
				}
				else
				{
					sample = unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, animated_track_data, animated_track_data_bit_offset);
					range_ignore_flags = 0x00;	// Don't skip range reduction
				}
			}
			else // vector_format8::vector3f_full
			{
				animated_track_data_bit_offset += unpack_index * 96;
				sample = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
				range_ignore_flags = 0x03;	// Skip clip and segment
			}

			// Remap within our ranges
			if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
			{
				if (decomp_context.has_segments && (range_ignore_flags & 0x01) == 0)
				{
					// Apply segment range remapping
					const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(segment_range_data);
					const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(segment_range_data + 3 * sizeof(uint8_t));

					sample = rtm::vector_mul_add(sample, segment_range_extent, segment_range_min);
				}

				if ((range_ignore_flags & 0x02) == 0)
				{
					// Apply clip range remapping
					const rtm::vector4f clip_range_min = rtm::vector_load(clip_range_data);
					const rtm::vector4f clip_range_extent = rtm::vector_load(clip_range_data + sizeof(rtm::float3f));

					sample = rtm::vector_mul_add(sample, clip_range_extent, clip_range_min);
				}
			}

			ACL_ASSERT(rtm::vector_is_finite3(sample), "Vector3 is not valid!");
			return sample;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

#undef ACL_IMPL_STEP_CURRENT_GROUP_VARIANT

ACL_IMPL_FILE_PRAGMA_POP
