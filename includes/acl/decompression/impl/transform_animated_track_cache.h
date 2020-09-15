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

#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/impl/track_cache.h"
#include "acl/decompression/impl/transform_decompression_context.h"
#include "acl/math/quatf.h"
#include "acl/math/vector4f.h"

#include <rtm/mask4f.h>
#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/packing/quatf.h>

#include <cstdint>

#define ACL_IMPL_USE_ANIMATED_PREFETCH
//#define ACL_IMPL_USE_AVX_DECOMP

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
#if defined(ACL_IMPL_USE_ANIMATED_PREFETCH)
#define ACL_IMPL_ANIMATED_PREFETCH(ptr) memory_prefetch(ptr)
#else
#define ACL_IMPL_ANIMATED_PREFETCH(ptr) (void)(ptr)
#endif

		struct clip_animated_sampling_context_v0
		{
			// Data is ordered in groups of 4 animated sub-tracks (e.g rot0, rot1, rot2, rot3)
			// Order depends on animated track order. If we have 6 animated rotation tracks before the first animated
			// translation track, we'll have 8 animated rotation sub-tracks followed by 4 animated translation sub-tracks.
			// Once we reach the end, there is no extra padding. The last group might be less than 4 sub-tracks.
			// This is because we always process 4 animated sub-tracks at a time and cache the results.

			const uint8_t* clip_range_data;				// Range information of the current sub-track in the clip
		};

		struct segment_animated_sampling_context_v0
		{
			// Data is ordered in groups of 4 animated sub-tracks (e.g rot0, rot1, rot2, rot3)
			// Order depends on animated track order. If we have 6 animated rotation tracks before the first animated
			// translation track, we'll have 8 animated rotation sub-tracks followed by 4 animated translation sub-tracks.
			// Once we reach the end, there is no extra padding. The last group might be less than 4 sub-tracks.
			// This is because we always process 4 animated sub-tracks at a time and cache the results.

			const uint8_t* format_per_track_data;		// Metadata of the current sub-track
			const uint8_t* segment_range_data;			// Range information (or constant sample if bit rate is 0) of the current sub-track in this segment

			// For the animated samples, constant bit rate sub-tracks (with a bit rate of 0) do not contain samples.
			// As such, their group will not contain 4 sub-tracks.

			const uint8_t* animated_track_data;			// Base of animated sample data, constant and doesn't change after init
			uint32_t animated_track_data_bit_offset;	// Bit offset of the current animated sub-track
		};

		struct animated_group_cursor_v0
		{
			clip_animated_sampling_context_v0 clip_sampling_context;
			segment_animated_sampling_context_v0 segment_sampling_context[2];
			uint32_t group_size;
		};

		template<class decompression_settings_type>
		inline ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::mask4f RTM_SIMD_CALL unpack_animated_quat(const persistent_transform_decompression_context_v0& decomp_context, rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack, segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

			uint32_t segment_range_ignore_mask = 0;
			uint32_t clip_range_ignore_mask = 0;

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			// For SIMD, can we load constant samples and write them to scratch? Afterwards its the same as packed on 16 bits
			// We get 4 short branches (test, cmp, 6x loads, 3x ORs, 3x writes, load immediate) followed by a common code path for all 4 samples

			for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
			{
				// Our decompressed rotation as a vector4
				rtm::vector4f rotation_as_vec;

				if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
				{
					const uint32_t num_bits_at_bit_rate = format_per_track_data[unpack_index];

					uint32_t sample_segment_range_ignore_mask;
					uint32_t sample_clip_range_ignore_mask;

					if (num_bits_at_bit_rate == 0)	// Constant bit rate
					{
						// Segment range is packed: min.xxxx, min.yyyy, min.zzzz, extent.xxxx, extent.yyyy, extent.zzzz
						// Our constant sample value is packed 8 bits in each group in the sample's lane
						// To load our sample, we need to load: (min.x[unpack_index] << 8) | min.y[unpack_index], (min.z[unpack_index] << 8) | extent.x[unpack_index], (extent.y[unpack_index] << 8) | extent.z[unpack_index]
						// This is more complicated than if we were in AOS form but constant bit rates are somewhat rare while nearly every sample
						// has segment range information which is a lot simpler to load in SOA form
						const uint8_t* shifted_segment_range_data = segment_range_data + unpack_index;
						const uint32_t x = (uint32_t(shifted_segment_range_data[0]) << 8) | shifted_segment_range_data[4];
						const uint32_t y = (uint32_t(shifted_segment_range_data[8]) << 8) | shifted_segment_range_data[12];
						const uint32_t z = (uint32_t(shifted_segment_range_data[16]) << 8) | shifted_segment_range_data[20];

#if defined(RTM_SSE2_INTRINSICS)
						// TODO: Use SIMD for this

						// Load min.xxxx, min.yyyy, 8 bytes, offset by our sample index such that the first byte is our sample
						// Unpack low and interleave xxxx, yyyy, we end up with sample.x in our first lane as uint16_t
						// Unpack low to convert to uint32_t, sample.x lives in lane 0, repeat for sample.yz
						// Total of 2x loads (re-use first load and interleave high for sample.y), 5x unpack
						// Merge sample.xy together (1x shuffle)
						// Merge sample.xyz together (1x shuffle)
						// Convert to floats and normalize
						__m128i xyz = _mm_setr_epi32(x, y, z, 0);
						__m128 xyzf = _mm_cvtepi32_ps(xyz);
						rotation_as_vec = _mm_mul_ps(xyzf, _mm_set_ps1(1.0F / 65535.0F));
#elif defined(RTM_NEON_INTRINSICS)
						uint32x4_t xyz = vcombine_u32(vcreate_u32((uint64_t(y) << 32) | x), vcreate_u32(z));
						float32x4_t xyzf = vcvtq_f32_u32(xyz);
						rotation_as_vec = vmulq_n_f32(xyzf, 1.0F / 65535.0F);
#else
						const rtm::vector4f xyz = rtm::vector_set(float(x), float(y), float(z), 0.0F);
						rotation_as_vec = rtm::vector_mul(xyz, 1.0F / 65535.0F);
#endif

						sample_segment_range_ignore_mask = 0xFF;	// Ignore segment range
						sample_clip_range_ignore_mask = 0x00;
					}
					else if (num_bits_at_bit_rate == 32)			// Raw bit rate
					{
						rotation_as_vec = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
						animated_track_data_bit_offset += 96;
						sample_segment_range_ignore_mask = 0xFF;	// Ignore segment range
						sample_clip_range_ignore_mask = 0xFF;		// Ignore clip range
					}
					else
					{
						rotation_as_vec = unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, animated_track_data, animated_track_data_bit_offset);
						animated_track_data_bit_offset += num_bits_at_bit_rate * 3;
						sample_segment_range_ignore_mask = 0x00;
						sample_clip_range_ignore_mask = 0x00;
					}

					// Masks are used in little endian format so the first sample is in the LSB end
					segment_range_ignore_mask |= sample_segment_range_ignore_mask << (unpack_index * 8);
					clip_range_ignore_mask |= sample_clip_range_ignore_mask << (unpack_index * 8);
				}
				else
				{
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
					{
						rotation_as_vec = unpack_vector4_128_unsafe(animated_track_data, animated_track_data_bit_offset);
						animated_track_data_bit_offset += 128;
					}
					else // rotation_format8::quatf_drop_w_full
					{
						rotation_as_vec = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
						animated_track_data_bit_offset += 96;
					}
				}

				output_scratch[unpack_index] = rotation_as_vec;
			}

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
			ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 63);
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);

			// Update our pointers
			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
				// Skip our used metadata data, all groups are padded to 4 elements
				segment_sampling_context.format_per_track_data = format_per_track_data + 4;
			}

			segment_sampling_context.animated_track_data_bit_offset = animated_track_data_bit_offset;

			// Swizzle our samples into SOA form
			// TODO: Optimize for NEON
			rtm::vector4f tmp0 = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::a, rtm::mix4::b>(output_scratch[0], output_scratch[1]);
			rtm::vector4f tmp1 = rtm::vector_mix<rtm::mix4::z, rtm::mix4::w, rtm::mix4::c, rtm::mix4::d>(output_scratch[0], output_scratch[1]);
			rtm::vector4f tmp2 = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::a, rtm::mix4::b>(output_scratch[2], output_scratch[3]);
			rtm::vector4f tmp3 = rtm::vector_mix<rtm::mix4::z, rtm::mix4::w, rtm::mix4::c, rtm::mix4::d>(output_scratch[2], output_scratch[3]);

			rtm::vector4f sample_xxxx = rtm::vector_mix<rtm::mix4::x, rtm::mix4::z, rtm::mix4::a, rtm::mix4::c>(tmp0, tmp2);
			rtm::vector4f sample_yyyy = rtm::vector_mix<rtm::mix4::y, rtm::mix4::w, rtm::mix4::b, rtm::mix4::d>(tmp0, tmp2);
			rtm::vector4f sample_zzzz = rtm::vector_mix<rtm::mix4::x, rtm::mix4::z, rtm::mix4::a, rtm::mix4::c>(tmp1, tmp3);

			rtm::mask4f clip_range_ignore_mask_v32f;	// function's return value

			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
				// TODO: Move range remapping out of here and do it with AVX together with quat W reconstruction

				const rtm::vector4f one_v = rtm::vector_set(1.0F);

#if defined(RTM_SSE2_INTRINSICS)
				const __m128i ignore_masks_v8 = _mm_set_epi32(0, 0, clip_range_ignore_mask, segment_range_ignore_mask);
				const __m128i ignore_masks_v16 = _mm_unpacklo_epi8(ignore_masks_v8, ignore_masks_v8);
#elif defined(RTM_NEON_INTRINSICS)
				const int8x8_t ignore_masks_v8 = vcreate_s8((uint64_t(clip_range_ignore_mask) << 32) | segment_range_ignore_mask);
				const int16x8_t ignore_masks_v16 = vmovl_s8(ignore_masks_v8);
#endif

				if (decomp_context.has_segments)
				{
					// TODO: prefetch segment data earlier, as soon as we are done loading

					// Segment range is packed: min.xxxx, min.yyyy, min.zzzz, extent.xxxx, extent.yyyy, extent.zzzz

#if defined(RTM_SSE2_INTRINSICS)
					const __m128i zero = _mm_setzero_si128();

					const __m128i segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8 = _mm_loadu_si128((const __m128i*)segment_range_data);
					const __m128i segment_range_extent_yyyy_zzzz_u8 = _mm_loadu_si128((const __m128i*)(segment_range_data + 16));

					// Convert from u8 to u32
					const __m128i segment_range_min_xxxx_yyyy_u16 = _mm_unpacklo_epi8(segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8, zero);
					const __m128i segment_range_min_zzzz_extent_xxxx_u16 = _mm_unpackhi_epi8(segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8, zero);
					const __m128i segment_range_extent_yyyy_zzzz_u16 = _mm_unpacklo_epi8(segment_range_extent_yyyy_zzzz_u8, zero);

					__m128i segment_range_min_xxxx_u32 = _mm_unpacklo_epi16(segment_range_min_xxxx_yyyy_u16, zero);
					__m128i segment_range_min_yyyy_u32 = _mm_unpackhi_epi16(segment_range_min_xxxx_yyyy_u16, zero);
					__m128i segment_range_min_zzzz_u32 = _mm_unpacklo_epi16(segment_range_min_zzzz_extent_xxxx_u16, zero);

					const __m128i segment_range_extent_xxxx_u32 = _mm_unpackhi_epi16(segment_range_min_zzzz_extent_xxxx_u16, zero);
					const __m128i segment_range_extent_yyyy_u32 = _mm_unpacklo_epi16(segment_range_extent_yyyy_zzzz_u16, zero);
					const __m128i segment_range_extent_zzzz_u32 = _mm_unpackhi_epi16(segment_range_extent_yyyy_zzzz_u16, zero);

					// Mask out the segment min we ignore
					const __m128i segment_range_ignore_mask_u32 = _mm_unpacklo_epi16(ignore_masks_v16, ignore_masks_v16);

					segment_range_min_xxxx_u32 = _mm_andnot_si128(segment_range_ignore_mask_u32, segment_range_min_xxxx_u32);
					segment_range_min_yyyy_u32 = _mm_andnot_si128(segment_range_ignore_mask_u32, segment_range_min_yyyy_u32);
					segment_range_min_zzzz_u32 = _mm_andnot_si128(segment_range_ignore_mask_u32, segment_range_min_zzzz_u32);

					__m128 segment_range_min_xxxx = _mm_cvtepi32_ps(segment_range_min_xxxx_u32);
					__m128 segment_range_min_yyyy = _mm_cvtepi32_ps(segment_range_min_yyyy_u32);
					__m128 segment_range_min_zzzz = _mm_cvtepi32_ps(segment_range_min_zzzz_u32);

					__m128 segment_range_extent_xxxx = _mm_cvtepi32_ps(segment_range_extent_xxxx_u32);
					__m128 segment_range_extent_yyyy = _mm_cvtepi32_ps(segment_range_extent_yyyy_u32);
					__m128 segment_range_extent_zzzz = _mm_cvtepi32_ps(segment_range_extent_zzzz_u32);

					const __m128 normalization_value = _mm_set_ps1(1.0F / 255.0F);

					segment_range_min_xxxx = _mm_mul_ps(segment_range_min_xxxx, normalization_value);
					segment_range_min_yyyy = _mm_mul_ps(segment_range_min_yyyy, normalization_value);
					segment_range_min_zzzz = _mm_mul_ps(segment_range_min_zzzz, normalization_value);

					segment_range_extent_xxxx = _mm_mul_ps(segment_range_extent_xxxx, normalization_value);
					segment_range_extent_yyyy = _mm_mul_ps(segment_range_extent_yyyy, normalization_value);
					segment_range_extent_zzzz = _mm_mul_ps(segment_range_extent_zzzz, normalization_value);

					const rtm::mask4f segment_range_ignore_mask_v = _mm_castsi128_ps(segment_range_ignore_mask_u32);
#elif defined(RTM_NEON_INTRINSICS)
					const uint8x16_t segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8 = vld1q_u8(segment_range_data);
					const uint8x8_t segment_range_extent_yyyy_zzzz_u8 = vld1_u8(segment_range_data + 16);

					// Convert from u8 to u32
					const uint16x8_t segment_range_min_xxxx_yyyy_u16 = vmovl_u8(vget_low_u8(segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8));
					const uint16x8_t segment_range_min_zzzz_extent_xxxx_u16 = vmovl_u8(vget_high_u8(segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8));
					const uint16x8_t segment_range_extent_yyyy_zzzz_u16 = vmovl_u8(segment_range_extent_yyyy_zzzz_u8);

					uint32x4_t segment_range_min_xxxx_u32 = vmovl_u16(vget_low_u16(segment_range_min_xxxx_yyyy_u16));
					uint32x4_t segment_range_min_yyyy_u32 = vmovl_u16(vget_high_u16(segment_range_min_xxxx_yyyy_u16));
					uint32x4_t segment_range_min_zzzz_u32 = vmovl_u16(vget_low_u16(segment_range_min_zzzz_extent_xxxx_u16));

					const uint32x4_t segment_range_extent_xxxx_u32 = vmovl_u16(vget_high_u16(segment_range_min_zzzz_extent_xxxx_u16));
					const uint32x4_t segment_range_extent_yyyy_u32 = vmovl_u16(vget_low_u16(segment_range_extent_yyyy_zzzz_u16));
					const uint32x4_t segment_range_extent_zzzz_u32 = vmovl_u16(vget_high_u16(segment_range_extent_yyyy_zzzz_u16));

					// Mask out the segment min we ignore
					const uint32x4_t segment_range_ignore_mask_u32 = vreinterpretq_u32_s32(vmovl_s16(vget_low_s16(ignore_masks_v16)));

					segment_range_min_xxxx_u32 = vbicq_u32(segment_range_min_xxxx_u32, segment_range_ignore_mask_u32);
					segment_range_min_yyyy_u32 = vbicq_u32(segment_range_min_yyyy_u32, segment_range_ignore_mask_u32);
					segment_range_min_zzzz_u32 = vbicq_u32(segment_range_min_zzzz_u32, segment_range_ignore_mask_u32);

					float32x4_t segment_range_min_xxxx = vcvtq_f32_u32(segment_range_min_xxxx_u32);
					float32x4_t segment_range_min_yyyy = vcvtq_f32_u32(segment_range_min_yyyy_u32);
					float32x4_t segment_range_min_zzzz = vcvtq_f32_u32(segment_range_min_zzzz_u32);

					float32x4_t segment_range_extent_xxxx = vcvtq_f32_u32(segment_range_extent_xxxx_u32);
					float32x4_t segment_range_extent_yyyy = vcvtq_f32_u32(segment_range_extent_yyyy_u32);
					float32x4_t segment_range_extent_zzzz = vcvtq_f32_u32(segment_range_extent_zzzz_u32);

					const float normalization_value = 1.0F / 255.0F;

					segment_range_min_xxxx = vmulq_n_f32(segment_range_min_xxxx, normalization_value);
					segment_range_min_yyyy = vmulq_n_f32(segment_range_min_yyyy, normalization_value);
					segment_range_min_zzzz = vmulq_n_f32(segment_range_min_zzzz, normalization_value);

					segment_range_extent_xxxx = vmulq_n_f32(segment_range_extent_xxxx, normalization_value);
					segment_range_extent_yyyy = vmulq_n_f32(segment_range_extent_yyyy, normalization_value);
					segment_range_extent_zzzz = vmulq_n_f32(segment_range_extent_zzzz, normalization_value);

					const rtm::mask4f segment_range_ignore_mask_v = vreinterpretq_f32_u32(segment_range_ignore_mask_u32);
#else
					rtm::vector4f segment_range_min_xxxx = rtm::vector_set(float(segment_range_data[0]), float(segment_range_data[1]), float(segment_range_data[2]), float(segment_range_data[3]));
					rtm::vector4f segment_range_min_yyyy = rtm::vector_set(float(segment_range_data[4]), float(segment_range_data[5]), float(segment_range_data[6]), float(segment_range_data[7]));
					rtm::vector4f segment_range_min_zzzz = rtm::vector_set(float(segment_range_data[8]), float(segment_range_data[9]), float(segment_range_data[10]), float(segment_range_data[11]));

					rtm::vector4f segment_range_extent_xxxx = rtm::vector_set(float(segment_range_data[12]), float(segment_range_data[13]), float(segment_range_data[14]), float(segment_range_data[15]));
					rtm::vector4f segment_range_extent_yyyy = rtm::vector_set(float(segment_range_data[16]), float(segment_range_data[17]), float(segment_range_data[18]), float(segment_range_data[19]));
					rtm::vector4f segment_range_extent_zzzz = rtm::vector_set(float(segment_range_data[20]), float(segment_range_data[21]), float(segment_range_data[22]), float(segment_range_data[23]));

					const float normalization_value = 1.0F / 255.0F;

					segment_range_min_xxxx = rtm::vector_mul(segment_range_min_xxxx, normalization_value);
					segment_range_min_yyyy = rtm::vector_mul(segment_range_min_yyyy, normalization_value);
					segment_range_min_zzzz = rtm::vector_mul(segment_range_min_zzzz, normalization_value);

					segment_range_extent_xxxx = rtm::vector_mul(segment_range_extent_xxxx, normalization_value);
					segment_range_extent_yyyy = rtm::vector_mul(segment_range_extent_yyyy, normalization_value);
					segment_range_extent_zzzz = rtm::vector_mul(segment_range_extent_zzzz, normalization_value);

					// Mask out the segment min we ignore
					if (segment_range_ignore_mask & 0x000000FF)
					{
						segment_range_min_xxxx = rtm::vector_set_x(segment_range_min_xxxx, 0.0F);
						segment_range_min_yyyy = rtm::vector_set_x(segment_range_min_yyyy, 0.0F);
						segment_range_min_zzzz = rtm::vector_set_x(segment_range_min_zzzz, 0.0F);
					}

					if (segment_range_ignore_mask & 0x0000FF00)
					{
						segment_range_min_xxxx = rtm::vector_set_y(segment_range_min_xxxx, 0.0F);
						segment_range_min_yyyy = rtm::vector_set_y(segment_range_min_yyyy, 0.0F);
						segment_range_min_zzzz = rtm::vector_set_y(segment_range_min_zzzz, 0.0F);
					}

					if (segment_range_ignore_mask & 0x00FF0000)
					{
						segment_range_min_xxxx = rtm::vector_set_z(segment_range_min_xxxx, 0.0F);
						segment_range_min_yyyy = rtm::vector_set_z(segment_range_min_yyyy, 0.0F);
						segment_range_min_zzzz = rtm::vector_set_z(segment_range_min_zzzz, 0.0F);
					}

					if (segment_range_ignore_mask & 0xFF000000)
					{
						segment_range_min_xxxx = rtm::vector_set_w(segment_range_min_xxxx, 0.0F);
						segment_range_min_yyyy = rtm::vector_set_w(segment_range_min_yyyy, 0.0F);
						segment_range_min_zzzz = rtm::vector_set_w(segment_range_min_zzzz, 0.0F);
					}

					const rtm::mask4f segment_range_ignore_mask_v = rtm::mask_set((segment_range_ignore_mask & 0x000000FF) != 0, (segment_range_ignore_mask & 0x0000FF00) != 0, (segment_range_ignore_mask & 0x00FF0000) != 0, (segment_range_ignore_mask & 0xFF000000) != 0);
#endif

					// Mask out the segment extent we ignore
					segment_range_extent_xxxx = rtm::vector_select(segment_range_ignore_mask_v, one_v, segment_range_extent_xxxx);
					segment_range_extent_yyyy = rtm::vector_select(segment_range_ignore_mask_v, one_v, segment_range_extent_yyyy);
					segment_range_extent_zzzz = rtm::vector_select(segment_range_ignore_mask_v, one_v, segment_range_extent_zzzz);

					sample_xxxx = rtm::vector_mul_add(sample_xxxx, segment_range_extent_xxxx, segment_range_min_xxxx);
					sample_yyyy = rtm::vector_mul_add(sample_yyyy, segment_range_extent_yyyy, segment_range_min_yyyy);
					sample_zzzz = rtm::vector_mul_add(sample_zzzz, segment_range_extent_zzzz, segment_range_min_zzzz);
				}

#if defined(RTM_SSE2_INTRINSICS)
				clip_range_ignore_mask_v32f = _mm_castsi128_ps(_mm_unpackhi_epi16(ignore_masks_v16, ignore_masks_v16));
#elif defined(RTM_NEON_INTRINSICS)
				clip_range_ignore_mask_v32f = vreinterpretq_f32_s32(vmovl_s16(vget_high_s16(ignore_masks_v16)));
#else
				clip_range_ignore_mask_v32f = rtm::mask_set((clip_range_ignore_mask & 0x000000FF) != 0, (clip_range_ignore_mask & 0x0000FF00) != 0, (clip_range_ignore_mask & 0x00FF0000) != 0, (clip_range_ignore_mask & 0xFF000000) != 0);
#endif

				// Skip our used segment range data, all groups are padded to 4 elements
				segment_range_data += 6 * 4;

				// Update our ptr
				segment_sampling_context.segment_range_data = segment_range_data;

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
				ACL_IMPL_ANIMATED_PREFETCH(segment_range_data + 63);
			}
			else
			{
				const rtm::vector4f sample_wwww = rtm::vector_mix<rtm::mix4::y, rtm::mix4::w, rtm::mix4::b, rtm::mix4::d>(tmp1, tmp3);
				output_scratch[3] = sample_wwww;

				// TODO: Optimize for SSE/NEON, codegen for this might not be optimal
				clip_range_ignore_mask_v32f = rtm::mask_set(0U, 0U, 0U, 0U);	// Won't be used, just initialize it to something
			}

			output_scratch[0] = sample_xxxx;
			output_scratch[1] = sample_yyyy;
			output_scratch[2] = sample_zzzz;

			return clip_range_ignore_mask_v32f;
		}

		template<class decompression_settings_type>
		inline ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_single_animated_quat(const persistent_transform_decompression_context_v0& decomp_context,
			uint32_t unpack_index, uint32_t group_size,
			const clip_animated_sampling_context_v0& clip_sampling_context, const segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

			uint32_t segment_range_ignore_mask = 0;
			uint32_t clip_range_ignore_mask = 0;

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			// Unpack sample
			rtm::vector4f rotation_as_vec;
			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
				// Fall-through intentional
				uint32_t skip_size = 0;
				switch (unpack_index)
				{
				default:
				case 3:
					skip_size += format_per_track_data[2];
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 2:
					skip_size += format_per_track_data[1];
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 1:
					skip_size += format_per_track_data[0];
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 0:
					// Nothing to skip
					(void)skip_size;
				}

				// Skip prior samples
				animated_track_data_bit_offset += skip_size * 3;

				const uint32_t num_bits_at_bit_rate = format_per_track_data[unpack_index];

				if (num_bits_at_bit_rate == 0)	// Constant bit rate
				{
					// Segment range is packed: min.xxxx, min.yyyy, min.zzzz, extent.xxxx, extent.yyyy, extent.zzzz
					// Our constant sample value is packed 8 bits in each group in the sample's lane
					// To load our sample, we need to load: (min.x[unpack_index] << 8) | min.y[unpack_index], (min.z[unpack_index] << 8) | extent.x[unpack_index], (extent.y[unpack_index] << 8) | extent.z[unpack_index]
					// This is more complicated than if we were in AOS form but constant bit rates are somewhat rare while nearly every sample
					// has segment range information which is a lot simpler to load in SOA form
					const uint8_t* shifted_segment_range_data = segment_range_data + unpack_index;
					const uint32_t x = (uint32_t(shifted_segment_range_data[0]) << 8) | shifted_segment_range_data[4];
					const uint32_t y = (uint32_t(shifted_segment_range_data[8]) << 8) | shifted_segment_range_data[12];
					const uint32_t z = (uint32_t(shifted_segment_range_data[16]) << 8) | shifted_segment_range_data[20];

#if defined(RTM_SSE2_INTRINSICS)
					// TODO: Use SIMD for this

					// Load min.xxxx, min.yyyy, 8 bytes, offset by our sample index such that the first byte is our sample
					// Unpack low and interleave xxxx, yyyy, we end up with sample.x in our first lane as uint16_t
					// Unpack low to convert to uint32_t, sample.x lives in lane 0, repeat for sample.yz
					// Total of 2x loads (re-use first load and interleave high for sample.y), 5x unpack
					// Merge sample.xy together (1x shuffle)
					// Merge sample.xyz together (1x shuffle)
					// Convert to floats and normalize
					__m128i xyz = _mm_setr_epi32(x, y, z, 0);
					__m128 xyzf = _mm_cvtepi32_ps(xyz);
					rotation_as_vec = _mm_mul_ps(xyzf, _mm_set_ps1(1.0F / 65535.0F));
#elif defined(RTM_NEON_INTRINSICS)
					uint32x4_t xyz = vcombine_u32(vcreate_u32((uint64_t(y) << 32) | x), vcreate_u32(z));
					float32x4_t xyzf = vcvtq_f32_u32(xyz);
					rotation_as_vec = vmulq_n_f32(xyzf, 1.0F / 65535.0F);
#else
					const rtm::vector4f xyz = rtm::vector_set(float(x), float(y), float(z), 0.0F);
					rotation_as_vec = rtm::vector_mul(xyz, 1.0F / 65535.0F);
#endif

					segment_range_ignore_mask = 0xFF;	// Ignore segment range
					clip_range_ignore_mask = 0x00;
				}
				else if (num_bits_at_bit_rate == 32)	// Raw bit rate
				{
					rotation_as_vec = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
					segment_range_ignore_mask = 0xFF;	// Ignore segment range
					clip_range_ignore_mask = 0xFF;		// Ignore clip range
				}
				else
				{
					rotation_as_vec = unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, animated_track_data, animated_track_data_bit_offset);
					segment_range_ignore_mask = 0x00;
					clip_range_ignore_mask = 0x00;
				}
			}
			else
			{
				if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					animated_track_data_bit_offset += unpack_index * 128;
					rotation_as_vec = unpack_vector4_128_unsafe(animated_track_data, animated_track_data_bit_offset);
				}
				else // rotation_format8::quatf_drop_w_full
				{
					animated_track_data_bit_offset += unpack_index * 96;
					rotation_as_vec = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
				}
			}

			// Remap within our ranges
			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
				if (decomp_context.has_segments && segment_range_ignore_mask == 0)
				{
					// Segment range is packed: min.xxxx, min.yyyy, min.zzzz, extent.xxxx, extent.yyyy, extent.zzzz
					segment_range_data += unpack_index;	// Offset to our sample

					const uint32_t min_x = segment_range_data[0];
					const uint32_t min_y = segment_range_data[4];
					const uint32_t min_z = segment_range_data[8];

					const uint32_t extent_x = segment_range_data[12];
					const uint32_t extent_y = segment_range_data[16];
					const uint32_t extent_z = segment_range_data[20];

#if defined(RTM_SSE2_INTRINSICS)
					__m128i min_u32 = _mm_setr_epi32(min_x, min_y, min_z, 0);
					__m128i extent_u32 = _mm_setr_epi32(extent_x, extent_y, extent_z, 0);

					rtm::vector4f segment_range_min = _mm_cvtepi32_ps(min_u32);
					rtm::vector4f segment_range_extent = _mm_cvtepi32_ps(extent_u32);
#elif defined(RTM_NEON_INTRINSICS)
					uint32x4_t min_u32 = vcombine_u32(vcreate_u32((uint64_t(min_y) << 32) | min_x), vcreate_u32(min_z));
					uint32x4_t extent_u32 = vcombine_u32(vcreate_u32((uint64_t(extent_y) << 32) | extent_x), vcreate_u32(extent_z));

					rtm::vector4f segment_range_min = vcvtq_f32_u32(min_u32);
					rtm::vector4f segment_range_extent = vcvtq_f32_u32(extent_u32);
#else
					rtm::vector4f segment_range_min = rtm::vector_set(float(min_x), float(min_y), float(min_z), 0.0F);
					rtm::vector4f segment_range_extent = rtm::vector_set(float(extent_x), float(extent_y), float(extent_z), 0.0F);
#endif

					const float normalization_scale = 1.0F / 255.0F;
					segment_range_min = rtm::vector_mul(segment_range_min, normalization_scale);
					segment_range_extent = rtm::vector_mul(segment_range_extent, normalization_scale);

					rotation_as_vec = rtm::vector_mul_add(rotation_as_vec, segment_range_extent, segment_range_min);
				}

				if (clip_range_ignore_mask == 0)
				{
					const float* clip_range_data = reinterpret_cast<const float*>(clip_sampling_context.clip_range_data) + unpack_index;	// Offset to our sample

					const float min_x = clip_range_data[group_size * 0];
					const float min_y = clip_range_data[group_size * 1];
					const float min_z = clip_range_data[group_size * 2];
					const rtm::vector4f clip_range_min = rtm::vector_set(min_x, min_y, min_z, 0.0F);

					const float extent_x = clip_range_data[group_size * 3];
					const float extent_y = clip_range_data[group_size * 4];
					const float extent_z = clip_range_data[group_size * 5];
					const rtm::vector4f clip_range_extent = rtm::vector_set(extent_x, extent_y, extent_z, 0.0F);

					rotation_as_vec = rtm::vector_mul_add(rotation_as_vec, clip_range_extent, clip_range_min);
				}
			}

			return rotation_as_vec;
		}

		template<class decompression_settings_adapter_type>
		inline ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_animated_vector3(const persistent_transform_decompression_context_v0& decomp_context, rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack,
			const clip_animated_sampling_context_v0& clip_sampling_context, segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

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
					else if (num_bits_at_bit_rate == 32)	// Raw bit rate
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
				else // vector_format8::vector3f_full
				{
					sample = unpack_vector3_96_unsafe(animated_track_data, animated_track_data_bit_offset);
					animated_track_data_bit_offset += 96;
					range_ignore_flags = 0x03;	// Skip clip and segment
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
						const uint32_t range_entry_size = 3 * sizeof(float);
						const uint32_t sub_track_offset = range_entry_size * 2 * unpack_index;
						const uint8_t* clip_range_min_ptr = clip_range_data + sub_track_offset;
						const uint8_t* clip_range_extent_ptr = clip_range_min_ptr + range_entry_size;

						const rtm::vector4f clip_range_min = rtm::vector_load(clip_range_min_ptr);
						const rtm::vector4f clip_range_extent = rtm::vector_load(clip_range_extent_ptr);

						sample = rtm::vector_mul_add(sample, clip_range_extent, clip_range_min);
					}
				}

				ACL_ASSERT(rtm::vector_is_finite3(sample), "Vector3 is not valid!");

				// TODO: Fill in W component with something sensible?

				// Cache
				output_scratch[unpack_index] = sample;
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
			ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 63);
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);
			ACL_IMPL_ANIMATED_PREFETCH(segment_range_data + 63);
		}

		template<class decompression_settings_adapter_type>
		inline ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_single_animated_vector3(const persistent_transform_decompression_context_v0& decomp_context,
			uint32_t unpack_index,
			const clip_animated_sampling_context_v0& clip_sampling_context, const segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));

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
					skip_size += format_per_track_data[2];
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 2:
					skip_size += format_per_track_data[1];
					ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
				case 1:
					skip_size += format_per_track_data[0];
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
				else if (num_bits_at_bit_rate == 32)	// Raw bit rate
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

		struct animated_track_cache_v0
		{
			track_cache_quatf_v0 rotations;
			track_cache_vector4f_v0 translations;
			track_cache_vector4f_v0 scales;

			// Scratch space when we decompress our samples before we interpolate
			rtm::vector4f scratch0[4];
			rtm::vector4f scratch1[4];

			clip_animated_sampling_context_v0 clip_sampling_context;

			segment_animated_sampling_context_v0 segment_sampling_context[2];

			ACL_DISABLE_SECURITY_COOKIE_CHECK void get_rotation_cursor(animated_group_cursor_v0& cursor) const
			{
				cursor.clip_sampling_context = clip_sampling_context;
				cursor.segment_sampling_context[0] = segment_sampling_context[0];
				cursor.segment_sampling_context[1] = segment_sampling_context[1];
				cursor.group_size = std::min<uint32_t>(rotations.num_left_to_unpack, 4);
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK void get_translation_cursor(animated_group_cursor_v0& cursor) const
			{
				cursor.clip_sampling_context = clip_sampling_context;
				cursor.segment_sampling_context[0] = segment_sampling_context[0];
				cursor.segment_sampling_context[1] = segment_sampling_context[1];
				cursor.group_size = std::min<uint32_t>(translations.num_left_to_unpack, 4);
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK void get_scale_cursor(animated_group_cursor_v0& cursor) const
			{
				cursor.clip_sampling_context = clip_sampling_context;
				cursor.segment_sampling_context[0] = segment_sampling_context[0];
				cursor.segment_sampling_context[1] = segment_sampling_context[1];
				cursor.group_size = std::min<uint32_t>(scales.num_left_to_unpack, 4);
			}

			void ACL_DISABLE_SECURITY_COOKIE_CHECK initialize(const persistent_transform_decompression_context_v0& decomp_context)
			{
				clip_sampling_context.clip_range_data = decomp_context.clip_range_data;

				segment_sampling_context[0].format_per_track_data = decomp_context.format_per_track_data[0];
				segment_sampling_context[0].segment_range_data = decomp_context.segment_range_data[0];
				segment_sampling_context[0].animated_track_data = decomp_context.animated_track_data[0];
				segment_sampling_context[0].animated_track_data_bit_offset = decomp_context.key_frame_bit_offsets[0];

				segment_sampling_context[1].format_per_track_data = decomp_context.format_per_track_data[1];
				segment_sampling_context[1].segment_range_data = decomp_context.segment_range_data[1];
				segment_sampling_context[1].animated_track_data = decomp_context.animated_track_data[1];
				segment_sampling_context[1].animated_track_data_bit_offset = decomp_context.key_frame_bit_offsets[1];

				const transform_tracks_header& transform_header = get_transform_tracks_header(*decomp_context.tracks);

				rotations.num_left_to_unpack = transform_header.num_animated_rotation_sub_tracks;
				translations.num_left_to_unpack = transform_header.num_animated_translation_sub_tracks;
				scales.num_left_to_unpack = transform_header.num_animated_scale_sub_tracks;
			}

			template<class decompression_settings_type>
			void ACL_DISABLE_SECURITY_COOKIE_CHECK unpack_rotation_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
				uint32_t num_left_to_unpack = rotations.num_left_to_unpack;
				if (num_left_to_unpack == 0)
					return;	// Nothing left to do, we are done

				// If we have less than 4 cached samples, unpack 4 more and prefetch the next cache line
				const uint32_t num_cached = rotations.get_num_cached();
				if (num_cached >= 4)
					return;	// Enough cached, nothing to do

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				num_left_to_unpack -= num_to_unpack;
				rotations.num_left_to_unpack = num_left_to_unpack;

				// Write index will be either 0 or 4 here since we always unpack 4 at a time
				uint32_t cache_write_index = rotations.cache_write_index % 8;
				rotations.cache_write_index += num_to_unpack;

				const rtm::mask4f clip_range_mask0 = unpack_animated_quat<decompression_settings_type>(decomp_context, scratch0, num_to_unpack, segment_sampling_context[0]);
				const rtm::mask4f clip_range_mask1 = unpack_animated_quat<decompression_settings_type>(decomp_context, scratch1, num_to_unpack, segment_sampling_context[1]);

				rtm::vector4f scratch0_xxxx = scratch0[0];
				rtm::vector4f scratch0_yyyy = scratch0[1];
				rtm::vector4f scratch0_zzzz = scratch0[2];
				rtm::vector4f scratch0_wwww;

				rtm::vector4f scratch1_xxxx = scratch1[0];
				rtm::vector4f scratch1_yyyy = scratch1[1];
				rtm::vector4f scratch1_zzzz = scratch1[2];
				rtm::vector4f scratch1_wwww;

#if defined(RTM_AVX_INTRINSICS) && defined(ACL_IMPL_USE_AVX_DECOMP)
				__m256 scratch_xxxx0_xxxx1 = _mm256_set_m128(scratch1_xxxx, scratch0_xxxx);
				__m256 scratch_yyyy0_yyyy1 = _mm256_set_m128(scratch1_yyyy, scratch0_yyyy);
				__m256 scratch_zzzz0_zzzz1 = _mm256_set_m128(scratch1_zzzz, scratch0_zzzz);

				const __m256 one_v = _mm256_set1_ps(1.0F);
#endif

				// If we have a variable bit rate, we perform range reduction, skip the data we used
				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
				{
					const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

					// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
					const uint32_t load_size = num_to_unpack * sizeof(float);

#if defined(RTM_AVX_INTRINSICS) && defined(ACL_IMPL_USE_AVX_DECOMP)
					__m256 clip_range_mask0_mask1 = _mm256_set_m128(clip_range_mask1, clip_range_mask0);
#endif

					const rtm::vector4f clip_range_min_xxxx = rtm::vector_load(clip_range_data + load_size * 0);
					const rtm::vector4f clip_range_min_yyyy = rtm::vector_load(clip_range_data + load_size * 1);
					const rtm::vector4f clip_range_min_zzzz = rtm::vector_load(clip_range_data + load_size * 2);

					const rtm::vector4f clip_range_extent_xxxx = rtm::vector_load(clip_range_data + load_size * 3);
					const rtm::vector4f clip_range_extent_yyyy = rtm::vector_load(clip_range_data + load_size * 4);
					const rtm::vector4f clip_range_extent_zzzz = rtm::vector_load(clip_range_data + load_size * 5);

#if defined(RTM_AVX_INTRINSICS) && defined(ACL_IMPL_USE_AVX_DECOMP)
					__m256 clip_range_min_xxxx_xxxx = _mm256_set_m128(clip_range_min_xxxx, clip_range_min_xxxx);
					__m256 clip_range_min_yyyy_yyyy = _mm256_set_m128(clip_range_min_yyyy, clip_range_min_yyyy);
					__m256 clip_range_min_zzzz_zzzz = _mm256_set_m128(clip_range_min_zzzz, clip_range_min_zzzz);

					__m256 clip_range_extent_xxxx_xxxx = _mm256_set_m128(clip_range_extent_xxxx, clip_range_extent_xxxx);
					__m256 clip_range_extent_yyyy_yyyy = _mm256_set_m128(clip_range_extent_yyyy, clip_range_extent_yyyy);
					__m256 clip_range_extent_zzzz_zzzz = _mm256_set_m128(clip_range_extent_zzzz, clip_range_extent_zzzz);
#endif

					// Mask out the clip ranges we ignore
#if defined(RTM_AVX_INTRINSICS) && defined(ACL_IMPL_USE_AVX_DECOMP)
					clip_range_min_xxxx_xxxx = _mm256_andnot_ps(clip_range_mask0_mask1, clip_range_min_xxxx_xxxx);
					clip_range_min_yyyy_yyyy = _mm256_andnot_ps(clip_range_mask0_mask1, clip_range_min_yyyy_yyyy);
					clip_range_min_zzzz_zzzz = _mm256_andnot_ps(clip_range_mask0_mask1, clip_range_min_zzzz_zzzz);
#elif defined(RTM_SSE2_INTRINSICS)
					const rtm::vector4f clip_range_min_xxxx0 = _mm_andnot_ps(clip_range_mask0, clip_range_min_xxxx);
					const rtm::vector4f clip_range_min_yyyy0 = _mm_andnot_ps(clip_range_mask0, clip_range_min_yyyy);
					const rtm::vector4f clip_range_min_zzzz0 = _mm_andnot_ps(clip_range_mask0, clip_range_min_zzzz);

					const rtm::vector4f clip_range_min_xxxx1 = _mm_andnot_ps(clip_range_mask1, clip_range_min_xxxx);
					const rtm::vector4f clip_range_min_yyyy1 = _mm_andnot_ps(clip_range_mask1, clip_range_min_yyyy);
					const rtm::vector4f clip_range_min_zzzz1 = _mm_andnot_ps(clip_range_mask1, clip_range_min_zzzz);
#elif defined(RTM_NEON_INTRINSICS)
					const rtm::vector4f clip_range_min_xxxx0 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(clip_range_min_xxxx), vreinterpretq_u32_f32(clip_range_mask0)));
					const rtm::vector4f clip_range_min_yyyy0 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(clip_range_min_yyyy), vreinterpretq_u32_f32(clip_range_mask0)));
					const rtm::vector4f clip_range_min_zzzz0 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(clip_range_min_zzzz), vreinterpretq_u32_f32(clip_range_mask0)));

					const rtm::vector4f clip_range_min_xxxx1 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(clip_range_min_xxxx), vreinterpretq_u32_f32(clip_range_mask1)));
					const rtm::vector4f clip_range_min_yyyy1 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(clip_range_min_yyyy), vreinterpretq_u32_f32(clip_range_mask1)));
					const rtm::vector4f clip_range_min_zzzz1 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(clip_range_min_zzzz), vreinterpretq_u32_f32(clip_range_mask1)));
#else
					const rtm::vector4f zero_v = rtm::vector_zero();

					const rtm::vector4f clip_range_min_xxxx0 = rtm::vector_select(clip_range_mask0, zero_v, clip_range_min_xxxx);
					const rtm::vector4f clip_range_min_yyyy0 = rtm::vector_select(clip_range_mask0, zero_v, clip_range_min_yyyy);
					const rtm::vector4f clip_range_min_zzzz0 = rtm::vector_select(clip_range_mask0, zero_v, clip_range_min_zzzz);

					const rtm::vector4f clip_range_min_xxxx1 = rtm::vector_select(clip_range_mask1, zero_v, clip_range_min_xxxx);
					const rtm::vector4f clip_range_min_yyyy1 = rtm::vector_select(clip_range_mask1, zero_v, clip_range_min_yyyy);
					const rtm::vector4f clip_range_min_zzzz1 = rtm::vector_select(clip_range_mask1, zero_v, clip_range_min_zzzz);
#endif

#if defined(RTM_AVX_INTRINSICS) && defined(ACL_IMPL_USE_AVX_DECOMP)
					clip_range_extent_xxxx_xxxx = _mm256_blendv_ps(clip_range_extent_xxxx_xxxx, one_v, clip_range_mask0_mask1);
					clip_range_extent_yyyy_yyyy = _mm256_blendv_ps(clip_range_extent_yyyy_yyyy, one_v, clip_range_mask0_mask1);
					clip_range_extent_zzzz_zzzz = _mm256_blendv_ps(clip_range_extent_zzzz_zzzz, one_v, clip_range_mask0_mask1);

					scratch_xxxx0_xxxx1 = _mm256_add_ps(_mm256_mul_ps(scratch_xxxx0_xxxx1, clip_range_extent_xxxx_xxxx), clip_range_min_xxxx_xxxx);
					scratch_yyyy0_yyyy1 = _mm256_add_ps(_mm256_mul_ps(scratch_yyyy0_yyyy1, clip_range_extent_yyyy_yyyy), clip_range_min_yyyy_yyyy);
					scratch_zzzz0_zzzz1 = _mm256_add_ps(_mm256_mul_ps(scratch_zzzz0_zzzz1, clip_range_extent_zzzz_zzzz), clip_range_min_zzzz_zzzz);
#else
					const rtm::vector4f one_v = rtm::vector_set(1.0F);

					const rtm::vector4f clip_range_extent_xxxx0 = rtm::vector_select(clip_range_mask0, one_v, clip_range_extent_xxxx);
					const rtm::vector4f clip_range_extent_yyyy0 = rtm::vector_select(clip_range_mask0, one_v, clip_range_extent_yyyy);
					const rtm::vector4f clip_range_extent_zzzz0 = rtm::vector_select(clip_range_mask0, one_v, clip_range_extent_zzzz);

					const rtm::vector4f clip_range_extent_xxxx1 = rtm::vector_select(clip_range_mask1, one_v, clip_range_extent_xxxx);
					const rtm::vector4f clip_range_extent_yyyy1 = rtm::vector_select(clip_range_mask1, one_v, clip_range_extent_yyyy);
					const rtm::vector4f clip_range_extent_zzzz1 = rtm::vector_select(clip_range_mask1, one_v, clip_range_extent_zzzz);

					scratch0_xxxx = rtm::vector_mul_add(scratch0_xxxx, clip_range_extent_xxxx0, clip_range_min_xxxx0);
					scratch0_yyyy = rtm::vector_mul_add(scratch0_yyyy, clip_range_extent_yyyy0, clip_range_min_yyyy0);
					scratch0_zzzz = rtm::vector_mul_add(scratch0_zzzz, clip_range_extent_zzzz0, clip_range_min_zzzz0);

					scratch1_xxxx = rtm::vector_mul_add(scratch1_xxxx, clip_range_extent_xxxx1, clip_range_min_xxxx1);
					scratch1_yyyy = rtm::vector_mul_add(scratch1_yyyy, clip_range_extent_yyyy1, clip_range_min_yyyy1);
					scratch1_zzzz = rtm::vector_mul_add(scratch1_zzzz, clip_range_extent_zzzz1, clip_range_min_zzzz1);
#endif

					// Skip our data
					clip_range_data += num_to_unpack * sizeof(rtm::float3f) * 2;
					clip_sampling_context.clip_range_data = clip_range_data;

					// Clip range data is 24-32 bytes per sub-track and as such we need to prefetch two cache lines ahead to process 4 sub-tracks
					ACL_IMPL_ANIMATED_PREFETCH(clip_range_data + 63);
					ACL_IMPL_ANIMATED_PREFETCH(clip_range_data + 127);
				}

				// For interpolation later
				rtm::vector4f xxxx_squared;
				rtm::vector4f yyyy_squared;
				rtm::vector4f zzzz_squared;

				// Reconstruct our quaternion W component in SOA
				if (rotation_format != rotation_format8::quatf_full || !decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					// quat_from_positive_w_soa
#if defined(RTM_AVX_INTRINSICS) && defined(ACL_IMPL_USE_AVX_DECOMP)
					const __m256 scratch_xxxx0_xxxx1_squared = _mm256_mul_ps(scratch_xxxx0_xxxx1, scratch_xxxx0_xxxx1);
					const __m256 scratch_yyyy0_yyyy1_squared = _mm256_mul_ps(scratch_yyyy0_yyyy1, scratch_yyyy0_yyyy1);
					const __m256 scratch_zzzz0_zzzz1_squared = _mm256_mul_ps(scratch_zzzz0_zzzz1, scratch_zzzz0_zzzz1);

					const __m256 scratch_wwww0_wwww1_squared = _mm256_sub_ps(_mm256_sub_ps(_mm256_sub_ps(one_v, scratch_xxxx0_xxxx1_squared), scratch_yyyy0_yyyy1_squared), scratch_zzzz0_zzzz1_squared);

					const __m256i abs_mask = _mm256_set1_epi32(0x7FFFFFFFULL);
					const __m256 scratch_wwww0_wwww1_squared_abs = _mm256_and_ps(scratch_wwww0_wwww1_squared, _mm256_castsi256_ps(abs_mask));

					__m256 scratch_wwww0_wwww1 = _mm256_sqrt_ps(scratch_wwww0_wwww1_squared_abs);

					// Try and help the compiler hide the latency from the square-root
					scratch0_xxxx = _mm256_extractf128_ps(scratch_xxxx0_xxxx1, 0);
					scratch1_xxxx = _mm256_extractf128_ps(scratch_xxxx0_xxxx1, 1);
					scratch0_yyyy = _mm256_extractf128_ps(scratch_yyyy0_yyyy1, 0);
					scratch1_yyyy = _mm256_extractf128_ps(scratch_yyyy0_yyyy1, 1);
					scratch0_zzzz = _mm256_extractf128_ps(scratch_zzzz0_zzzz1, 0);
					scratch1_zzzz = _mm256_extractf128_ps(scratch_zzzz0_zzzz1, 1);

					xxxx_squared = rtm::vector_mul(scratch0_xxxx, scratch1_xxxx);
					yyyy_squared = rtm::vector_mul(scratch0_yyyy, scratch1_yyyy);
					zzzz_squared = rtm::vector_mul(scratch0_zzzz, scratch1_zzzz);

					scratch0_wwww = _mm256_extractf128_ps(scratch_wwww0_wwww1, 0);
					scratch1_wwww = _mm256_extractf128_ps(scratch_wwww0_wwww1, 1);
#else
					const rtm::vector4f scratch0_xxxx_squared = rtm::vector_mul(scratch0_xxxx, scratch0_xxxx);
					const rtm::vector4f scratch0_yyyy_squared = rtm::vector_mul(scratch0_yyyy, scratch0_yyyy);
					const rtm::vector4f scratch0_zzzz_squared = rtm::vector_mul(scratch0_zzzz, scratch0_zzzz);
					const rtm::vector4f scratch0_wwww_squared = rtm::vector_sub(rtm::vector_sub(rtm::vector_sub(rtm::vector_set(1.0F), scratch0_xxxx_squared), scratch0_yyyy_squared), scratch0_zzzz_squared);

					const rtm::vector4f scratch1_xxxx_squared = rtm::vector_mul(scratch1_xxxx, scratch1_xxxx);
					const rtm::vector4f scratch1_yyyy_squared = rtm::vector_mul(scratch1_yyyy, scratch1_yyyy);
					const rtm::vector4f scratch1_zzzz_squared = rtm::vector_mul(scratch1_zzzz, scratch1_zzzz);
					const rtm::vector4f scratch1_wwww_squared = rtm::vector_sub(rtm::vector_sub(rtm::vector_sub(rtm::vector_set(1.0F), scratch1_xxxx_squared), scratch1_yyyy_squared), scratch1_zzzz_squared);

					// w_squared can be negative either due to rounding or due to quantization imprecision, we take the absolute value
					// to ensure the resulting quaternion is always normalized with a positive W component
					scratch0_wwww = rtm::vector_sqrt(rtm::vector_abs(scratch0_wwww_squared));
					scratch1_wwww = rtm::vector_sqrt(rtm::vector_abs(scratch1_wwww_squared));

					// Try and help the compiler hide the latency from the square-root
					xxxx_squared = rtm::vector_mul(scratch0_xxxx, scratch1_xxxx);
					yyyy_squared = rtm::vector_mul(scratch0_yyyy, scratch1_yyyy);
					zzzz_squared = rtm::vector_mul(scratch0_zzzz, scratch1_zzzz);
#endif
				}
				else
				{
					xxxx_squared = rtm::vector_mul(scratch0_xxxx, scratch1_xxxx);
					yyyy_squared = rtm::vector_mul(scratch0_yyyy, scratch1_yyyy);
					zzzz_squared = rtm::vector_mul(scratch0_zzzz, scratch1_zzzz);

					scratch0_wwww = scratch0[3];
					scratch1_wwww = scratch1[3];
				}

				// Interpolate linearly and store our rotations in SOA
				{
					// Calculate the vector4 dot product: dot(start, end)
					//const rtm::vector4f xxxx_squared = rtm::vector_mul(scratch0_xxxx, scratch1_xxxx);
					//const rtm::vector4f yyyy_squared = rtm::vector_mul(scratch0_yyyy, scratch1_yyyy);
					//const rtm::vector4f zzzz_squared = rtm::vector_mul(scratch0_zzzz, scratch1_zzzz);
					const rtm::vector4f wwww_squared = rtm::vector_mul(scratch0_wwww, scratch1_wwww);

					const rtm::vector4f dot4 = rtm::vector_add(rtm::vector_add(rtm::vector_add(xxxx_squared, yyyy_squared), zzzz_squared), wwww_squared);

					// Calculate the bias, if the dot product is positive or zero, there is no bias
					// but if it is negative, we want to flip the 'end' rotation XYZW components
					const rtm::vector4f neg_zero = rtm::vector_set(-0.0F);
					const rtm::vector4f bias = acl_impl::vector_and(dot4, neg_zero);

					// Apply our bias to the 'end'
					scratch1_xxxx = acl_impl::vector_xor(scratch1_xxxx, bias);
					scratch1_yyyy = acl_impl::vector_xor(scratch1_yyyy, bias);
					scratch1_zzzz = acl_impl::vector_xor(scratch1_zzzz, bias);
					scratch1_wwww = acl_impl::vector_xor(scratch1_wwww, bias);

					// Lerp the rotation after applying the bias
					// ((1.0 - alpha) * start) + (alpha * (end ^ bias)) == (start - alpha * start) + (alpha * (end ^ bias))
					const rtm::vector4f alpha = rtm::vector_set(decomp_context.interpolation_alpha);

					rtm::vector4f interp_xxxx = rtm::vector_mul_add(scratch1_xxxx, alpha, rtm::vector_neg_mul_sub(scratch0_xxxx, alpha, scratch0_xxxx));
					rtm::vector4f interp_yyyy = rtm::vector_mul_add(scratch1_yyyy, alpha, rtm::vector_neg_mul_sub(scratch0_yyyy, alpha, scratch0_yyyy));
					rtm::vector4f interp_zzzz = rtm::vector_mul_add(scratch1_zzzz, alpha, rtm::vector_neg_mul_sub(scratch0_zzzz, alpha, scratch0_zzzz));
					rtm::vector4f interp_wwww = rtm::vector_mul_add(scratch1_wwww, alpha, rtm::vector_neg_mul_sub(scratch0_wwww, alpha, scratch0_wwww));

					// Due to the interpolation, the result might not be anywhere near normalized!
					// Make sure to normalize afterwards before using
					const bool normalize_rotations = decompression_settings_type::normalize_rotations();
					if (normalize_rotations)
					{
						const rtm::vector4f interp_xxxx_squared = rtm::vector_mul(interp_xxxx, interp_xxxx);
						const rtm::vector4f interp_yyyy_squared = rtm::vector_mul(interp_yyyy, interp_yyyy);
						const rtm::vector4f interp_zzzz_squared = rtm::vector_mul(interp_zzzz, interp_zzzz);
						const rtm::vector4f interp_wwww_squared = rtm::vector_mul(interp_wwww, interp_wwww);

						const rtm::vector4f interp_dot4 = rtm::vector_add(rtm::vector_add(rtm::vector_add(interp_xxxx_squared, interp_yyyy_squared), interp_zzzz_squared), interp_wwww_squared);

						const rtm::vector4f interp_len = rtm::vector_sqrt(interp_dot4);
						const rtm::vector4f interp_inv_len = rtm::vector_div(rtm::vector_set(1.0F), interp_len);

						interp_xxxx = rtm::vector_mul(interp_xxxx, interp_inv_len);
						interp_yyyy = rtm::vector_mul(interp_yyyy, interp_inv_len);
						interp_zzzz = rtm::vector_mul(interp_zzzz, interp_inv_len);
						interp_wwww = rtm::vector_mul(interp_wwww, interp_inv_len);
					}

					// Swizzle out our 4 samples
					rtm::vector4f sample0;
					rtm::vector4f sample1;
					rtm::vector4f sample2;
					rtm::vector4f sample3;
					RTM_MATRIXF_TRANSPOSE_4X4(interp_xxxx, interp_yyyy, interp_zzzz, interp_wwww, sample0, sample1, sample2, sample3);

					rtm::quatf* cache_ptr = &rotations.cached_samples[cache_write_index];

					cache_ptr[0] = rtm::vector_to_quat(sample0);
					cache_ptr[1] = rtm::vector_to_quat(sample1);
					cache_ptr[2] = rtm::vector_to_quat(sample2);
					cache_ptr[3] = rtm::vector_to_quat(sample3);
				}
			}

			template<class decompression_settings_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void skip_rotation_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
				const uint32_t num_left_to_unpack = rotations.num_left_to_unpack;
				ACL_ASSERT(num_left_to_unpack != 0, "Cannot skip rotations that aren't present");

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				rotations.num_left_to_unpack = num_left_to_unpack - num_to_unpack;

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
				{
					const uint8_t* format_per_track_data0 = segment_sampling_context[0].format_per_track_data;
					const uint8_t* format_per_track_data1 = segment_sampling_context[1].format_per_track_data;

					uint32_t group_size0 = 0;
					uint32_t group_size1 = 0;

					// Fall-through intentional
					switch (num_to_unpack)
					{
					default:
					case 4:
						group_size0 += format_per_track_data0[3];
						group_size1 += format_per_track_data1[3];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 3:
						group_size0 += format_per_track_data0[2];
						group_size1 += format_per_track_data1[2];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 2:
						group_size0 += format_per_track_data0[1];
						group_size1 += format_per_track_data1[1];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 1:
						group_size0 += format_per_track_data0[0];
						group_size1 += format_per_track_data1[0];
					}

					// Per track data and segment range are always padded to 4 samples
					segment_sampling_context[0].format_per_track_data += 4;
					segment_sampling_context[0].segment_range_data += 6 * 4;
					segment_sampling_context[0].animated_track_data_bit_offset += group_size0 * 3;
					segment_sampling_context[1].format_per_track_data += 4;
					segment_sampling_context[1].segment_range_data += 6 * 4;
					segment_sampling_context[1].animated_track_data_bit_offset += group_size1 * 3;

					clip_sampling_context.clip_range_data += sizeof(rtm::float3f) * 2 * num_to_unpack;
				}
				else
				{
					uint32_t group_size;
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						group_size = 32 * 4 * num_to_unpack;
					else // drop w full
						group_size = 32 * 3 * num_to_unpack;

					segment_sampling_context[0].animated_track_data_bit_offset += group_size;
					segment_sampling_context[1].animated_track_data_bit_offset += group_size;
				}
			}

			template<class decompression_settings_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::quatf RTM_SIMD_CALL unpack_rotation_within_group(const persistent_transform_decompression_context_v0& decomp_context, const animated_group_cursor_v0& group_cursor, uint32_t unpack_index)
			{
				ACL_ASSERT(unpack_index < group_cursor.group_size, "Cannot unpack sample that isn't present");

				const clip_animated_sampling_context_v0& cursor_clip_sampling_context = group_cursor.clip_sampling_context;
				const uint32_t group_size = group_cursor.group_size;

				const rtm::vector4f sample_as_vec0 = unpack_single_animated_quat<decompression_settings_type>(decomp_context, unpack_index, group_size, cursor_clip_sampling_context, group_cursor.segment_sampling_context[0]);
				const rtm::vector4f sample_as_vec1 = unpack_single_animated_quat<decompression_settings_type>(decomp_context, unpack_index, group_size, cursor_clip_sampling_context, group_cursor.segment_sampling_context[1]);

				rtm::quatf sample0;
				rtm::quatf sample1;

				// Reconstruct our quaternion W component
				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				if (rotation_format != rotation_format8::quatf_full || !decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					sample0 = rtm::quat_from_positive_w(sample_as_vec0);
					sample1 = rtm::quat_from_positive_w(sample_as_vec1);
				}
				else
				{
					sample0 = rtm::vector_to_quat(sample_as_vec0);
					sample1 = rtm::vector_to_quat(sample_as_vec1);
				}

				// Due to the interpolation, the result might not be anywhere near normalized!
				// Make sure to normalize afterwards before using
				const bool normalize_rotations = decompression_settings_type::normalize_rotations();
				if (normalize_rotations)
					return rtm::quat_lerp(sample0, sample1, decomp_context.interpolation_alpha);
				else
					return quat_lerp_no_normalization(sample0, sample1, decomp_context.interpolation_alpha);
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::quatf RTM_SIMD_CALL consume_rotation()
			{
				ACL_ASSERT(rotations.cache_read_index < rotations.cache_write_index, "Attempting to consume an animated sample that isn't cached");
				const uint32_t cache_read_index = rotations.cache_read_index++;
				return rotations.cached_samples[cache_read_index % 8];
			}

			template<class decompression_settings_adapter_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_translation_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
				uint32_t num_left_to_unpack = translations.num_left_to_unpack;
				if (num_left_to_unpack == 0)
					return;	// Nothing left to do, we are done

							// If we have less than 4 cached samples, unpack 4 more and prefetch the next cache line
				const uint32_t num_cached = translations.get_num_cached();
				if (num_cached >= 4)
					return;	// Enough cached, nothing to do

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				num_left_to_unpack -= num_to_unpack;
				translations.num_left_to_unpack = num_left_to_unpack;

				// Write index will be either 0 or 4 here since we always unpack 4 at a time
				uint32_t cache_write_index = translations.cache_write_index % 8;
				translations.cache_write_index += num_to_unpack;

				unpack_animated_vector3<decompression_settings_adapter_type>(decomp_context, scratch0, num_to_unpack, clip_sampling_context, segment_sampling_context[0]);
				unpack_animated_vector3<decompression_settings_adapter_type>(decomp_context, scratch1, num_to_unpack, clip_sampling_context, segment_sampling_context[1]);

				const float interpolation_alpha = decomp_context.interpolation_alpha;
				for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
				{
					const rtm::vector4f sample0 = scratch0[unpack_index];
					const rtm::vector4f sample1 = scratch1[unpack_index];

					const rtm::vector4f sample = rtm::vector_lerp(sample0, sample1, interpolation_alpha);

					translations.cached_samples[cache_write_index] = sample;
					cache_write_index++;
				}

				// If we have some range reduction, skip the data we read
				if (are_any_enum_flags_set(decomp_context.range_reduction, range_reduction_flags8::translations))
					clip_sampling_context.clip_range_data += num_to_unpack * sizeof(rtm::float3f) * 2;

				// Clip range data is 24 bytes per sub-track and as such we need to prefetch two cache lines ahead to process 4 sub-tracks
				ACL_IMPL_ANIMATED_PREFETCH(clip_sampling_context.clip_range_data + 63);
				ACL_IMPL_ANIMATED_PREFETCH(clip_sampling_context.clip_range_data + 127);
			}

			template<class decompression_settings_adapter_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void skip_translation_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
				const uint32_t num_left_to_unpack = translations.num_left_to_unpack;
				ACL_ASSERT(num_left_to_unpack != 0, "Cannot skip translations that aren't present");

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				translations.num_left_to_unpack = num_left_to_unpack - num_to_unpack;

				const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));
				if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
				{
					const uint8_t* format_per_track_data0 = segment_sampling_context[0].format_per_track_data;
					const uint8_t* format_per_track_data1 = segment_sampling_context[1].format_per_track_data;

					uint32_t group_size0 = 0;
					uint32_t group_size1 = 0;

					// Fall-through intentional
					switch (num_to_unpack)
					{
					default:
					case 4:
						group_size0 += format_per_track_data0[3];
						group_size1 += format_per_track_data1[3];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 3:
						group_size0 += format_per_track_data0[2];
						group_size1 += format_per_track_data1[2];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 2:
						group_size0 += format_per_track_data0[1];
						group_size1 += format_per_track_data1[1];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 1:
						group_size0 += format_per_track_data0[0];
						group_size1 += format_per_track_data1[0];
					}

					segment_sampling_context[0].format_per_track_data += num_to_unpack;
					segment_sampling_context[0].segment_range_data += 6 * num_to_unpack;
					segment_sampling_context[0].animated_track_data_bit_offset += group_size0 * 3;
					segment_sampling_context[1].format_per_track_data += num_to_unpack;
					segment_sampling_context[1].segment_range_data += 6 * num_to_unpack;
					segment_sampling_context[1].animated_track_data_bit_offset += group_size1 * 3;

					clip_sampling_context.clip_range_data += sizeof(rtm::float3f) * 2 * num_to_unpack;
				}
				else
				{
					const uint32_t group_size = 32 * 3 * num_to_unpack;
					segment_sampling_context[0].animated_track_data_bit_offset += group_size;
					segment_sampling_context[1].animated_track_data_bit_offset += group_size;
				}
			}

			template<class decompression_settings_adapter_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_translation_within_group(const persistent_transform_decompression_context_v0& decomp_context, const animated_group_cursor_v0& group_cursor, uint32_t unpack_index)
			{
				ACL_ASSERT(unpack_index < group_cursor.group_size, "Cannot unpack sample that isn't present");

				const clip_animated_sampling_context_v0& cursor_clip_sampling_context = group_cursor.clip_sampling_context;

				const rtm::vector4f sample0 = unpack_single_animated_vector3<decompression_settings_adapter_type>(decomp_context, unpack_index, cursor_clip_sampling_context, group_cursor.segment_sampling_context[0]);
				const rtm::vector4f sample1 = unpack_single_animated_vector3<decompression_settings_adapter_type>(decomp_context, unpack_index, cursor_clip_sampling_context, group_cursor.segment_sampling_context[1]);

				return rtm::vector_lerp(sample0, sample1, decomp_context.interpolation_alpha);
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL consume_translation()
			{
				ACL_ASSERT(translations.cache_read_index < translations.cache_write_index, "Attempting to consume an animated sample that isn't cached");
				const uint32_t cache_read_index = translations.cache_read_index++;
				return translations.cached_samples[cache_read_index % 8];
			}

			template<class decompression_settings_adapter_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_scale_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
				uint32_t num_left_to_unpack = scales.num_left_to_unpack;
				if (num_left_to_unpack == 0)
					return;	// Nothing left to do, we are done

							// If we have less than 4 cached samples, unpack 4 more and prefetch the next cache line
				const uint32_t num_cached = scales.get_num_cached();
				if (num_cached >= 4)
					return;	// Enough cached, nothing to do

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				num_left_to_unpack -= num_to_unpack;
				scales.num_left_to_unpack = num_left_to_unpack;

				// Write index will be either 0 or 4 here since we always unpack 4 at a time
				uint32_t cache_write_index = scales.cache_write_index % 8;
				scales.cache_write_index += num_to_unpack;

				unpack_animated_vector3<decompression_settings_adapter_type>(decomp_context, scratch0, num_to_unpack, clip_sampling_context, segment_sampling_context[0]);
				unpack_animated_vector3<decompression_settings_adapter_type>(decomp_context, scratch1, num_to_unpack, clip_sampling_context, segment_sampling_context[1]);

				const float interpolation_alpha = decomp_context.interpolation_alpha;
				for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
				{
					const rtm::vector4f sample0 = scratch0[unpack_index];
					const rtm::vector4f sample1 = scratch1[unpack_index];

					const rtm::vector4f sample = rtm::vector_lerp(sample0, sample1, interpolation_alpha);

					scales.cached_samples[cache_write_index] = sample;
					cache_write_index++;
				}

				// If we have some range reduction, skip the data we read
				if (are_any_enum_flags_set(decomp_context.range_reduction, range_reduction_flags8::scales))
					clip_sampling_context.clip_range_data += num_to_unpack * sizeof(rtm::float3f) * 2;

				// Clip range data is 24 bytes per sub-track and as such we need to prefetch two cache lines ahead to process 4 sub-tracks
				ACL_IMPL_ANIMATED_PREFETCH(clip_sampling_context.clip_range_data + 63);
				ACL_IMPL_ANIMATED_PREFETCH(clip_sampling_context.clip_range_data + 127);
			}

			template<class decompression_settings_adapter_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void skip_scale_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
				const uint32_t num_left_to_unpack = scales.num_left_to_unpack;
				ACL_ASSERT(num_left_to_unpack != 0, "Cannot skip scales that aren't present");

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				scales.num_left_to_unpack = num_left_to_unpack - num_to_unpack;

				const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));
				if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
				{
					const uint8_t* format_per_track_data0 = segment_sampling_context[0].format_per_track_data;
					const uint8_t* format_per_track_data1 = segment_sampling_context[1].format_per_track_data;

					uint32_t group_size0 = 0;
					uint32_t group_size1 = 0;

					// Fall-through intentional
					switch (num_to_unpack)
					{
					default:
					case 4:
						group_size0 += format_per_track_data0[3];
						group_size1 += format_per_track_data1[3];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 3:
						group_size0 += format_per_track_data0[2];
						group_size1 += format_per_track_data1[2];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 2:
						group_size0 += format_per_track_data0[1];
						group_size1 += format_per_track_data1[1];
						ACL_SWITCH_CASE_FALLTHROUGH_INTENTIONAL;
					case 1:
						group_size0 += format_per_track_data0[0];
						group_size1 += format_per_track_data1[0];
					}

					segment_sampling_context[0].format_per_track_data += num_to_unpack;
					segment_sampling_context[0].segment_range_data += 6 * num_to_unpack;
					segment_sampling_context[0].animated_track_data_bit_offset += group_size0 * 3;
					segment_sampling_context[1].format_per_track_data += num_to_unpack;
					segment_sampling_context[1].segment_range_data += 6 * num_to_unpack;
					segment_sampling_context[1].animated_track_data_bit_offset += group_size1 * 3;

					clip_sampling_context.clip_range_data += sizeof(rtm::float3f) * 2 * num_to_unpack;
				}
				else
				{
					const uint32_t group_size = 32 * 3 * num_to_unpack;
					segment_sampling_context[0].animated_track_data_bit_offset += group_size;
					segment_sampling_context[1].animated_track_data_bit_offset += group_size;
				}
			}

			template<class decompression_settings_adapter_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_scale_within_group(const persistent_transform_decompression_context_v0& decomp_context, const animated_group_cursor_v0& group_cursor, uint32_t unpack_index)
			{
				// Same as translation but a different adapter
				return unpack_translation_within_group<decompression_settings_adapter_type>(decomp_context, group_cursor, unpack_index);
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL consume_scale()
			{
				ACL_ASSERT(scales.cache_read_index < scales.cache_write_index, "Attempting to consume an animated sample that isn't cached");
				const uint32_t cache_read_index = scales.cache_read_index++;
				return scales.cached_samples[cache_read_index % 8];
			}
		};
	}
}
ACL_IMPL_FILE_PRAGMA_POP
