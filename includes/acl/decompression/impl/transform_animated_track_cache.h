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

// This defined enables the SIMD 8 wide AVX decompression code path
// Note that currently, it is often slower than the regular SIMD 4 wide AVX code path
// On Intel Haswell and AMD Zen2 CPUs, the 8 wide code is measurably slower
// Perhaps it is faster on newer Intel CPUs but I don't have one to test with
// Enable at your own risk
//#define ACL_IMPL_USE_AVX_8_WIDE_DECOMP

#define ACL_IMPL_UNROLL_VAR_UNPACK

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
	#if !defined(RTM_AVX_INTRINSICS)
		// AVX isn't enabled, disable the 8 wide code path
		#undef ACL_IMPL_USE_AVX_8_WIDE_DECOMP
	#endif
#endif

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

		struct alignas(32) segment_animated_scratch_v0
		{
			// We store out potential range data in SOA form and we have no W, just XYZ
			// To facilitate AVX and wider SIMD usage, we store our data interleaved in a single contiguous array
			// Segment 0 has a base offset of 0 bytes and afterwards every write has a 32 byte offset
			// Segment 1 has a base offset of 16 bytes and afterwards every write has a 32 byte offset

			// segment_range_min_xxxx0, segment_range_min_xxxx1, segment_range_min_yyyy0, segment_range_min_yyyy1, segment_range_min_zzzz0, segment_range_min_zzzz1
			rtm::vector4f segment_range_min[6];

			// segment_range_extent_xxxx0, segment_range_extent_xxxx1, segment_range_extent_yyyy0, segment_range_extent_yyyy1, segment_range_extent_zzzz0, segment_range_extent_zzzz1
			rtm::vector4f segment_range_extent[6];

#if defined(ACL_IMPL_UNROLL_VAR_UNPACK)
			// We store our potential constant bit rate samples in AOS form with 16 bit per component
			// We have 3 components (XYZ, no W), each 16 bit wide, and we have 4 samples with 2 segments
			// Segment 0 has a base offset of 0 bytes
			// Segment 1 has a base offset of 32 bytes
			// Each segment uses 24 bytes but we pad to 32

			// constant_sample0_xyz0, constant_sample1_xyz0, constant_sample1_xyz0, constant_sample1_xyz0, padding (8 bytes), constant_sample0_xyz1, constant_sample1_xyz1, constant_sample1_xyz1, constant_sample1_xyz1, padding (8 bytes)
			uint8_t constant_sample_data[64];
#endif
		};

#if defined(RTM_SSE2_INTRINSICS)
		using range_reduction_masks_t = __m128i;
#elif defined(RTM_NEON_INTRINSICS)
		using range_reduction_masks_t = int16x8_t;
#else
		using range_reduction_masks_t = uint64_t;
#endif

		// About 9 cycles with AVX on Skylake
		// Constant unpacking adds about 9 cycles
		inline ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_segment_range_data(const uint8_t* segment_range_data, uint32_t scratch_offset, segment_animated_scratch_v0& output_scratch)
		{
			// Segment range is packed: min.xxxx, min.yyyy, min.zzzz, extent.xxxx, extent.yyyy, extent.zzzz

#if defined(RTM_SSE2_INTRINSICS)
			const __m128i zero = _mm_setzero_si128();

			const __m128i segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8 = _mm_loadu_si128((const __m128i*)segment_range_data);
			const __m128i segment_range_extent_yyyy_zzzz_u8 = _mm_loadu_si128((const __m128i*)(segment_range_data + 16));

#if defined(ACL_IMPL_UNROLL_VAR_UNPACK)
			// Our constant sample value is packed 8 bits in each group in the sample's lane
			// To load our sample, we need to load: (min.x[unpack_index] << 8) | min.y[unpack_index], (min.z[unpack_index] << 8) | extent.x[unpack_index], (extent.y[unpack_index] << 8) | extent.z[unpack_index]
			// This is more complicated than if we were in AOS form but constant bit rates are somewhat rare while nearly every sample
			// has segment range information which is a lot simpler to load in SOA form
			// Keep things in big endian order since we swap later

			const uint16_t x0 = (uint16_t(segment_range_data[4]) << 8) | segment_range_data[0];
			const uint16_t x1 = (uint16_t(segment_range_data[5]) << 8) | segment_range_data[1];
			const uint16_t x2 = (uint16_t(segment_range_data[6]) << 8) | segment_range_data[2];
			const uint16_t x3 = (uint16_t(segment_range_data[7]) << 8) | segment_range_data[3];

			const uint16_t y0 = (uint16_t(segment_range_data[12]) << 8) | segment_range_data[8];
			const uint16_t y1 = (uint16_t(segment_range_data[13]) << 8) | segment_range_data[9];
			const uint16_t y2 = (uint16_t(segment_range_data[14]) << 8) | segment_range_data[10];
			const uint16_t y3 = (uint16_t(segment_range_data[15]) << 8) | segment_range_data[11];

			const uint16_t z0 = (uint16_t(segment_range_data[20]) << 8) | segment_range_data[16];
			const uint16_t z1 = (uint16_t(segment_range_data[21]) << 8) | segment_range_data[17];
			const uint16_t z2 = (uint16_t(segment_range_data[22]) << 8) | segment_range_data[18];
			const uint16_t z3 = (uint16_t(segment_range_data[23]) << 8) | segment_range_data[19];

			uint16_t* constant_sample_scratch = reinterpret_cast<uint16_t*>(&output_scratch.constant_sample_data[scratch_offset * 32]);
			constant_sample_scratch[0] = x0;
			constant_sample_scratch[1] = y0;
			constant_sample_scratch[2] = z0;

			constant_sample_scratch[3] = x1;
			constant_sample_scratch[4] = y1;
			constant_sample_scratch[5] = z1;

			constant_sample_scratch[6] = x2;
			constant_sample_scratch[7] = y2;
			constant_sample_scratch[8] = z2;

			constant_sample_scratch[9] = x3;
			constant_sample_scratch[10] = y3;
			constant_sample_scratch[11] = z3;

#if 0
			const __m128i segment_range_min_yyyy_zzzz_extent_xxxx_u8 = _mm_srli_si128(segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8, 4);
			const __m128i segment_range_extent_zzzz_u8 = _mm_srli_si128(segment_range_extent_yyyy_zzzz_u8, 4);

			// Unpack in the low part of the register the 4x components: [x0 x1 x2 x3, ?? ?? ?? ??], [y0 y1 y2 y3, ?? ?? ?? ??], [z0 z1 z2 z3, ?? ?? ?? ??]
			const __m128i constant_sample_x0x1x2x3_u16 = _mm_unpacklo_epi8(segment_range_min_yyyy_zzzz_extent_xxxx_u8, segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8);
			const __m128i constant_sample_y0y1y2y3_u16 = _mm_unpackhi_epi8(segment_range_min_yyyy_zzzz_extent_xxxx_u8, segment_range_min_xxxx_yyyy_zzzz_extent_xxxx_u8);
			const __m128i constant_sample_z0z1z2z3_u16 = _mm_unpacklo_epi8(segment_range_extent_zzzz_u8, segment_range_extent_yyyy_zzzz_u8);

			// We now have to swizzle our u16 values into AOS: [x0 y0 z0 x1 y1 z1 x2 y2], [z2 x3 y3 z3 ?? ?? ?? ??]
			const __m128i constant_sample_z0z0_z1z1_z2z2_z3z3_u16 = _mm_unpacklo_epi16(constant_sample_z0z1z2z3_u16, constant_sample_z0z1z2z3_u16);

			// We wish to store our constant samples in AOS to mirror the variable packed data
			const __m128i constant_sample_x0y0_x1y1_x2y2_x3y3_u16 = _mm_unpacklo_epi16(constant_sample_x0x1x2x3_u16, constant_sample_y0y1y2y3_u16);
			const __m128i constant_sample_x1y1_x2y2_x3y3_u16 = _mm_srli_si128(constant_sample_x0y0_x1y1_x2y2_x3y3_u16, 4);
			const __m128i constant_sample_x0y0z0z1_x1y1z2z3_u16 = _mm_unpacklo_epi32(constant_sample_x0y0_x1y1_x2y2_x3y3_u16, constant_sample_z0z1z2z3_u16);
			const __m128i constant_sample_x2y2z2z3_x3y3_u16 = _mm_unpackhi_epi32(constant_sample_x0y0_x1y1_x2y2_x3y3_u16, constant_sample_z0z1z2z3_u16);

			// Perform an aligned store, it's safe
			_mm_store_si128(reinterpret_cast<__m128i*>(&output_scratch.constant_sample_data[0]), constant_sample_xxxx_yyyy_zzzz_u16);

			const uint16_t tmp1[] =
			{
				uint16_t((uint16_t(segment_range_data[0]) << 8) | uint16_t(segment_range_data[4])),
				uint16_t((uint16_t(segment_range_data[8]) << 8) | uint16_t(segment_range_data[12])),
				uint16_t((uint16_t(segment_range_data[16]) << 8) | uint16_t(segment_range_data[20])),

				uint16_t((uint16_t(segment_range_data[1]) << 8) | uint16_t(segment_range_data[5])),
				uint16_t((uint16_t(segment_range_data[9]) << 8) | uint16_t(segment_range_data[13])),
				uint16_t((uint16_t(segment_range_data[17]) << 8) | uint16_t(segment_range_data[21])),

				uint16_t((uint16_t(segment_range_data[2]) << 8) | uint16_t(segment_range_data[6])),
				uint16_t((uint16_t(segment_range_data[10]) << 8) | uint16_t(segment_range_data[14])),
				uint16_t((uint16_t(segment_range_data[18]) << 8) | uint16_t(segment_range_data[22])),

				uint16_t((uint16_t(segment_range_data[3]) << 8) | uint16_t(segment_range_data[7])),
				uint16_t((uint16_t(segment_range_data[11]) << 8) | uint16_t(segment_range_data[15])),
				uint16_t((uint16_t(segment_range_data[19]) << 8) | uint16_t(segment_range_data[23])),
			}; (void)tmp1;

			const uint16_t* tmp = reinterpret_cast<const uint16_t*>(&output_scratch.constant_sample_data[0]); (void)tmp;
			ACL_ASSERT(tmp[0] == tmp1[0], "!!"); // x0
			ACL_ASSERT(tmp[1] == tmp1[1], "!!"); // y0
			ACL_ASSERT(tmp[2] == tmp1[2], "!!"); // z0

			ACL_ASSERT(tmp[3] == tmp1[3], "!!"); // x1
			ACL_ASSERT(tmp[4] == tmp1[4], "!!"); // y1
			ACL_ASSERT(tmp[5] == tmp1[5], "!!"); // z1

			ACL_ASSERT(tmp[6] == tmp1[6], "!!"); // x2
			ACL_ASSERT(tmp[7] == tmp1[7], "!!"); // y2
			ACL_ASSERT(tmp[8] == tmp1[8], "!!"); // z2

			ACL_ASSERT(tmp[9] == tmp1[9], "!!"); // x3
			ACL_ASSERT(tmp[10] == tmp1[10], "!!"); // y3
			ACL_ASSERT(tmp[11] == tmp1[11], "!!"); // z3
#endif
#endif

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
#endif

			// Skip our used segment range data, all groups are padded to 4 elements
			segment_range_data += 6 * 4;

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

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
			// With AVX, we must duplicate our data for the first segment in case we don't have a second segment
			if (scratch_offset == 0)
			{
				// First segment data is duplicated
				// Use 256 bit stores to avoid doing too many stores which might stall
				_mm256_store_ps(reinterpret_cast<float*>(&output_scratch.segment_range_min[0]), _mm256_set_m128(segment_range_min_xxxx, segment_range_min_xxxx));
				_mm256_store_ps(reinterpret_cast<float*>(&output_scratch.segment_range_min[2]), _mm256_set_m128(segment_range_min_yyyy, segment_range_min_yyyy));
				_mm256_store_ps(reinterpret_cast<float*>(&output_scratch.segment_range_min[4]), _mm256_set_m128(segment_range_min_zzzz, segment_range_min_zzzz));
				_mm256_store_ps(reinterpret_cast<float*>(&output_scratch.segment_range_extent[0]), _mm256_set_m128(segment_range_extent_xxxx, segment_range_extent_xxxx));
				_mm256_store_ps(reinterpret_cast<float*>(&output_scratch.segment_range_extent[2]), _mm256_set_m128(segment_range_extent_yyyy, segment_range_extent_yyyy));
				_mm256_store_ps(reinterpret_cast<float*>(&output_scratch.segment_range_extent[4]), _mm256_set_m128(segment_range_extent_zzzz, segment_range_extent_zzzz));
			}
			else
			{
				// Second segment overwrites our data
				output_scratch.segment_range_min[1] = segment_range_min_xxxx;
				output_scratch.segment_range_min[3] = segment_range_min_yyyy;
				output_scratch.segment_range_min[5] = segment_range_min_zzzz;
				output_scratch.segment_range_extent[1] = segment_range_extent_xxxx;
				output_scratch.segment_range_extent[3] = segment_range_extent_yyyy;
				output_scratch.segment_range_extent[5] = segment_range_extent_zzzz;
			}
#else
			output_scratch.segment_range_min[scratch_offset + 0] = segment_range_min_xxxx;
			output_scratch.segment_range_min[scratch_offset + 2] = segment_range_min_yyyy;
			output_scratch.segment_range_min[scratch_offset + 4] = segment_range_min_zzzz;
			output_scratch.segment_range_extent[scratch_offset + 0] = segment_range_extent_xxxx;
			output_scratch.segment_range_extent[scratch_offset + 2] = segment_range_extent_yyyy;
			output_scratch.segment_range_extent[scratch_offset + 4] = segment_range_extent_zzzz;
#endif
		}

		// About 19 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL remap_segment_range_data4(const segment_animated_scratch_v0& segment_scratch, uint32_t scratch_offset, range_reduction_masks_t range_reduction_masks,
			rtm::vector4f& xxxx, rtm::vector4f& yyyy, rtm::vector4f& zzzz)
		{
			// Load and mask out our segment range data
			const rtm::vector4f one_v = rtm::vector_set(1.0F);

			rtm::vector4f segment_range_min_xxxx = segment_scratch.segment_range_min[scratch_offset + 0];
			rtm::vector4f segment_range_min_yyyy = segment_scratch.segment_range_min[scratch_offset + 2];
			rtm::vector4f segment_range_min_zzzz = segment_scratch.segment_range_min[scratch_offset + 4];

			rtm::vector4f segment_range_extent_xxxx = segment_scratch.segment_range_extent[scratch_offset + 0];
			rtm::vector4f segment_range_extent_yyyy = segment_scratch.segment_range_extent[scratch_offset + 2];
			rtm::vector4f segment_range_extent_zzzz = segment_scratch.segment_range_extent[scratch_offset + 4];

#if defined(RTM_SSE2_INTRINSICS)
			// Mask out the segment min we ignore
			const rtm::mask4f segment_range_ignore_mask_v = _mm_castsi128_ps(_mm_unpacklo_epi16(range_reduction_masks, range_reduction_masks));

			segment_range_min_xxxx = _mm_andnot_ps(segment_range_ignore_mask_v, segment_range_min_xxxx);
			segment_range_min_yyyy = _mm_andnot_ps(segment_range_ignore_mask_v, segment_range_min_yyyy);
			segment_range_min_zzzz = _mm_andnot_ps(segment_range_ignore_mask_v, segment_range_min_zzzz);
#elif defined(RTM_NEON_INTRINSICS)
			// Mask out the segment min we ignore
			const uint32x4_t segment_range_ignore_mask_v = vreinterpretq_u32_s32(vmovl_s16(vget_low_s16(range_reduction_masks)));

			segment_range_min_xxxx = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(segment_range_min_xxxx), segment_range_ignore_mask_v));
			segment_range_min_yyyy = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(segment_range_min_yyyy), segment_range_ignore_mask_v));
			segment_range_min_zzzz = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(segment_range_min_zzzz), segment_range_ignore_mask_v));
#else
			const rtm::vector4f zero_v = rtm::vector_zero();

			const uint32_t segment_range_mask_u32 = uint32_t(range_reduction_masks);
			const rtm::mask4f segment_range_mask = rtm::mask_set((segment_range_mask_u32 & 0x000000FF) != 0, (segment_range_mask_u32 & 0x0000FF00) != 0, (segment_range_mask_u32 & 0x00FF0000) != 0, (segment_range_mask_u32 & 0xFF000000) != 0);

			segment_range_min_xxxx = rtm::vector_select(segment_range_mask, zero_v, segment_range_min_xxxx);
			segment_range_min_yyyy = rtm::vector_select(segment_range_mask, zero_v, segment_range_min_yyyy);
			segment_range_min_zzzz = rtm::vector_select(segment_range_mask, zero_v, segment_range_min_zzzz);
#endif

			// Mask out the segment extent we ignore
			segment_range_extent_xxxx = rtm::vector_select(segment_range_ignore_mask_v, one_v, segment_range_extent_xxxx);
			segment_range_extent_yyyy = rtm::vector_select(segment_range_ignore_mask_v, one_v, segment_range_extent_yyyy);
			segment_range_extent_zzzz = rtm::vector_select(segment_range_ignore_mask_v, one_v, segment_range_extent_zzzz);

			// Remap
			xxxx = rtm::vector_mul_add(xxxx, segment_range_extent_xxxx, segment_range_min_xxxx);
			yyyy = rtm::vector_mul_add(yyyy, segment_range_extent_yyyy, segment_range_min_yyyy);
			zzzz = rtm::vector_mul_add(zzzz, segment_range_extent_zzzz, segment_range_min_zzzz);
		}

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL remap_segment_range_data_avx8(const segment_animated_scratch_v0& segment_scratch,
			range_reduction_masks_t range_reduction_masks0, range_reduction_masks_t range_reduction_masks1,
			__m256& xxxx0_xxxx1, __m256& yyyy0_yyyy1, __m256& zzzz0_zzzz1)
		{
			// Load and mask out our segment range data
			const __m256 one_v = _mm256_set1_ps(1.0F);

			__m256 segment_range_min_xxxx0_xxxx1 = _mm256_load_ps(reinterpret_cast<const float*>(&segment_scratch.segment_range_min[0]));
			__m256 segment_range_min_yyyy0_yyyy1 = _mm256_load_ps(reinterpret_cast<const float*>(&segment_scratch.segment_range_min[2]));
			__m256 segment_range_min_zzzz0_zzzz1 = _mm256_load_ps(reinterpret_cast<const float*>(&segment_scratch.segment_range_min[4]));

			__m256 segment_range_extent_xxxx0_xxxx1 = _mm256_load_ps(reinterpret_cast<const float*>(&segment_scratch.segment_range_extent[0]));
			__m256 segment_range_extent_yyyy0_yyyy1 = _mm256_load_ps(reinterpret_cast<const float*>(&segment_scratch.segment_range_extent[2]));
			__m256 segment_range_extent_zzzz0_zzzz1 = _mm256_load_ps(reinterpret_cast<const float*>(&segment_scratch.segment_range_extent[4]));

			// Mask out the segment min we ignore
			const __m128 segment_range_ignore_mask_v0 = _mm_castsi128_ps(_mm_unpacklo_epi16(range_reduction_masks0, range_reduction_masks0));
			const __m128 segment_range_ignore_mask_v1 = _mm_castsi128_ps(_mm_unpacklo_epi16(range_reduction_masks1, range_reduction_masks1));

			const __m256 segment_range_mask0_mask1 = _mm256_set_m128(segment_range_ignore_mask_v1, segment_range_ignore_mask_v0);

			segment_range_min_xxxx0_xxxx1 = _mm256_andnot_ps(segment_range_mask0_mask1, segment_range_min_xxxx0_xxxx1);
			segment_range_min_yyyy0_yyyy1 = _mm256_andnot_ps(segment_range_mask0_mask1, segment_range_min_yyyy0_yyyy1);
			segment_range_min_zzzz0_zzzz1 = _mm256_andnot_ps(segment_range_mask0_mask1, segment_range_min_zzzz0_zzzz1);

			segment_range_extent_xxxx0_xxxx1 = _mm256_blendv_ps(segment_range_extent_xxxx0_xxxx1, one_v, segment_range_mask0_mask1);
			segment_range_extent_yyyy0_yyyy1 = _mm256_blendv_ps(segment_range_extent_yyyy0_yyyy1, one_v, segment_range_mask0_mask1);
			segment_range_extent_zzzz0_zzzz1 = _mm256_blendv_ps(segment_range_extent_zzzz0_zzzz1, one_v, segment_range_mask0_mask1);

			xxxx0_xxxx1 = _mm256_add_ps(_mm256_mul_ps(xxxx0_xxxx1, segment_range_extent_xxxx0_xxxx1), segment_range_min_xxxx0_xxxx1);
			yyyy0_yyyy1 = _mm256_add_ps(_mm256_mul_ps(yyyy0_yyyy1, segment_range_extent_yyyy0_yyyy1), segment_range_min_yyyy0_yyyy1);
			zzzz0_zzzz1 = _mm256_add_ps(_mm256_mul_ps(zzzz0_zzzz1, segment_range_extent_zzzz0_zzzz1), segment_range_min_zzzz0_zzzz1);
		}
#endif

		// About 24 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL remap_clip_range_data4(const uint8_t* clip_range_data, uint32_t num_to_unpack,
			range_reduction_masks_t range_reduction_masks0, range_reduction_masks_t range_reduction_masks1,
			rtm::vector4f& xxxx0, rtm::vector4f& yyyy0, rtm::vector4f& zzzz0,
			rtm::vector4f& xxxx1, rtm::vector4f& yyyy1, rtm::vector4f& zzzz1)
		{
			// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
			const uint32_t load_size = num_to_unpack * sizeof(float);

#if defined(RTM_SSE2_INTRINSICS)
			const __m128 clip_range_mask0 = _mm_castsi128_ps(_mm_unpackhi_epi16(range_reduction_masks0, range_reduction_masks0));
			const __m128 clip_range_mask1 = _mm_castsi128_ps(_mm_unpackhi_epi16(range_reduction_masks1, range_reduction_masks1));
#elif defined(RTM_NEON_INTRINSICS)
			const float32x4_t clip_range_mask0 = vreinterpretq_f32_s32(vmovl_s16(vget_high_s16(range_reduction_masks0)));
			const float32x4_t clip_range_mask1 = vreinterpretq_f32_s32(vmovl_s16(vget_high_s16(range_reduction_masks1)));
#else
			const uint32_t clip_range_mask_u32_0 = uint32_t(range_reduction_masks0 >> 32);
			const uint32_t clip_range_mask_u32_1 = uint32_t(range_reduction_masks1 >> 32);
			const rtm::mask4f clip_range_mask0 = rtm::mask_set((clip_range_mask_u32_0 & 0x000000FF) != 0, (clip_range_mask_u32_0 & 0x0000FF00) != 0, (clip_range_mask_u32_0 & 0x00FF0000) != 0, (clip_range_mask_u32_0 & 0xFF000000) != 0);
			const rtm::mask4f clip_range_mask1 = rtm::mask_set((clip_range_mask_u32_1 & 0x000000FF) != 0, (clip_range_mask_u32_1 & 0x0000FF00) != 0, (clip_range_mask_u32_1 & 0x00FF0000) != 0, (clip_range_mask_u32_1 & 0xFF000000) != 0);
#endif

			const rtm::vector4f clip_range_min_xxxx = rtm::vector_load(clip_range_data + load_size * 0);
			const rtm::vector4f clip_range_min_yyyy = rtm::vector_load(clip_range_data + load_size * 1);
			const rtm::vector4f clip_range_min_zzzz = rtm::vector_load(clip_range_data + load_size * 2);

			const rtm::vector4f clip_range_extent_xxxx = rtm::vector_load(clip_range_data + load_size * 3);
			const rtm::vector4f clip_range_extent_yyyy = rtm::vector_load(clip_range_data + load_size * 4);
			const rtm::vector4f clip_range_extent_zzzz = rtm::vector_load(clip_range_data + load_size * 5);

			// Mask out the clip ranges we ignore
#if defined(RTM_SSE2_INTRINSICS)
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

			const rtm::vector4f one_v = rtm::vector_set(1.0F);

			const rtm::vector4f clip_range_extent_xxxx0 = rtm::vector_select(clip_range_mask0, one_v, clip_range_extent_xxxx);
			const rtm::vector4f clip_range_extent_yyyy0 = rtm::vector_select(clip_range_mask0, one_v, clip_range_extent_yyyy);
			const rtm::vector4f clip_range_extent_zzzz0 = rtm::vector_select(clip_range_mask0, one_v, clip_range_extent_zzzz);

			const rtm::vector4f clip_range_extent_xxxx1 = rtm::vector_select(clip_range_mask1, one_v, clip_range_extent_xxxx);
			const rtm::vector4f clip_range_extent_yyyy1 = rtm::vector_select(clip_range_mask1, one_v, clip_range_extent_yyyy);
			const rtm::vector4f clip_range_extent_zzzz1 = rtm::vector_select(clip_range_mask1, one_v, clip_range_extent_zzzz);

			xxxx0 = rtm::vector_mul_add(xxxx0, clip_range_extent_xxxx0, clip_range_min_xxxx0);
			yyyy0 = rtm::vector_mul_add(yyyy0, clip_range_extent_yyyy0, clip_range_min_yyyy0);
			zzzz0 = rtm::vector_mul_add(zzzz0, clip_range_extent_zzzz0, clip_range_min_zzzz0);

			xxxx1 = rtm::vector_mul_add(xxxx1, clip_range_extent_xxxx1, clip_range_min_xxxx1);
			yyyy1 = rtm::vector_mul_add(yyyy1, clip_range_extent_yyyy1, clip_range_min_yyyy1);
			zzzz1 = rtm::vector_mul_add(zzzz1, clip_range_extent_zzzz1, clip_range_min_zzzz1);
		}

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL remap_clip_range_data_avx8(const uint8_t* clip_range_data, uint32_t num_to_unpack,
			range_reduction_masks_t range_reduction_masks0, range_reduction_masks_t range_reduction_masks1,
			__m256& xxxx0_xxxx1, __m256& yyyy0_yyyy1, __m256& zzzz0_zzzz1)
		{
			const __m256 one_v = _mm256_set1_ps(1.0F);

			// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
			const uint32_t load_size = num_to_unpack * sizeof(float);

			const __m128 clip_range_mask0 = _mm_castsi128_ps(_mm_unpackhi_epi16(range_reduction_masks0, range_reduction_masks0));
			const __m128 clip_range_mask1 = _mm_castsi128_ps(_mm_unpackhi_epi16(range_reduction_masks1, range_reduction_masks1));

			const __m256 clip_range_mask0_mask1 = _mm256_set_m128(clip_range_mask1, clip_range_mask0);

			const rtm::vector4f clip_range_min_xxxx = rtm::vector_load(clip_range_data + load_size * 0);
			const rtm::vector4f clip_range_min_yyyy = rtm::vector_load(clip_range_data + load_size * 1);
			const rtm::vector4f clip_range_min_zzzz = rtm::vector_load(clip_range_data + load_size * 2);

			const rtm::vector4f clip_range_extent_xxxx = rtm::vector_load(clip_range_data + load_size * 3);
			const rtm::vector4f clip_range_extent_yyyy = rtm::vector_load(clip_range_data + load_size * 4);
			const rtm::vector4f clip_range_extent_zzzz = rtm::vector_load(clip_range_data + load_size * 5);

			__m256 clip_range_min_xxxx_xxxx = _mm256_set_m128(clip_range_min_xxxx, clip_range_min_xxxx);
			__m256 clip_range_min_yyyy_yyyy = _mm256_set_m128(clip_range_min_yyyy, clip_range_min_yyyy);
			__m256 clip_range_min_zzzz_zzzz = _mm256_set_m128(clip_range_min_zzzz, clip_range_min_zzzz);

			__m256 clip_range_extent_xxxx_xxxx = _mm256_set_m128(clip_range_extent_xxxx, clip_range_extent_xxxx);
			__m256 clip_range_extent_yyyy_yyyy = _mm256_set_m128(clip_range_extent_yyyy, clip_range_extent_yyyy);
			__m256 clip_range_extent_zzzz_zzzz = _mm256_set_m128(clip_range_extent_zzzz, clip_range_extent_zzzz);

			// Mask out the clip ranges we ignore
			clip_range_min_xxxx_xxxx = _mm256_andnot_ps(clip_range_mask0_mask1, clip_range_min_xxxx_xxxx);
			clip_range_min_yyyy_yyyy = _mm256_andnot_ps(clip_range_mask0_mask1, clip_range_min_yyyy_yyyy);
			clip_range_min_zzzz_zzzz = _mm256_andnot_ps(clip_range_mask0_mask1, clip_range_min_zzzz_zzzz);

			clip_range_extent_xxxx_xxxx = _mm256_blendv_ps(clip_range_extent_xxxx_xxxx, one_v, clip_range_mask0_mask1);
			clip_range_extent_yyyy_yyyy = _mm256_blendv_ps(clip_range_extent_yyyy_yyyy, one_v, clip_range_mask0_mask1);
			clip_range_extent_zzzz_zzzz = _mm256_blendv_ps(clip_range_extent_zzzz_zzzz, one_v, clip_range_mask0_mask1);

			xxxx0_xxxx1 = _mm256_add_ps(_mm256_mul_ps(xxxx0_xxxx1, clip_range_extent_xxxx_xxxx), clip_range_min_xxxx_xxxx);
			yyyy0_yyyy1 = _mm256_add_ps(_mm256_mul_ps(yyyy0_yyyy1, clip_range_extent_yyyy_yyyy), clip_range_min_yyyy_yyyy);
			zzzz0_zzzz1 = _mm256_add_ps(_mm256_mul_ps(zzzz0_zzzz1, clip_range_extent_zzzz_zzzz), clip_range_min_zzzz_zzzz);
		}
#endif

		// About 31 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL quat_from_positive_w4(rtm::vector4f xxxx, rtm::vector4f yyyy, rtm::vector4f zzzz)
		{
			const rtm::vector4f xxxx_squared = rtm::vector_mul(xxxx, xxxx);
			const rtm::vector4f yyyy_squared = rtm::vector_mul(yyyy, yyyy);
			const rtm::vector4f zzzz_squared = rtm::vector_mul(zzzz, zzzz);
			const rtm::vector4f wwww_squared = rtm::vector_sub(rtm::vector_sub(rtm::vector_sub(rtm::vector_set(1.0F), xxxx_squared), yyyy_squared), zzzz_squared);

			// w_squared can be negative either due to rounding or due to quantization imprecision, we take the absolute value
			// to ensure the resulting quaternion is always normalized with a positive W component
			return rtm::vector_sqrt(rtm::vector_abs(wwww_squared));
		}

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK __m256 RTM_SIMD_CALL quat_from_positive_w_avx8(__m256 xxxx0_xxxx1, __m256 yyyy0_yyyy1, __m256 zzzz0_zzzz1)
		{
			const __m256 one_v = _mm256_set1_ps(1.0F);

			const __m256 xxxx0_xxxx1_squared = _mm256_mul_ps(xxxx0_xxxx1, xxxx0_xxxx1);
			const __m256 yyyy0_yyyy1_squared = _mm256_mul_ps(yyyy0_yyyy1, yyyy0_yyyy1);
			const __m256 zzzz0_zzzz1_squared = _mm256_mul_ps(zzzz0_zzzz1, zzzz0_zzzz1);

			const __m256 wwww0_wwww1_squared = _mm256_sub_ps(_mm256_sub_ps(_mm256_sub_ps(one_v, xxxx0_xxxx1_squared), yyyy0_yyyy1_squared), zzzz0_zzzz1_squared);

			const __m256i abs_mask = _mm256_set1_epi32(0x7FFFFFFFULL);
			const __m256 wwww0_wwww1_squared_abs = _mm256_and_ps(wwww0_wwww1_squared, _mm256_castsi256_ps(abs_mask));

			return _mm256_sqrt_ps(wwww0_wwww1_squared_abs);
		}
#endif

		// About 28 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL quat_lerp4(
			rtm::vector4f xxxx0, rtm::vector4f yyyy0, rtm::vector4f zzzz0, rtm::vector4f wwww0,
			rtm::vector4f xxxx1, rtm::vector4f yyyy1, rtm::vector4f zzzz1, rtm::vector4f wwww1,
			float interpolation_alpha,
			rtm::vector4f& interp_xxxx, rtm::vector4f& interp_yyyy, rtm::vector4f& interp_zzzz, rtm::vector4f& interp_wwww)
		{
			// Calculate the vector4 dot product: dot(start, end)
			const rtm::vector4f xxxx_squared = rtm::vector_mul(xxxx0, xxxx1);
			const rtm::vector4f yyyy_squared = rtm::vector_mul(yyyy0, yyyy1);
			const rtm::vector4f zzzz_squared = rtm::vector_mul(zzzz0, zzzz1);
			const rtm::vector4f wwww_squared = rtm::vector_mul(wwww0, wwww1);

			const rtm::vector4f dot4 = rtm::vector_add(rtm::vector_add(rtm::vector_add(xxxx_squared, yyyy_squared), zzzz_squared), wwww_squared);

			// Calculate the bias, if the dot product is positive or zero, there is no bias
			// but if it is negative, we want to flip the 'end' rotation XYZW components
			const rtm::vector4f neg_zero = rtm::vector_set(-0.0F);
			const rtm::vector4f bias = acl_impl::vector_and(dot4, neg_zero);

			// Apply our bias to the 'end'
			xxxx1 = acl_impl::vector_xor(xxxx1, bias);
			yyyy1 = acl_impl::vector_xor(yyyy1, bias);
			zzzz1 = acl_impl::vector_xor(zzzz1, bias);
			wwww1 = acl_impl::vector_xor(wwww1, bias);

			// Lerp the rotation after applying the bias
			// ((1.0 - alpha) * start) + (alpha * (end ^ bias)) == (start - alpha * start) + (alpha * (end ^ bias))
			const rtm::vector4f alpha = rtm::vector_set(interpolation_alpha);

			interp_xxxx = rtm::vector_mul_add(xxxx1, alpha, rtm::vector_neg_mul_sub(xxxx0, alpha, xxxx0));
			interp_yyyy = rtm::vector_mul_add(yyyy1, alpha, rtm::vector_neg_mul_sub(yyyy0, alpha, yyyy0));
			interp_zzzz = rtm::vector_mul_add(zzzz1, alpha, rtm::vector_neg_mul_sub(zzzz0, alpha, zzzz0));
			interp_wwww = rtm::vector_mul_add(wwww1, alpha, rtm::vector_neg_mul_sub(wwww0, alpha, wwww0));
		}

		// About 9 cycles with AVX on Skylake
		// Force inline this function, we only use it to keep the code readable
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL quat_normalize4(rtm::vector4f& xxxx, rtm::vector4f& yyyy, rtm::vector4f& zzzz, rtm::vector4f& wwww)
		{
			const rtm::vector4f xxxx_squared = rtm::vector_mul(xxxx, xxxx);
			const rtm::vector4f yyyy_squared = rtm::vector_mul(yyyy, yyyy);
			const rtm::vector4f zzzz_squared = rtm::vector_mul(zzzz, zzzz);
			const rtm::vector4f wwww_squared = rtm::vector_mul(wwww, wwww);

			const rtm::vector4f dot4 = rtm::vector_add(rtm::vector_add(rtm::vector_add(xxxx_squared, yyyy_squared), zzzz_squared), wwww_squared);

			const rtm::vector4f len4 = rtm::vector_sqrt(dot4);
			const rtm::vector4f inv_len4 = rtm::vector_div(rtm::vector_set(1.0F), len4);

			xxxx = rtm::vector_mul(xxxx, inv_len4);
			yyyy = rtm::vector_mul(yyyy, inv_len4);
			zzzz = rtm::vector_mul(zzzz, inv_len4);
			wwww = rtm::vector_mul(wwww, inv_len4);
		}

		template<class decompression_settings_type>
		inline ACL_DISABLE_SECURITY_COOKIE_CHECK range_reduction_masks_t RTM_SIMD_CALL unpack_animated_quat(const persistent_transform_decompression_context_v0& decomp_context,
			rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack, segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

			uint32_t segment_range_ignore_mask = 0;
			uint32_t clip_range_ignore_mask = 0;

			// Current format is like this:
			// Packed in groups of 4 of the same sub-track type (e.g rot, rot, rot, rot)
			// Per track metadata (1 byte per sub-track entry, 4 bytes per group) comes first
			// Segment range data (6 bytes per sub-track entry, 24 bytes per group) comes second
			// When using variable bit rates with multiple segments, both are used and touched but with raw data (drop w or full) we need neither
			// If we have a single segment, there is no segment range data (no constant bit rate either) but we still have per sub-track metadata
			// Each sub-track thus needs 7 bytes, and we need 28 bytes per group. Two groups fit per cache line: 56 bytes.
			// Packing them together reduces the amount of prefetching we need to do from 2 to 1 cache line still with plenty of work to hide the latency
			// It also avoids the need to track our 'segment range data' pointer since it is per track + 4 bytes when present
			//
			// If we have segment range data, we should unpack it first and swizzle our constant samples first, store them in scratch
			// For now, ignore which lanes are required in our SOA form, store our 4x constant samples (3x vec4, might be garbage if not really constant)
			// and our range data (3x vec4 min, 3x vec4 extent)
			// If both segments we use are the same, we can re-use our values and avoid unpacking the second segment (most common case!)
			// This will also reduce the amount of redundant prefetching we do
			// Once that is done, we can move the segment range remapping outside this function into the caller and perform it with AVX with the W reconstruction

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			// TODO:
			// For SIMD, can we load constant samples and write them to scratch? Afterwards its the same as packed on 16 bits
			// We get 4 short branches (test, cmp, 6x loads, 3x ORs, 3x writes) followed by a common code path for all 4 samples
			// Similarly, the raw 32 bit unpacking can be done as variable unpacking, we'll do a bit more work with 64 bit loads instead of 32 bit
			// but we can avoid the loop and branches for better pipelining
			// We can build a mask of which sample was raw and doesn't need conversion from int->float
			// This still forces us to load our 4 samples in AOS form one by one and to swizzle into SOA
			//
			// Currently, unpacking a constant sample takes: 6 loads, 3 shifts, 3 OR, 3 SIMD MOV/SET, 1 cvt, 1 fmul = 17 instructions
			// Unpacking a raw sample takes: 1 shift, 1 AND, 3 loads, 3 byte swaps, 6 shifts, 3 SIMD MOV/SET, 1 cycle cast = 18 instructions
			// Unpacking a variable sample takes: 1 sub, 2 loads, 3 shifts, 3 loads, 3 byte swaps, 3 AND, 3 sub, 3 shifts, 2 add, 3 SIMD MOV/SET, 1 and, 1 cvt, 1 fmul = 29 instructions
			// The final swizzle is 5-6 instructions with SSE (4 with NEON)
			//
			// To be able to unpack directly in SOA form, we need to rework things
			// Instead of broadcasting the mask/inv_max_value across all SIMD lanes, we need to build it
			// from our 4 bit rates (4 instructions with SSE each)
			// The reads are re-ordered but otherwise remain the same
			// This simplifies shifting. In AOS each component shift is different because it depends on the bit offset.
			// In SOA form, the byte and shift offsets are much more predictable

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
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);

			// Update our pointers
			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
				// Prefetch 4 samples ahead in all levels of the CPU cache
				ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 63);

				// Skip our used metadata data, all groups are padded to 4 elements
				segment_sampling_context.format_per_track_data = format_per_track_data + 4;
			}

			segment_sampling_context.animated_track_data_bit_offset = animated_track_data_bit_offset;

			// Swizzle our samples into SOA form
			rtm::vector4f sample_xxxx;
			rtm::vector4f sample_yyyy;
			rtm::vector4f sample_zzzz;
			rtm::vector4f sample_wwww;
			RTM_MATRIXF_TRANSPOSE_4X4(output_scratch[0], output_scratch[1], output_scratch[2], output_scratch[3], sample_xxxx, sample_yyyy, sample_zzzz, sample_wwww);

			// Output our W components right away, either we do not need them or they are good to go (full precision)
			output_scratch[3] = sample_wwww;

			range_reduction_masks_t range_reduction_masks;	// function's return value

			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
#if defined(RTM_SSE2_INTRINSICS)
				const __m128i ignore_masks_v8 = _mm_set_epi32(0, 0, clip_range_ignore_mask, segment_range_ignore_mask);
				range_reduction_masks = _mm_unpacklo_epi8(ignore_masks_v8, ignore_masks_v8);
#elif defined(RTM_NEON_INTRINSICS)
				const int8x8_t ignore_masks_v8 = vcreate_s8((uint64_t(clip_range_ignore_mask) << 32) | segment_range_ignore_mask);
				range_reduction_masks = vmovl_s8(ignore_masks_v8);
#else
				range_reduction_masks = uint64_t(clip_range_ignore_mask) << 32) | segment_range_ignore_mask;
#endif

				// Skip our used segment range data, all groups are padded to 4 elements
				segment_range_data += 6 * 4;

				// Update our ptr
				segment_sampling_context.segment_range_data = segment_range_data;
			}
			else
			{
#if defined(RTM_SSE2_INTRINSICS)
				range_reduction_masks = _mm_setzero_si128();
#elif defined(RTM_NEON_INTRINSICS)
				range_reduction_masks = vcreate_s16(0ULL);
#else
				range_reduction_masks = 0ULL;
#endif
			}

			output_scratch[0] = sample_xxxx;
			output_scratch[1] = sample_yyyy;
			output_scratch[2] = sample_zzzz;

			return range_reduction_masks;
		}

		template<class decompression_settings_type>
		inline ACL_DISABLE_SECURITY_COOKIE_CHECK range_reduction_masks_t RTM_SIMD_CALL unpack_animated_quat2(const persistent_transform_decompression_context_v0& decomp_context,
			const segment_animated_scratch_v0& segment_scratch, uint32_t scratch_offset,
			rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack, segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

			uint32_t segment_range_ignore_mask = 0;
			uint32_t clip_range_ignore_mask = 0;

			// Current format is like this:
			// Packed in groups of 4 of the same sub-track type (e.g rot, rot, rot, rot)
			// Per track metadata (1 byte per sub-track entry, 4 bytes per group) comes first
			// Segment range data (6 bytes per sub-track entry, 24 bytes per group) comes second
			// When using variable bit rates with multiple segments, both are used and touched but with raw data (drop w or full) we need neither
			// If we have a single segment, there is no segment range data (no constant bit rate either) but we still have per sub-track metadata
			// Each sub-track thus needs 7 bytes, and we need 28 bytes per group. Two groups fit per cache line: 56 bytes.
			// Packing them together reduces the amount of prefetching we need to do from 2 to 1 cache line still with plenty of work to hide the latency
			// It also avoids the need to track our 'segment range data' pointer since it is per track + 4 bytes when present
			//
			// If we have segment range data, we should unpack it first and swizzle our constant samples first, store them in scratch
			// For now, ignore which lanes are required in our SOA form, store our 4x constant samples (3x vec4, might be garbage if not really constant)
			// and our range data (3x vec4 min, 3x vec4 extent)
			// If both segments we use are the same, we can re-use our values and avoid unpacking the second segment (most common case!)
			// This will also reduce the amount of redundant prefetching we do
			// Once that is done, we can move the segment range remapping outside this function into the caller and perform it with AVX with the W reconstruction

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			// TODO:
			// For SIMD, can we load constant samples and write them to scratch? Afterwards its the same as packed on 16 bits
			// We get 4 short branches (test, cmp, 6x loads, 3x ORs, 3x writes) followed by a common code path for all 4 samples
			// Similarly, the raw 32 bit unpacking can be done as variable unpacking, we'll do a bit more work with 64 bit loads instead of 32 bit
			// but we can avoid the loop and branches for better pipelining
			// We can build a mask of which sample was raw and doesn't need conversion from int->float
			// This still forces us to load our 4 samples in AOS form one by one and to swizzle into SOA
			//
			// Currently, unpacking a constant sample takes: 6 loads, 3 shifts, 3 OR, 3 SIMD MOV/SET, 1 cvt, 1 fmul = 17 instructions
			// Unpacking a raw sample takes: 1 shift, 1 AND, 3 loads, 3 byte swaps, 6 shifts, 3 SIMD MOV/SET, 1 cycle cast = 18 instructions
			// Unpacking a variable sample takes: 1 sub, 2 loads, 3 shifts, 3 loads, 3 byte swaps, 3 AND, 3 sub, 3 shifts, 2 add, 3 SIMD MOV/SET, 1 and, 1 cvt, 1 fmul = 29 instructions
			// The final swizzle is 5-6 instructions with SSE (4 with NEON)
			//
			// To be able to unpack directly in SOA form, we need to rework things
			// Instead of broadcasting the mask/inv_max_value across all SIMD lanes, we need to build it
			// from our 4 bit rates (4 instructions with SSE each)
			// The reads are re-ordered but otherwise remain the same
			// This simplifies shifting. In AOS each component shift is different because it depends on the bit offset.
			// In SOA form, the byte and shift offsets are much more predictable

#if 0
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
#else
			rtm::vector4f sample_xxxx;
			rtm::vector4f sample_yyyy;
			rtm::vector4f sample_zzzz;
			rtm::vector4f sample_wwww;

			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
				const uint8_t* constant_sample_data = &segment_scratch.constant_sample_data[scratch_offset * 32];

				uint32_t num_bits_at_bit_rate0 = format_per_track_data[0];
				uint32_t num_bits_at_bit_rate1 = format_per_track_data[1];
				uint32_t num_bits_at_bit_rate2 = format_per_track_data[2];
				uint32_t num_bits_at_bit_rate3 = format_per_track_data[3];

				uint32_t animated_bit_offset = animated_track_data_bit_offset;

				const uint8_t* sample_data_ptr0 = num_bits_at_bit_rate0 == 0 ? constant_sample_data : animated_track_data;
				const uint32_t sample_bit_offset0 = num_bits_at_bit_rate0 == 0 ? 0 : animated_bit_offset;
				const uint32_t sample_segment_range_ignore_mask0 = (num_bits_at_bit_rate0 == 0) | (num_bits_at_bit_rate0 == 32) ? 0x000000FF : 0x00000000;
				const uint32_t sample_clip_range_ignore_mask0 = num_bits_at_bit_rate0 == 32 ? 0x000000FF : 0x00000000;
				animated_bit_offset += num_bits_at_bit_rate0 == 0 ? 0 : (num_bits_at_bit_rate0 * 3);
				num_bits_at_bit_rate0 = num_bits_at_bit_rate0 == 0 ? 16 : num_bits_at_bit_rate0;

				const uint8_t* sample_data_ptr1 = num_bits_at_bit_rate1 == 0 ? constant_sample_data : animated_track_data;
				const uint32_t sample_bit_offset1 = num_bits_at_bit_rate1 == 0 ? 48 : animated_bit_offset;
				const uint32_t sample_segment_range_ignore_mask1 = (num_bits_at_bit_rate1 == 0) | (num_bits_at_bit_rate1 == 32) ? 0x0000FF00 : 0x00000000;
				const uint32_t sample_clip_range_ignore_mask1 = num_bits_at_bit_rate1 == 32 ? 0x0000FF00 : 0x00000000;
				animated_bit_offset += num_bits_at_bit_rate1 == 0 ? 0 : (num_bits_at_bit_rate1 * 3);
				num_bits_at_bit_rate1 = num_bits_at_bit_rate1 == 0 ? 16 : num_bits_at_bit_rate1;

				const uint8_t* sample_data_ptr2 = num_bits_at_bit_rate2 == 0 ? constant_sample_data : animated_track_data;
				const uint32_t sample_bit_offset2 = num_bits_at_bit_rate2 == 0 ? 96 : animated_bit_offset;
				const uint32_t sample_segment_range_ignore_mask2 = (num_bits_at_bit_rate2 == 0) | (num_bits_at_bit_rate2 == 32) ? 0x00FF0000 : 0x00000000;
				const uint32_t sample_clip_range_ignore_mask2 = num_bits_at_bit_rate2 == 32 ? 0x00FF0000 : 0x00000000;
				animated_bit_offset += num_bits_at_bit_rate2 == 0 ? 0 : (num_bits_at_bit_rate2 * 3);
				num_bits_at_bit_rate2 = num_bits_at_bit_rate2 == 0 ? 16 : num_bits_at_bit_rate2;

				const uint8_t* sample_data_ptr3 = num_bits_at_bit_rate3 == 0 ? constant_sample_data : animated_track_data;
				const uint32_t sample_bit_offset3 = num_bits_at_bit_rate3 == 0 ? 144 : animated_bit_offset;
				const uint32_t sample_segment_range_ignore_mask3 = (num_bits_at_bit_rate3 == 0) | (num_bits_at_bit_rate3 == 32) ? 0xFF000000 : 0x00000000;
				const uint32_t sample_clip_range_ignore_mask3 = num_bits_at_bit_rate3 == 32 ? 0xFF000000 : 0x00000000;
				animated_bit_offset += num_bits_at_bit_rate3 == 0 ? 0 : (num_bits_at_bit_rate3 * 3);
				num_bits_at_bit_rate3 = num_bits_at_bit_rate3 == 0 ? 16 : num_bits_at_bit_rate3;

				// Build up our range mapping masks
				// Masks are used in little endian format so the first sample is in the LSB end
				segment_range_ignore_mask = sample_segment_range_ignore_mask0 | sample_segment_range_ignore_mask1 | sample_segment_range_ignore_mask2 | sample_segment_range_ignore_mask3;
				clip_range_ignore_mask = sample_clip_range_ignore_mask0 | sample_clip_range_ignore_mask1 | sample_clip_range_ignore_mask2 | sample_clip_range_ignore_mask3;

				// Update our final offset
				animated_track_data_bit_offset = animated_bit_offset;

				const uint32_t bit_shift0 = 64 - num_bits_at_bit_rate0;
				const uint32_t bit_shift1 = 64 - num_bits_at_bit_rate1;
				const uint32_t bit_shift2 = 64 - num_bits_at_bit_rate2;
				const uint32_t bit_shift3 = 64 - num_bits_at_bit_rate3;

				const uint32_t x0_byte_offset = (sample_bit_offset0 + 0) / 8;
				const uint32_t y0_byte_offset = (sample_bit_offset0 + num_bits_at_bit_rate0) / 8;
				const uint32_t z0_byte_offset = (sample_bit_offset0 + num_bits_at_bit_rate0 + num_bits_at_bit_rate0) / 8;

				const uint32_t x0_bit_offset = (sample_bit_offset0 + 0) % 8;
				const uint32_t y0_bit_offset = (sample_bit_offset0 + num_bits_at_bit_rate0) % 8;
				const uint32_t z0_bit_offset = (sample_bit_offset0 + num_bits_at_bit_rate0 + num_bits_at_bit_rate0) % 8;

				uint64_t x0_u64 = unaligned_load<uint64_t>(sample_data_ptr0 + x0_byte_offset);
				x0_u64 = byte_swap(x0_u64);
				x0_u64 <<= x0_bit_offset;
				x0_u64 >>= bit_shift0;

				uint64_t y0_u64 = unaligned_load<uint64_t>(sample_data_ptr0 + y0_byte_offset);
				y0_u64 = byte_swap(y0_u64);
				y0_u64 <<= y0_bit_offset;
				y0_u64 >>= bit_shift0;

				uint64_t z0_u64 = unaligned_load<uint64_t>(sample_data_ptr0 + z0_byte_offset);
				z0_u64 = byte_swap(z0_u64);
				z0_u64 <<= z0_bit_offset;
				z0_u64 >>= bit_shift0;

				const uint32_t x0 = uint32_t(x0_u64);
				const uint32_t y0 = uint32_t(y0_u64);
				const uint32_t z0 = uint32_t(z0_u64);

				const uint32_t x1_byte_offset = (sample_bit_offset1 + 0) / 8;
				const uint32_t y1_byte_offset = (sample_bit_offset1 + num_bits_at_bit_rate1) / 8;
				const uint32_t z1_byte_offset = (sample_bit_offset1 + num_bits_at_bit_rate1 + num_bits_at_bit_rate1) / 8;

				const uint32_t x1_bit_offset = (sample_bit_offset1 + 0) % 8;
				const uint32_t y1_bit_offset = (sample_bit_offset1 + num_bits_at_bit_rate1) % 8;
				const uint32_t z1_bit_offset = (sample_bit_offset1 + num_bits_at_bit_rate1 + num_bits_at_bit_rate1) % 8;

				uint64_t x1_u64 = unaligned_load<uint64_t>(sample_data_ptr1 + x1_byte_offset);
				x1_u64 = byte_swap(x1_u64);
				x1_u64 <<= x1_bit_offset;
				x1_u64 >>= bit_shift1;

				uint64_t y1_u64 = unaligned_load<uint64_t>(sample_data_ptr1 + y1_byte_offset);
				y1_u64 = byte_swap(y1_u64);
				y1_u64 <<= y1_bit_offset;
				y1_u64 >>= bit_shift1;

				uint64_t z1_u64 = unaligned_load<uint64_t>(sample_data_ptr1 + z1_byte_offset);
				z1_u64 = byte_swap(z1_u64);
				z1_u64 <<= z1_bit_offset;
				z1_u64 >>= bit_shift1;

				const uint32_t x1 = uint32_t(x1_u64);
				const uint32_t y1 = uint32_t(y1_u64);
				const uint32_t z1 = uint32_t(z1_u64);

				const uint32_t x2_byte_offset = (sample_bit_offset2 + 0) / 8;
				const uint32_t y2_byte_offset = (sample_bit_offset2 + num_bits_at_bit_rate2) / 8;
				const uint32_t z2_byte_offset = (sample_bit_offset2 + num_bits_at_bit_rate2 + num_bits_at_bit_rate2) / 8;

				const uint32_t x2_bit_offset = (sample_bit_offset2 + 0) % 8;
				const uint32_t y2_bit_offset = (sample_bit_offset2 + num_bits_at_bit_rate2) % 8;
				const uint32_t z2_bit_offset = (sample_bit_offset2 + num_bits_at_bit_rate2 + num_bits_at_bit_rate2) % 8;

				uint64_t x2_u64 = unaligned_load<uint64_t>(sample_data_ptr2 + x2_byte_offset);
				x2_u64 = byte_swap(x2_u64);
				x2_u64 <<= x2_bit_offset;
				x2_u64 >>= bit_shift2;

				uint64_t y2_u64 = unaligned_load<uint64_t>(sample_data_ptr2 + y2_byte_offset);
				y2_u64 = byte_swap(y2_u64);
				y2_u64 <<= y2_bit_offset;
				y2_u64 >>= bit_shift2;

				uint64_t z2_u64 = unaligned_load<uint64_t>(sample_data_ptr2 + z2_byte_offset);
				z2_u64 = byte_swap(z2_u64);
				z2_u64 <<= z2_bit_offset;
				z2_u64 >>= bit_shift2;

				const uint32_t x2 = uint32_t(x2_u64);
				const uint32_t y2 = uint32_t(y2_u64);
				const uint32_t z2 = uint32_t(z2_u64);

				const uint32_t x3_byte_offset = (sample_bit_offset3 + 0) / 8;
				const uint32_t y3_byte_offset = (sample_bit_offset3 + num_bits_at_bit_rate3) / 8;
				const uint32_t z3_byte_offset = (sample_bit_offset3 + num_bits_at_bit_rate3 + num_bits_at_bit_rate3) / 8;

				const uint32_t x3_bit_offset = (sample_bit_offset3 + 0) % 8;
				const uint32_t y3_bit_offset = (sample_bit_offset3 + num_bits_at_bit_rate3) % 8;
				const uint32_t z3_bit_offset = (sample_bit_offset3 + num_bits_at_bit_rate3 + num_bits_at_bit_rate3) % 8;

				uint64_t x3_u64 = unaligned_load<uint64_t>(sample_data_ptr3 + x3_byte_offset);
				x3_u64 = byte_swap(x3_u64);
				x3_u64 <<= x3_bit_offset;
				x3_u64 >>= bit_shift3;

				uint64_t y3_u64 = unaligned_load<uint64_t>(sample_data_ptr3 + y3_byte_offset);
				y3_u64 = byte_swap(y3_u64);
				y3_u64 <<= y3_bit_offset;
				y3_u64 >>= bit_shift3;

				uint64_t z3_u64 = unaligned_load<uint64_t>(sample_data_ptr3 + z3_byte_offset);
				z3_u64 = byte_swap(z3_u64);
				z3_u64 <<= z3_bit_offset;
				z3_u64 >>= bit_shift3;

				const uint32_t x3 = uint32_t(x3_u64);
				const uint32_t y3 = uint32_t(y3_u64);
				const uint32_t z3 = uint32_t(z3_u64);

				// Build our mask to strip the leading bits we don't need
				//const uint32_t mask0 = num_bits_at_bit_rate0 == 32 ? ~0U : ((1 << num_bits_at_bit_rate0) - 1);
				//const uint32_t mask1 = num_bits_at_bit_rate1 == 32 ? ~0U : ((1 << num_bits_at_bit_rate1) - 1);
				//const uint32_t mask2 = num_bits_at_bit_rate2 == 32 ? ~0U : ((1 << num_bits_at_bit_rate2) - 1);
				//const uint32_t mask3 = num_bits_at_bit_rate3 == 32 ? ~0U : ((1 << num_bits_at_bit_rate3) - 1);

				//const __m128i mask = _mm_set_epi32(mask3, mask2, mask1, mask0);

				__m128i sample_xxxx_u32 = _mm_set_epi32(x3, x2, x1, x0);
				__m128i sample_yyyy_u32 = _mm_set_epi32(y3, y2, y1, y0);
				__m128i sample_zzzz_u32 = _mm_set_epi32(z3, z2, z1, z0);

				//sample_xxxx = _mm_and_si128(sample_xxxx, mask);

				// Convert our integers or keep our original value depending on the bit rate
				const rtm::mask4f is_raw_bit_rate = rtm::mask_set(num_bits_at_bit_rate0 == 32, num_bits_at_bit_rate1 == 32, num_bits_at_bit_rate2 == 32, num_bits_at_bit_rate3 == 32);
				const __m128 inv_max_value = _mm_div_ps(_mm_set_ps1(1.0F), _mm_set_ps(float((1 << num_bits_at_bit_rate3) - 1), float((1 << num_bits_at_bit_rate2) - 1), float((1 << num_bits_at_bit_rate1) - 1), float((1 << num_bits_at_bit_rate0) - 1)));

				sample_xxxx = rtm::vector_select(is_raw_bit_rate, _mm_castsi128_ps(sample_xxxx_u32), _mm_mul_ps(_mm_cvtepi32_ps(sample_xxxx_u32), inv_max_value));
				sample_yyyy = rtm::vector_select(is_raw_bit_rate, _mm_castsi128_ps(sample_yyyy_u32), _mm_mul_ps(_mm_cvtepi32_ps(sample_yyyy_u32), inv_max_value));
				sample_zzzz = rtm::vector_select(is_raw_bit_rate, _mm_castsi128_ps(sample_zzzz_u32), _mm_mul_ps(_mm_cvtepi32_ps(sample_zzzz_u32), inv_max_value));
				sample_wwww = inv_max_value;	// Set some garbage, not needed
			}
			else
			{
				// Our decompressed rotation as a vector4
				rtm::vector4f rotation_as_vec = rtm::vector_zero();
				for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
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

					output_scratch[unpack_index] = rotation_as_vec;
				}

				RTM_MATRIXF_TRANSPOSE_4X4(output_scratch[0], output_scratch[1], output_scratch[2], output_scratch[3], sample_xxxx, sample_yyyy, sample_zzzz, sample_wwww);
			}
#endif

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
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);

			// Update our pointers
			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
				// Prefetch 4 samples ahead in all levels of the CPU cache
				ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 63);

				// Skip our used metadata data, all groups are padded to 4 elements
				segment_sampling_context.format_per_track_data = format_per_track_data + 4;
			}

			segment_sampling_context.animated_track_data_bit_offset = animated_track_data_bit_offset;

			// Output our W components right away, either we do not need them or they are good to go (full precision)
			output_scratch[3] = sample_wwww;

			range_reduction_masks_t range_reduction_masks;	// function's return value

			if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
			{
#if defined(RTM_SSE2_INTRINSICS)
				const __m128i ignore_masks_v8 = _mm_set_epi32(0, 0, clip_range_ignore_mask, segment_range_ignore_mask);
				range_reduction_masks = _mm_unpacklo_epi8(ignore_masks_v8, ignore_masks_v8);
#elif defined(RTM_NEON_INTRINSICS)
				const int8x8_t ignore_masks_v8 = vcreate_s8((uint64_t(clip_range_ignore_mask) << 32) | segment_range_ignore_mask);
				range_reduction_masks = vmovl_s8(ignore_masks_v8);
#else
				range_reduction_masks = uint64_t(clip_range_ignore_mask) << 32) | segment_range_ignore_mask;
#endif

				// Skip our used segment range data, all groups are padded to 4 elements
				segment_range_data += 6 * 4;

				// Update our ptr
				segment_sampling_context.segment_range_data = segment_range_data;
			}
			else
			{
#if defined(RTM_SSE2_INTRINSICS)
				range_reduction_masks = _mm_setzero_si128();
#elif defined(RTM_NEON_INTRINSICS)
				range_reduction_masks = vcreate_s16(0ULL);
#else
				range_reduction_masks = 0ULL;
#endif
			}

			output_scratch[0] = sample_xxxx;
			output_scratch[1] = sample_yyyy;
			output_scratch[2] = sample_zzzz;

			return range_reduction_masks;
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

			bool uses_single_segment;	// TODO: Store in decomp context?

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
				clip_sampling_context.clip_range_data = decomp_context.clip_range_data.add_to(decomp_context.tracks);

				segment_sampling_context[0].format_per_track_data = decomp_context.format_per_track_data[0];
				segment_sampling_context[0].segment_range_data = decomp_context.segment_range_data[0];
				segment_sampling_context[0].animated_track_data = decomp_context.animated_track_data[0];
				segment_sampling_context[0].animated_track_data_bit_offset = decomp_context.key_frame_bit_offsets[0];

				segment_sampling_context[1].format_per_track_data = decomp_context.format_per_track_data[1];
				segment_sampling_context[1].segment_range_data = decomp_context.segment_range_data[1];
				segment_sampling_context[1].animated_track_data = decomp_context.animated_track_data[1];
				segment_sampling_context[1].animated_track_data_bit_offset = decomp_context.key_frame_bit_offsets[1];

				uses_single_segment = decomp_context.format_per_track_data[0] == decomp_context.format_per_track_data[1];

				const transform_tracks_header& transform_header = get_transform_tracks_header(*decomp_context.tracks);

				rotations.num_left_to_unpack = transform_header.num_animated_rotation_sub_tracks;
				translations.num_left_to_unpack = transform_header.num_animated_translation_sub_tracks;
				scales.num_left_to_unpack = transform_header.num_animated_scale_sub_tracks;
			}

			// Cache miss is ~150 cycles so ideally we want to prefetch 120-150 cycles ahead to hide the cost
			// We have to be careful how many prefetches are in flight to avoid saturating and stalling
			// Modern Intel can support about ~10 cache misses but modern ARM can support much more at 20-25, aim for 8
			// since we also cache miss on other stuff harder to control (code, constants, etc)
			// Do we do enough work with rotations to prefetch the clip range data before we unpack the segment range data?
			// We do enough for for sure to prefetch the next group but if we wish to reorder our data to keep single track decompression
			// fast, how do we prefetch in the right order? Just look it up since we'll stall on memory anyway (probably)?

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

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

				segment_animated_scratch_v0 segment_scratch;

				// We start by unpacking our segment range data into our scratch memory
				// We often only use a single segment to interpolate, we can avoid redundant work
				if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
				{
					if (decomp_context.has_segments)
					{
						unpack_segment_range_data(segment_sampling_context[0].segment_range_data, 0, segment_scratch);

						// We are interpolating between two segments (rare)
						if (!uses_single_segment)
							unpack_segment_range_data(segment_sampling_context[1].segment_range_data, 1, segment_scratch);
					}
				}

#if 0
				const segment_animated_sampling_context_v0 ref_s0 = segment_sampling_context[0];
				const segment_animated_sampling_context_v0 ref_s1 = segment_sampling_context[1];
				segment_animated_sampling_context_v0 s0 = segment_sampling_context[0];
				segment_animated_sampling_context_v0 s1 = segment_sampling_context[1];
				rtm::vector4f tmp0_[4]; (void)tmp0_;
				rtm::vector4f tmp1_[4]; (void)tmp1_;

				segment_sampling_context[0] = ref_s0;
				segment_sampling_context[1] = ref_s1;
				s0 = ref_s0;
				s1 = ref_s1;

				const range_reduction_masks_t range_reduction_masks0 = unpack_animated_quat<decompression_settings_type>(decomp_context, scratch0, num_to_unpack, segment_sampling_context[0]);
				const range_reduction_masks_t range_reduction_masks1 = unpack_animated_quat<decompression_settings_type>(decomp_context, scratch1, num_to_unpack, segment_sampling_context[1]);
				const range_reduction_masks_t range_reduction_masks0_ = unpack_animated_quat2<decompression_settings_type>(decomp_context, segment_scratch, 0, tmp0_, num_to_unpack, s0);
				const range_reduction_masks_t range_reduction_masks1_ = unpack_animated_quat2<decompression_settings_type>(decomp_context, segment_scratch, uint32_t(!uses_single_segment), tmp1_, num_to_unpack, s1);

				//ACL_ASSERT(std::memcmp(reinterpret_cast<uint8_t*>(&range_reduction_masks0) + (4 - num_to_unpack) * 2, &range_reduction_masks0_, sizeof(range_reduction_masks_t)) == 0, "??");
				//ACL_ASSERT(std::memcmp(&range_reduction_masks1, &range_reduction_masks1_, sizeof(range_reduction_masks_t)) == 0, "??");

				switch (num_to_unpack)
				{
				case 1:
					ACL_ASSERT(float(rtm::vector_get_x(scratch0[0])) == float(rtm::vector_get_x(tmp0_[0])), "??");
					ACL_ASSERT(float(rtm::vector_get_x(scratch0[1])) == float(rtm::vector_get_x(tmp0_[1])), "??");
					ACL_ASSERT(float(rtm::vector_get_x(scratch0[2])) == float(rtm::vector_get_x(tmp0_[2])), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(float(rtm::vector_get_x(scratch0[3])) == float(rtm::vector_get_x(tmp0_[3])), "??");

					ACL_ASSERT(float(rtm::vector_get_x(scratch1[0])) == float(rtm::vector_get_x(tmp1_[0])), "??");
					ACL_ASSERT(float(rtm::vector_get_x(scratch1[1])) == float(rtm::vector_get_x(tmp1_[1])), "??");
					ACL_ASSERT(float(rtm::vector_get_x(scratch1[2])) == float(rtm::vector_get_x(tmp1_[2])), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(float(rtm::vector_get_x(scratch1[3])) == float(rtm::vector_get_x(tmp1_[3])), "??");
					break;
				case 2:
					ACL_ASSERT(rtm::vector_all_near_equal2(scratch0[0], tmp0_[0]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal2(scratch0[1], tmp0_[1]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal2(scratch0[2], tmp0_[2]), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(rtm::vector_all_near_equal2(scratch0[3], tmp0_[3]), "??");

					ACL_ASSERT(rtm::vector_all_near_equal2(scratch1[0], tmp1_[0]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal2(scratch1[1], tmp1_[1]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal2(scratch1[2], tmp1_[2]), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(rtm::vector_all_near_equal2(scratch1[3], tmp1_[3]), "??");
					break;
				case 3:
					ACL_ASSERT(rtm::vector_all_near_equal3(scratch0[0], tmp0_[0]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal3(scratch0[1], tmp0_[1]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal3(scratch0[2], tmp0_[2]), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(rtm::vector_all_near_equal3(scratch0[3], tmp0_[3]), "??");

					ACL_ASSERT(rtm::vector_all_near_equal3(scratch1[0], tmp1_[0]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal3(scratch1[1], tmp1_[1]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal3(scratch1[2], tmp1_[2]), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(rtm::vector_all_near_equal3(scratch1[3], tmp1_[3]), "??");
					break;
				case 4:
					ACL_ASSERT(rtm::vector_all_near_equal(scratch0[0], tmp0_[0]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal(scratch0[1], tmp0_[1]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal(scratch0[2], tmp0_[2]), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(rtm::vector_all_near_equal(scratch0[3], tmp0_[3]), "??");

					ACL_ASSERT(rtm::vector_all_near_equal(scratch1[0], tmp1_[0]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal(scratch1[1], tmp1_[1]), "??");
					ACL_ASSERT(rtm::vector_all_near_equal(scratch1[2], tmp1_[2]), "??");
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						ACL_ASSERT(rtm::vector_all_near_equal(scratch1[3], tmp1_[3]), "??");
					break;
				}
#else
				const range_reduction_masks_t range_reduction_masks0 = unpack_animated_quat2<decompression_settings_type>(decomp_context, segment_scratch, 0, scratch0, num_to_unpack, segment_sampling_context[0]);
				const range_reduction_masks_t range_reduction_masks1 = unpack_animated_quat2<decompression_settings_type>(decomp_context, segment_scratch, uint32_t(!uses_single_segment), scratch1, num_to_unpack, segment_sampling_context[1]);
#endif

				rtm::vector4f scratch0_xxxx = scratch0[0];
				rtm::vector4f scratch0_yyyy = scratch0[1];
				rtm::vector4f scratch0_zzzz = scratch0[2];
				rtm::vector4f scratch0_wwww;

				rtm::vector4f scratch1_xxxx = scratch1[0];
				rtm::vector4f scratch1_yyyy = scratch1[1];
				rtm::vector4f scratch1_zzzz = scratch1[2];
				rtm::vector4f scratch1_wwww;

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
				__m256 scratch_xxxx0_xxxx1 = _mm256_set_m128(scratch1_xxxx, scratch0_xxxx);
				__m256 scratch_yyyy0_yyyy1 = _mm256_set_m128(scratch1_yyyy, scratch0_yyyy);
				__m256 scratch_zzzz0_zzzz1 = _mm256_set_m128(scratch1_zzzz, scratch0_zzzz);
#endif

				// If we have a variable bit rate, we perform range reduction, skip the data we used
				if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
				{
					if (decomp_context.has_segments)
					{
#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
						remap_segment_range_data_avx8(segment_scratch, range_reduction_masks0, range_reduction_masks1, scratch_xxxx0_xxxx1, scratch_yyyy0_yyyy1, scratch_zzzz0_zzzz1);
#else
						remap_segment_range_data4(segment_scratch, 0, range_reduction_masks0, scratch0_xxxx, scratch0_yyyy, scratch0_zzzz);
						remap_segment_range_data4(segment_scratch, uint32_t(!uses_single_segment), range_reduction_masks1, scratch1_xxxx, scratch1_yyyy, scratch1_zzzz);
#endif
					}

					const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
					remap_clip_range_data_avx8(clip_range_data, num_to_unpack, range_reduction_masks0, range_reduction_masks1, scratch_xxxx0_xxxx1, scratch_yyyy0_yyyy1, scratch_zzzz0_zzzz1);
#else
					remap_clip_range_data4(clip_range_data, num_to_unpack, range_reduction_masks0, range_reduction_masks1, scratch0_xxxx, scratch0_yyyy, scratch0_zzzz, scratch1_xxxx, scratch1_yyyy, scratch1_zzzz);
#endif

					// Clip range data is 24-32 bytes per sub-track and as such we need to prefetch two cache lines ahead to process 4 sub-tracks
					ACL_IMPL_ANIMATED_PREFETCH(clip_range_data + 128);
					ACL_IMPL_ANIMATED_PREFETCH(clip_range_data + 192);

					// Skip our data
					clip_range_data += num_to_unpack * sizeof(rtm::float3f) * 2;
					clip_sampling_context.clip_range_data = clip_range_data;
				}

				// Reconstruct our quaternion W component in SOA
				if (rotation_format != rotation_format8::quatf_full || !decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
					const __m256 scratch_wwww0_wwww1 = quat_from_positive_w_avx8(scratch_xxxx0_xxxx1, scratch_yyyy0_yyyy1, scratch_zzzz0_zzzz1);

					// This is the last AVX step, unpack everything
					scratch0_xxxx = _mm256_extractf128_ps(scratch_xxxx0_xxxx1, 0);
					scratch1_xxxx = _mm256_extractf128_ps(scratch_xxxx0_xxxx1, 1);
					scratch0_yyyy = _mm256_extractf128_ps(scratch_yyyy0_yyyy1, 0);
					scratch1_yyyy = _mm256_extractf128_ps(scratch_yyyy0_yyyy1, 1);
					scratch0_zzzz = _mm256_extractf128_ps(scratch_zzzz0_zzzz1, 0);
					scratch1_zzzz = _mm256_extractf128_ps(scratch_zzzz0_zzzz1, 1);
					scratch0_wwww = _mm256_extractf128_ps(scratch_wwww0_wwww1, 0);
					scratch1_wwww = _mm256_extractf128_ps(scratch_wwww0_wwww1, 1);
#else
					scratch0_wwww = quat_from_positive_w4(scratch0_xxxx, scratch0_yyyy, scratch0_zzzz);
					scratch1_wwww = quat_from_positive_w4(scratch1_xxxx, scratch1_yyyy, scratch1_zzzz);
#endif
				}
				else
				{
					scratch0_wwww = scratch0[3];
					scratch1_wwww = scratch1[3];
				}

				// Interpolate linearly and store our rotations in SOA
				{
					// Interpolate our quaternions without normalizing just yet
					rtm::vector4f interp_xxxx;
					rtm::vector4f interp_yyyy;
					rtm::vector4f interp_zzzz;
					rtm::vector4f interp_wwww;
					quat_lerp4(scratch0_xxxx, scratch0_yyyy, scratch0_zzzz, scratch0_wwww,
						scratch1_xxxx, scratch1_yyyy, scratch1_zzzz, scratch1_wwww,
						decomp_context.interpolation_alpha,
						interp_xxxx, interp_yyyy, interp_zzzz, interp_wwww);

					// Due to the interpolation, the result might not be anywhere near normalized!
					// Make sure to normalize afterwards if we need to
					const bool normalize_rotations = decompression_settings_type::normalize_rotations();
					if (normalize_rotations)
						quat_normalize4(interp_xxxx, interp_yyyy, interp_zzzz, interp_wwww);

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
