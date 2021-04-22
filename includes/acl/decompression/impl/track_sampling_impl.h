#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/compiler_utils.h"
#include "acl/core/compressed_tracks.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		struct alignas(64) persistent_decompression_context
		{
			// Clip related data							//   offsets
			const compressed_tracks* tracks;				//   0 |   0

			uint32_t tracks_hash;							//   4 |   8

			float duration;									//   8 |  12

			// Seeking related data
			float interpolation_alpha;						//  12 |  16
			float sample_time;								//  16 |  20

			uint32_t key_frame_bit_offsets[2];				//  20 |  24	// Variable quantization

			uint8_t padding_tail[sizeof(void*) == 4 ? 36 : 32];

			//////////////////////////////////////////////////////////////////////////

			inline bool is_initialized() const { return tracks != nullptr; }
		};

		// Assumes the 'vector_data' is in big-endian order and is padded in order to load up to 16 bytes from it
		inline rtm::scalarf ACL_SIMD_CALL unpack_scalarf_96_unsafe(const uint8_t* vector_data, uint32_t bit_offset)
		{
#if defined(ACL_SSE2_INTRINSICS)
			const uint32_t byte_offset = bit_offset / 8;
			const uint32_t shift_offset = bit_offset % 8;
			uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
			vector_u64 = byte_swap(vector_u64);
			vector_u64 <<= shift_offset;
			vector_u64 >>= 32;

			const uint32_t x32 = uint32_t(vector_u64);

			return _mm_castsi128_ps(_mm_set1_epi32(x32));
#elif defined(ACL_NEON_INTRINSICS)
			const uint32_t byte_offset = bit_offset / 8;
			const uint32_t shift_offset = bit_offset % 8;
			uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
			vector_u64 = byte_swap(vector_u64);
			vector_u64 <<= shift_offset;
			vector_u64 >>= 32;

			const uint64_t x64 = vector_u64;

			const uint32x2_t xy = vcreate_u32(x64);
			return vget_lane_f32(vreinterpret_f32_u32(xy), 0);
#else
			const uint32_t byte_offset = bit_offset / 8;
			const uint32_t shift_offset = bit_offset % 8;
			uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset + 0);
			vector_u64 = byte_swap(vector_u64);
			vector_u64 <<= shift_offset;
			vector_u64 >>= 32;

			const uint64_t x64 = vector_u64;

			const float x = aligned_load<float>(&x64);

			return rtm::scalar_set(x);
#endif
		}

		// Assumes the 'vector_data' is in big-endian order and padded in order to load up to 16 bytes from it
		inline rtm::scalarf ACL_SIMD_CALL unpack_scalarf_uXX_unsafe(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
		{

#ifdef ACL_BIT_RATE

			ACL_ASSERT(num_bits <= 24, "This function does not support reading more than 24 bits per component");

#else

			ACL_ASSERT(num_bits <= 19, "This function does not support reading more than 19 bits per component");

#endif

			struct PackedTableEntry
			{
				explicit constexpr PackedTableEntry(uint8_t num_bits_)
					: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
					, mask((1 << num_bits_) - 1)
				{}

				float max_value;
				uint32_t mask;
			};

#ifdef ACL_BIT_RATE

			// TODO: We technically don't need the first entry, which could save a few bytes
			alignas(64) static constexpr PackedTableEntry k_packed_constants[25] =

#else

			// TODO: We technically don't need the first 3 entries, which could save a few bytes
			alignas(64) static constexpr PackedTableEntry k_packed_constants[20] =

#endif

			{
				PackedTableEntry(0), PackedTableEntry(1), PackedTableEntry(2), PackedTableEntry(3),
				PackedTableEntry(4), PackedTableEntry(5), PackedTableEntry(6), PackedTableEntry(7),
				PackedTableEntry(8), PackedTableEntry(9), PackedTableEntry(10), PackedTableEntry(11),
				PackedTableEntry(12), PackedTableEntry(13), PackedTableEntry(14), PackedTableEntry(15),
				PackedTableEntry(16), PackedTableEntry(17), PackedTableEntry(18), PackedTableEntry(19),

#ifdef ACL_BIT_RATE

				PackedTableEntry(20), PackedTableEntry(21), PackedTableEntry(22), PackedTableEntry(23), PackedTableEntry(24),

#endif

			};

#ifdef ACL_PACKING

			ACL_ASSERT(false, "Not tested yet");

#endif

#if defined(ACL_SSE2_INTRINSICS)
			const uint32_t bit_shift = 32 - num_bits;
			const uint32_t mask = k_packed_constants[num_bits].mask;
			const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants[num_bits].max_value);

			uint32_t byte_offset = bit_offset / 8;
			uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
			vector_u32 = byte_swap(vector_u32);
			const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

			const __m128 value = _mm_cvtsi32_ss(inv_max_value, x32 & mask);
			return _mm_mul_ss(value, inv_max_value);
#elif defined(ACL_NEON_INTRINSICS)
			const uint32_t bit_shift = 32 - num_bits;
			const uint32_t mask = k_packed_constants[num_bits].mask;
			const float inv_max_value = k_packed_constants[num_bits].max_value;

			uint32_t byte_offset = bit_offset / 8;
			uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
			vector_u32 = byte_swap(vector_u32);
			const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

			const int32_t value_u32 = x32 & mask;
			const float value_f32 = static_cast<float>(value_u32);
			return value_f32 * inv_max_value;
#else
			const uint32_t bit_shift = 32 - num_bits;
			const uint32_t mask = k_packed_constants[num_bits].mask;
			const float inv_max_value = k_packed_constants[num_bits].max_value;

			uint32_t byte_offset = bit_offset / 8;
			uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
			vector_u32 = byte_swap(vector_u32);
			const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

			return rtm::scalar_set(static_cast<float>(x32) * inv_max_value);
#endif
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
