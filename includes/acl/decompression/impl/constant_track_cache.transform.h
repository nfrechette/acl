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

#include "acl/version.h"
#include "acl/core/track_formats.h"
#include "acl/core/impl/bit_cast.impl.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/impl/track_cache.h"
#include "acl/decompression/impl/decompression_context.transform.h"
#include "acl/math/quat_packing.h"
#include "acl/math/quatf.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

#define ACL_IMPL_USE_CONSTANT_PREFETCH

// Controls which variant to use for rotation unpacking
// 0: ACL 2.1 (baseline)
// 1: Unrolled
// 2: Unrolled with 8-wide AVX
//#define ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT 1

// If not manually defined, pick the optimal one
#if !defined(ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT)
	#if defined(RTM_AVX_INTRINSICS) && !defined(ACL_NO_8_WIDE_AVX)
		// Faster on modern AVX hardware
		#define ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT 2
	#else
		// Faster everywhere else
		#define ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT 1
	#endif
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

// We only initialize some variables when we need them which prompts the compiler to complain
// The usage is perfectly safe and because this code is VERY hot and needs to be as fast as possible,
// we disable the warning to avoid zeroing out things we don't need
#if defined(RTM_COMPILER_GCC)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C26495: Variable '...' is uninitialized. Always initialize a member variable (type.6).
	// We explicitly control initialization
	#pragma warning(disable : 26495)
	// warning C26451: Arithmetic overflow: Using operator '*' on a 4 byte value and then casting the result to a 8 byte value. Cast the value to the wider type before calling operator '*' to avoid overflow (io.2).
	// We can't overflow because compressed clips cannot contain more than 4 GB worth of data
	#pragma warning(disable : 26451)
	// warning C6385: Reading invalid data from '...':  the readable size is '512' bytes, but '528' bytes may be read.
	// We properly handle overflow, this is a false positive
	#pragma warning(disable : 6385)
#endif

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

#if defined(ACL_IMPL_USE_CONSTANT_PREFETCH)
	#define ACL_IMPL_CONSTANT_PREFETCH(ptr) memory_prefetch(ptr)
#else
	#define ACL_IMPL_CONSTANT_PREFETCH(ptr) (void)(ptr)
#endif

	namespace acl_impl
	{
		struct constant_track_cache_read_cursor_v0
		{
			uint32_t cache_read_index = 0;

#if ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT == 2	// AVX 8-wide
			uint32_t read_swizzle_lookup_table = 0x75316420;
#endif
		};

		struct constant_track_cache_v0
		{
			// Our constant rotation samples are packed in groups of 4 samples (16 floats, 64 bytes) in AOS form when we have full precision
			// and with SOA form otherwise (12 floats, 48 bytes). To avoid stalling, we unpack 16 samples at a time and cache the result in AOS form.
			// This means we need 3-4 cache lines to unpack. If we cache miss, we'll be able to queue up as many uops as we can before we stall.
			// We can also queue up enough independent uops to avoid stalling on the square-root (6-21 cycles depending on the platform).
			// Constant rotation samples are fairly common, see here: https://nfrechette.github.io/2020/08/09/animation_data_numbers/
			// CMU has 64.41%, Paragon has 47.69%, and Fortnite has 62.84%.
			// Following these numbers, it is common for clips to have at least 10 constant rotation samples to unpack.

#if ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT == 2	// AVX 8-wide
			alignas(32) track_cache_quatf_v0<32, 1> rotations;
#else
			track_cache_quatf_v0<32, 1> rotations;
#endif

			// Points to our packed sub-track data
			const uint8_t*	constant_data_rotations;
			const uint8_t*	constant_data_translations;
			const uint8_t*	constant_data_scales;

			template<class decompression_settings_type>
			RTM_DISABLE_SECURITY_COOKIE_CHECK void initialize(const persistent_transform_decompression_context_v0& decomp_context)
			{
				const transform_tracks_header& transform_header = get_transform_tracks_header(*decomp_context.tracks);

				rotations.num_left_to_unpack = transform_header.num_constant_rotation_samples;

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				const rotation_format8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
				const uint32_t packed_rotation_size = get_packed_rotation_size(packed_format);
				const uint32_t packed_translation_size = get_packed_vector_size(vector_format8::vector3f_full);

				constant_data_rotations = transform_header.get_constant_track_data();
				constant_data_translations = constant_data_rotations + packed_rotation_size * transform_header.num_constant_rotation_samples;
				constant_data_scales = constant_data_translations + packed_translation_size * transform_header.num_constant_translation_samples;
			}

			template<class decompression_settings_type>
			ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK
			void unpack_rotation_group(
				const persistent_transform_decompression_context_v0& decomp_context,
				const constant_track_cache_read_cursor_v0& read_cursor)
			{
#if ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT == 0	// ACL 2.1 baseline
				const uint32_t num_left_to_unpack = rotations.num_left_to_unpack;
				if (num_left_to_unpack == 0)
					return;	// Nothing left to do, we are done

				// If we have less than half our cache filled with samples, unpack some more
				const uint32_t num_cached = rotations.cache_write_index - read_cursor.cache_read_index;
				if (num_cached >= 16)
					return;	// Enough cached, nothing to do

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

				uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 16);
				rotations.num_left_to_unpack = num_left_to_unpack - num_to_unpack;

				// Write index will be either 0 or 16 here since we always unpack 16 at a time
				const uint32_t cache_write_index = rotations.cache_write_index % 32;
				rotations.cache_write_index += num_to_unpack;

				const uint8_t* constant_track_data = constant_data_rotations;
				rtm::quatf* cache_ptr = &rotations.cached_samples[0][cache_write_index];

				if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					for (uint32_t unpack_index = num_to_unpack; unpack_index != 0; --unpack_index)
					{
						// Unpack
						const rtm::quatf sample = unpack_quat_128(constant_track_data);

						// Cache
						*cache_ptr = sample;

						// Update our pointers
						constant_track_data += sizeof(rtm::float4f);
						cache_ptr++;
					}
				}
				else
				{
					while (num_to_unpack != 0)
					{
						const uint32_t unpack_count = std::min<uint32_t>(num_to_unpack, 4);
						num_to_unpack -= unpack_count;

						// Unpack
						// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
						// The last group contains no padding so we have to make to align our loads properly
						const uint32_t load_size = unpack_count * sizeof(float);

						rtm::vector4f xxxx = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 0));
						rtm::vector4f yyyy = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 1));
						rtm::vector4f zzzz = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 2));

						// Update our read ptr
						constant_track_data += load_size * 3;

						rtm::vector4f wwww = quat_from_positive_w_x4(xxxx, yyyy, zzzz);

						// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
						// isn't very accurate on small inputs, we need to normalize
						if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
							quat_normalize_x4(xxxx, yyyy, zzzz, wwww);

#if defined(RTM_NEON_INTRINSICS) && 0
						// This is much more compact, assemly wise, however it appears to be slower
						// Store interleaved
						const float32x4x4_t result = { xxxx, yyyy, zzzz, wwww };
						vst4q_f32(bit_cast<float*>(cache_ptr), result);
#else
						rtm::vector4f sample0;
						rtm::vector4f sample1;
						rtm::vector4f sample2;
						rtm::vector4f sample3;
						RTM_MATRIXF_TRANSPOSE_4X4(xxxx, yyyy, zzzz, wwww, sample0, sample1, sample2, sample3);

						// Cache
						cache_ptr[0] = rtm::vector_to_quat(sample0);
						cache_ptr[1] = rtm::vector_to_quat(sample1);
						cache_ptr[2] = rtm::vector_to_quat(sample2);
						cache_ptr[3] = rtm::vector_to_quat(sample3);
#endif
						cache_ptr += 4;
					}
				}

				// Update our pointer
				constant_data_rotations = constant_track_data;
#elif ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT == 1	// Unrolled
				const uint32_t num_left_to_unpack = rotations.num_left_to_unpack;
				if (num_left_to_unpack == 0)
					return;	// Nothing left to do, we are done

				// If we have less than half our cache filled with samples, unpack some more
				const uint32_t num_cached = rotations.cache_write_index - read_cursor.cache_read_index;
				if (num_cached >= 16)
					return;	// Enough cached, nothing to do

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

				uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 16);
				rotations.num_left_to_unpack = num_left_to_unpack - num_to_unpack;

				// Write index will be either 0 or 16 here since we always unpack 16 at a time
				const uint32_t cache_write_index = rotations.cache_write_index % 32;
				rotations.cache_write_index += num_to_unpack;

				const uint8_t* constant_track_data = constant_data_rotations;
				rtm::quatf* cache_ptr = &rotations.cached_samples[0][cache_write_index];

				if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					for (uint32_t unpack_index = num_to_unpack; unpack_index != 0; --unpack_index)
					{
						// Unpack
						const rtm::quatf sample = unpack_quat_128(constant_track_data);

						// Cache
						*cache_ptr = sample;

						// Update our pointers
						constant_track_data += sizeof(rtm::float4f);
						cache_ptr++;
					}
				}
				else
				{
					if (num_to_unpack >= 8)
					{
						num_to_unpack -= 8;

						// Cluster our loads together in case they cache miss
						rtm::vector4f xxxx0 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 0));
						rtm::vector4f yyyy0 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 1));
						rtm::vector4f zzzz0 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 2));

						rtm::vector4f xxxx1 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 3));
						rtm::vector4f yyyy1 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 4));
						rtm::vector4f zzzz1 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 5));

						// Update our read ptr
						constant_track_data += sizeof(rtm::float4f) * 6;

						rtm::vector4f wwww0 = quat_from_positive_w_x4(xxxx0, yyyy0, zzzz0);
						rtm::vector4f wwww1 = quat_from_positive_w_x4(xxxx1, yyyy1, zzzz1);

						// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
						// isn't very accurate on small inputs, we need to normalize
						if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
						{
							quat_normalize_x4(xxxx0, yyyy0, zzzz0, wwww0);
							quat_normalize_x4(xxxx1, yyyy1, zzzz1, wwww1);
						}

#if defined(RTM_NEON_INTRINSICS) && 0
						// This is much more compact, assemly wise, however it appears to be slower
						// Store interleaved
						const float32x4x4_t result0 = { xxxx0, yyyy0, zzzz0, wwww0 };
						const float32x4x4_t result1 = { xxxx1, yyyy1, zzzz1, wwww1 };
						vst4q_f32(bit_cast<float*>(cache_ptr + sizeof(float32x4x4_t) * 0), result0);
						vst4q_f32(bit_cast<float*>(cache_ptr + sizeof(float32x4x4_t) * 1), result1);
#else
						rtm::vector4f sample0;
						rtm::vector4f sample1;
						rtm::vector4f sample2;
						rtm::vector4f sample3;
						RTM_MATRIXF_TRANSPOSE_4X4(xxxx0, yyyy0, zzzz0, wwww0, sample0, sample1, sample2, sample3);

						// Cache
						cache_ptr[0] = rtm::vector_to_quat(sample0);
						cache_ptr[1] = rtm::vector_to_quat(sample1);
						cache_ptr[2] = rtm::vector_to_quat(sample2);
						cache_ptr[3] = rtm::vector_to_quat(sample3);

						rtm::vector4f sample4;
						rtm::vector4f sample5;
						rtm::vector4f sample6;
						rtm::vector4f sample7;
						RTM_MATRIXF_TRANSPOSE_4X4(xxxx1, yyyy1, zzzz1, wwww1, sample4, sample5, sample6, sample7);

						// Cache
						cache_ptr[4] = rtm::vector_to_quat(sample4);
						cache_ptr[5] = rtm::vector_to_quat(sample5);
						cache_ptr[6] = rtm::vector_to_quat(sample6);
						cache_ptr[7] = rtm::vector_to_quat(sample7);
#endif
						cache_ptr += 8;
					}

					if (num_to_unpack > 4)
					{
						// Cluster our loads together in case they cache miss
						rtm::vector4f xxxx0 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 0));
						rtm::vector4f yyyy0 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 1));
						rtm::vector4f zzzz0 = rtm::vector_load(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 2));

						// Update our read ptr
						constant_track_data += sizeof(rtm::float4f) * 3;

						// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
						// The last group contains no padding so we have to align our loads properly
						const uint32_t load_size = (num_to_unpack - 4) * sizeof(float);

						rtm::vector4f xxxx1 = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 0));
						rtm::vector4f yyyy1 = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 1));
						rtm::vector4f zzzz1 = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 2));

						// Update our read ptr
						constant_track_data += load_size * 3;

						rtm::vector4f wwww0 = quat_from_positive_w_x4(xxxx0, yyyy0, zzzz0);
						rtm::vector4f wwww1 = quat_from_positive_w_x4(xxxx1, yyyy1, zzzz1);

						// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
						// isn't very accurate on small inputs, we need to normalize
						if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
						{
							quat_normalize_x4(xxxx0, yyyy0, zzzz0, wwww0);
							quat_normalize_x4(xxxx1, yyyy1, zzzz1, wwww1);
						}

#if defined(RTM_NEON_INTRINSICS) && 0
						// This is much more compact, assemly wise, however it appears to be slower
						// Store interleaved
						const float32x4x4_t result0 = { xxxx0, yyyy0, zzzz0, wwww0 };
						const float32x4x4_t result1 = { xxxx1, yyyy1, zzzz1, wwww1 };
						vst4q_f32(bit_cast<float*>(cache_ptr + sizeof(float32x4x4_t) * 0), result0);
						vst4q_f32(bit_cast<float*>(cache_ptr + sizeof(float32x4x4_t) * 1), result1);
#else
						rtm::vector4f sample0;
						rtm::vector4f sample1;
						rtm::vector4f sample2;
						rtm::vector4f sample3;
						RTM_MATRIXF_TRANSPOSE_4X4(xxxx0, yyyy0, zzzz0, wwww0, sample0, sample1, sample2, sample3);

						// Cache
						cache_ptr[0] = rtm::vector_to_quat(sample0);
						cache_ptr[1] = rtm::vector_to_quat(sample1);
						cache_ptr[2] = rtm::vector_to_quat(sample2);
						cache_ptr[3] = rtm::vector_to_quat(sample3);

						rtm::vector4f sample4;
						rtm::vector4f sample5;
						rtm::vector4f sample6;
						rtm::vector4f sample7;
						RTM_MATRIXF_TRANSPOSE_4X4(xxxx1, yyyy1, zzzz1, wwww1, sample4, sample5, sample6, sample7);

						// Cache
						cache_ptr[4] = rtm::vector_to_quat(sample4);
						cache_ptr[5] = rtm::vector_to_quat(sample5);
						cache_ptr[6] = rtm::vector_to_quat(sample6);
						cache_ptr[7] = rtm::vector_to_quat(sample7);
#endif
						cache_ptr += 8;
					}
					else if (num_to_unpack != 0)
					{
						// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
						// The last group contains no padding so we have to align our loads properly
						const uint32_t load_size = num_to_unpack * sizeof(float);

						rtm::vector4f xxxx = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 0));
						rtm::vector4f yyyy = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 1));
						rtm::vector4f zzzz = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 2));

						// Update our read ptr
						constant_track_data += load_size * 3;

						rtm::vector4f wwww = quat_from_positive_w_x4(xxxx, yyyy, zzzz);

						// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
						// isn't very accurate on small inputs, we need to normalize
						if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
							quat_normalize_x4(xxxx, yyyy, zzzz, wwww);

#if defined(RTM_NEON_INTRINSICS) && 0
						// This is much more compact, assemly wise, however it appears to be slower
						// Store interleaved
						const float32x4x4_t result = { xxxx, yyyy, zzzz, wwww };
						vst4q_f32(bit_cast<float*>(cache_ptr), result);
#else
						rtm::vector4f sample0;
						rtm::vector4f sample1;
						rtm::vector4f sample2;
						rtm::vector4f sample3;
						RTM_MATRIXF_TRANSPOSE_4X4(xxxx, yyyy, zzzz, wwww, sample0, sample1, sample2, sample3);

						// Cache
						cache_ptr[0] = rtm::vector_to_quat(sample0);
						cache_ptr[1] = rtm::vector_to_quat(sample1);
						cache_ptr[2] = rtm::vector_to_quat(sample2);
						cache_ptr[3] = rtm::vector_to_quat(sample3);
#endif
						cache_ptr += 4;
					}
				}

				// Update our pointer
				constant_data_rotations = constant_track_data;
#elif ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT == 2	// AVX 8-wide
				const uint32_t num_left_to_unpack = rotations.num_left_to_unpack;
				if (num_left_to_unpack == 0)
					return;	// Nothing left to do, we are done

				// If we have less than half our cache filled with samples, unpack some more
				const uint32_t num_cached = rotations.cache_write_index - read_cursor.cache_read_index;
				if (num_cached >= 16)
					return;	// Enough cached, nothing to do

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

				uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 16);
				rotations.num_left_to_unpack = num_left_to_unpack - num_to_unpack;

				// Write index will be either 0 or 16 here since we always unpack 16 at a time
				const uint32_t cache_write_index = rotations.cache_write_index % 32;
				rotations.cache_write_index += num_to_unpack;

				const uint8_t* constant_track_data = constant_data_rotations;
				rtm::quatf* cache_ptr = &rotations.cached_samples[0][cache_write_index];

				if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					uint32_t write_swizzle_lookup_table = 0x75316420;

					for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
					{
						// Unpack
						const rtm::quatf sample = unpack_quat_128(constant_track_data);

						// Data is stored partially swizzled, see consume_rotation() for details
						const uint32_t swizzled_write_index = write_swizzle_lookup_table & 0x0F;
						write_swizzle_lookup_table = rotate_bits_right(write_swizzle_lookup_table, 4);
						const uint32_t masked_write_index = unpack_index & 0x18;
						const uint32_t write_index = masked_write_index + swizzled_write_index;

						cache_ptr[write_index] = sample;

						// Update our pointers
						constant_track_data += sizeof(rtm::float4f);
					}

					cache_ptr += num_to_unpack;
				}
				else
				{
					const __m256 one_v = _mm256_set1_ps(1.0F);
					const __m256 abs_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFFULL));

					if (num_to_unpack > 8)
					{
						// We'll read 9-16 values at a time
						const uint32_t unpack_count = num_to_unpack;

						// TODO: We could swizzle the data ahead of time during compression, even without AVX
						// we can just re-order the load offsets, not a big deal, and we save the blend/permute
						const __m256 xxxx0_yyyy0 = _mm256_loadu_ps(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 0));
						const __m256 zzzz0_xxxx1 = _mm256_loadu_ps(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 2));
						const __m256 yyyy1_zzzz1 = _mm256_loadu_ps(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 4));

						__m256 xxxx0_xxxx1 = _mm256_blend_ps(xxxx0_yyyy0, zzzz0_xxxx1, 0xF0);
						__m256 yyyy0_yyyy1 = _mm256_permute2f128_ps(xxxx0_yyyy0, yyyy1_zzzz1, 0x21);
						__m256 zzzz0_zzzz1 = _mm256_blend_ps(zzzz0_xxxx1, yyyy1_zzzz1, 0xF0);

						constant_track_data += sizeof(rtm::float4f) * 6;

						__m256 xxxx2_xxxx3;
						__m256 yyyy2_yyyy3;
						__m256 zzzz2_zzzz3;

						// Cluster our loads together in case they cache miss
						if (num_to_unpack > 12)
						{
							// Always load 8x rotations, we might contain garbage in a few lanes but it's fine
							// The last group contains no padding so we have to make to align our loads properly
							const uint32_t load_size = (unpack_count - 12) * sizeof(float);

							const __m256 xxxx2_yyyy2 = _mm256_loadu_ps(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 0));
							const __m256 zzzz2_xxxx3 = _mm256_loadu_ps(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 2));

							constant_track_data += sizeof(rtm::float4f) * 3;
							constant_track_data += load_size;

							const __m128 yyyy3 = _mm_loadu_ps(bit_cast<const float*>(constant_track_data + load_size * 0));
							const __m128 zzzz3 = _mm_loadu_ps(bit_cast<const float*>(constant_track_data + load_size * 1));

							constant_track_data += load_size * 2;

							xxxx2_xxxx3 = _mm256_blend_ps(xxxx2_yyyy2, zzzz2_xxxx3, 0xF0);
							yyyy2_yyyy3 = _mm256_permute2f128_ps(xxxx2_yyyy2, _mm256_castps128_ps256(yyyy3), 0x21);
							zzzz2_zzzz3 = _mm256_insertf128_ps(zzzz2_xxxx3, zzzz3, 1);
						}
						else
						{
							// Always load 8x rotations, we might contain garbage in a few lanes but it's fine
							// The last group contains no padding so we have to make to align our loads properly
							const uint32_t load_size = (unpack_count - 8) * sizeof(float);

							const __m128 xxxx2 = _mm_loadu_ps(bit_cast<const float*>(constant_track_data + load_size * 0));
							const __m128 yyyy2 = _mm_loadu_ps(bit_cast<const float*>(constant_track_data + load_size * 1));
							const __m128 zzzz2 = _mm_loadu_ps(bit_cast<const float*>(constant_track_data + load_size * 2));

							constant_track_data += load_size * 3;

							xxxx2_xxxx3 = _mm256_castps128_ps256(xxxx2);
							yyyy2_yyyy3 = _mm256_castps128_ps256(yyyy2);
							zzzz2_zzzz3 = _mm256_castps128_ps256(zzzz2);
						}

						num_to_unpack -= unpack_count;

						// quat_from_positive_w_avx8
						const __m256 xxxx0_xxxx1_squared = _mm256_mul_ps(xxxx0_xxxx1, xxxx0_xxxx1);
						const __m256 yyyy0_yyyy1_squared = _mm256_mul_ps(yyyy0_yyyy1, yyyy0_yyyy1);
						const __m256 zzzz0_zzzz1_squared = _mm256_mul_ps(zzzz0_zzzz1, zzzz0_zzzz1);

						const __m256 wwww0_wwww1_squared = _mm256_sub_ps(_mm256_sub_ps(_mm256_sub_ps(one_v, xxxx0_xxxx1_squared), yyyy0_yyyy1_squared), zzzz0_zzzz1_squared);
						const __m256 wwww0_wwww1_squared_abs = _mm256_and_ps(wwww0_wwww1_squared, abs_mask);

						__m256 wwww0_wwww1 = _mm256_sqrt_ps(wwww0_wwww1_squared_abs);

						const __m256 xxxx2_xxxx3_squared = _mm256_mul_ps(xxxx2_xxxx3, xxxx2_xxxx3);
						const __m256 yyyy2_yyyy3_squared = _mm256_mul_ps(yyyy2_yyyy3, yyyy2_yyyy3);
						const __m256 zzzz2_zzzz3_squared = _mm256_mul_ps(zzzz2_zzzz3, zzzz2_zzzz3);

						const __m256 wwww2_wwww3_squared = _mm256_sub_ps(_mm256_sub_ps(_mm256_sub_ps(one_v, xxxx2_xxxx3_squared), yyyy2_yyyy3_squared), zzzz2_zzzz3_squared);
						const __m256 wwww2_wwww3_squared_abs = _mm256_and_ps(wwww2_wwww3_squared, abs_mask);

						__m256 wwww2_wwww3 = _mm256_sqrt_ps(wwww2_wwww3_squared_abs);

						// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
						// isn't very accurate on small inputs, we need to normalize
						if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
						{
							// quat_normalize_avx8
							const __m256 dot01 = _mm256_add_ps(_mm256_add_ps(xxxx0_xxxx1_squared, yyyy0_yyyy1_squared), _mm256_add_ps(zzzz0_zzzz1_squared, wwww0_wwww1_squared_abs));

							const __m256 length01 = _mm256_sqrt_ps(dot01);
							const __m256 inv_length01 = _mm256_div_ps(one_v, length01);

							xxxx0_xxxx1 = _mm256_mul_ps(xxxx0_xxxx1, inv_length01);
							yyyy0_yyyy1 = _mm256_mul_ps(yyyy0_yyyy1, inv_length01);
							zzzz0_zzzz1 = _mm256_mul_ps(zzzz0_zzzz1, inv_length01);
							wwww0_wwww1 = _mm256_mul_ps(wwww0_wwww1, inv_length01);

							const __m256 dot23 = _mm256_add_ps(_mm256_add_ps(xxxx2_xxxx3_squared, yyyy2_yyyy3_squared), _mm256_add_ps(zzzz2_zzzz3_squared, wwww2_wwww3_squared_abs));

							const __m256 length23 = _mm256_sqrt_ps(dot23);
							const __m256 inv_length23 = _mm256_div_ps(one_v, length23);

							xxxx2_xxxx3 = _mm256_mul_ps(xxxx2_xxxx3, inv_length23);
							yyyy2_yyyy3 = _mm256_mul_ps(yyyy2_yyyy3, inv_length23);
							zzzz2_zzzz3 = _mm256_mul_ps(zzzz2_zzzz3, inv_length23);
							wwww2_wwww3 = _mm256_mul_ps(wwww2_wwww3, inv_length23);
						}

						const __m256 x0x1y0y1_x4x5y4y5 = _mm256_shuffle_ps(xxxx0_xxxx1, yyyy0_yyyy1, _MM_SHUFFLE(1, 0, 1, 0));
						const __m256 x2x3y2y3_x6x7y6y7 = _mm256_shuffle_ps(xxxx0_xxxx1, yyyy0_yyyy1, _MM_SHUFFLE(3, 2, 3, 2));
						const __m256 z0z1w0w1_z4z5w4w5 = _mm256_shuffle_ps(zzzz0_zzzz1, wwww0_wwww1, _MM_SHUFFLE(1, 0, 1, 0));
						const __m256 z2z3w2w3_z6z7w6w7 = _mm256_shuffle_ps(zzzz0_zzzz1, wwww0_wwww1, _MM_SHUFFLE(3, 2, 3, 2));

						const __m256 x0y0z0w0_x4y4z4w4 = _mm256_shuffle_ps(x0x1y0y1_x4x5y4y5, z0z1w0w1_z4z5w4w5, _MM_SHUFFLE(2, 0, 2, 0));
						const __m256 x1y1z1w1_x5y5z5w5 = _mm256_shuffle_ps(x0x1y0y1_x4x5y4y5, z0z1w0w1_z4z5w4w5, _MM_SHUFFLE(3, 1, 3, 1));
						const __m256 x2y2z2w2_x6y6z6w6 = _mm256_shuffle_ps(x2x3y2y3_x6x7y6y7, z2z3w2w3_z6z7w6w7, _MM_SHUFFLE(2, 0, 2, 0));
						const __m256 x3y3z3w3_x7y7z7w7 = _mm256_shuffle_ps(x2x3y2y3_x6x7y6y7, z2z3w2w3_z6z7w6w7, _MM_SHUFFLE(3, 1, 3, 1));

						// Data is stored partially swizzled, see consume_rotation() for details
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 0, x0y0z0w0_x4y4z4w4);
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 8, x1y1z1w1_x5y5z5w5);

						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 16, x2y2z2w2_x6y6z6w6);
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 24, x3y3z3w3_x7y7z7w7);

						const __m256 x8x9y8y9_x12x13y12y13 = _mm256_shuffle_ps(xxxx2_xxxx3, yyyy2_yyyy3, _MM_SHUFFLE(1, 0, 1, 0));
						const __m256 x10x11y10y11_x14x15y14y15 = _mm256_shuffle_ps(xxxx2_xxxx3, yyyy2_yyyy3, _MM_SHUFFLE(3, 2, 3, 2));
						const __m256 z8z9w8w9_z12z13w12w13 = _mm256_shuffle_ps(zzzz2_zzzz3, wwww2_wwww3, _MM_SHUFFLE(1, 0, 1, 0));
						const __m256 z10z11w10w11_z14z15w14w15 = _mm256_shuffle_ps(zzzz2_zzzz3, wwww2_wwww3, _MM_SHUFFLE(3, 2, 3, 2));

						const __m256 x8y8z8w8_x12y12z12w12 = _mm256_shuffle_ps(x8x9y8y9_x12x13y12y13, z8z9w8w9_z12z13w12w13, _MM_SHUFFLE(2, 0, 2, 0));
						const __m256 x9y9z9w9_x13y13z13w13 = _mm256_shuffle_ps(x8x9y8y9_x12x13y12y13, z8z9w8w9_z12z13w12w13, _MM_SHUFFLE(3, 1, 3, 1));
						const __m256 x10y10z10w10_x14y14z14w14 = _mm256_shuffle_ps(x10x11y10y11_x14x15y14y15, z10z11w10w11_z14z15w14w15, _MM_SHUFFLE(2, 0, 2, 0));
						const __m256 x11y11z11w11_x15y15z15w15 = _mm256_shuffle_ps(x10x11y10y11_x14x15y14y15, z10z11w10w11_z14z15w14w15, _MM_SHUFFLE(3, 1, 3, 1));

						// Data is stored partially swizzled, see consume_rotation() for details
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 32, x8y8z8w8_x12y12z12w12);
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 40, x9y9z9w9_x13y13z13w13);

						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 48, x10y10z10w10_x14y14z14w14);
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 56, x11y11z11w11_x15y15z15w15);

						cache_ptr += 16;
					}
					else if (num_to_unpack > 4)
					{
						// We'll read 5-8 values at a time
						const uint32_t unpack_count = std::min<uint32_t>(num_to_unpack, 8);

						// Always load 8x rotations, we might contain garbage in a few lanes but it's fine
						// The last group contains no padding so we have to make to align our loads properly
						const uint32_t load_size = (unpack_count - 4) * sizeof(float);

						const __m256 xxxx0_yyyy0 = _mm256_loadu_ps(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 0));
						const __m256 zzzz0_xxxx1 = _mm256_loadu_ps(bit_cast<const float*>(constant_track_data + sizeof(rtm::float4f) * 2));

						constant_track_data += sizeof(rtm::float4f) * 3;
						constant_track_data += load_size;

						const __m128 yyyy1 = _mm_loadu_ps(bit_cast<const float*>(constant_track_data + load_size * 0));
						const __m128 zzzz1 = _mm_loadu_ps(bit_cast<const float*>(constant_track_data + load_size * 1));

						constant_track_data += load_size * 2;
						num_to_unpack -= unpack_count;

						__m256 xxxx0_xxxx1 = _mm256_blend_ps(xxxx0_yyyy0, zzzz0_xxxx1, 0xF0);
						__m256 yyyy0_yyyy1 = _mm256_permute2f128_ps(xxxx0_yyyy0, _mm256_castps128_ps256(yyyy1), 0x21);
						__m256 zzzz0_zzzz1 = _mm256_insertf128_ps(zzzz0_xxxx1, zzzz1, 1);

						// quat_from_positive_w_avx8
						const __m256 xxxx0_xxxx1_squared = _mm256_mul_ps(xxxx0_xxxx1, xxxx0_xxxx1);
						const __m256 yyyy0_yyyy1_squared = _mm256_mul_ps(yyyy0_yyyy1, yyyy0_yyyy1);
						const __m256 zzzz0_zzzz1_squared = _mm256_mul_ps(zzzz0_zzzz1, zzzz0_zzzz1);

						const __m256 wwww0_wwww1_squared = _mm256_sub_ps(_mm256_sub_ps(_mm256_sub_ps(one_v, xxxx0_xxxx1_squared), yyyy0_yyyy1_squared), zzzz0_zzzz1_squared);
						const __m256 wwww0_wwww1_squared_abs = _mm256_and_ps(wwww0_wwww1_squared, abs_mask);

						__m256 wwww0_wwww1 = _mm256_sqrt_ps(wwww0_wwww1_squared_abs);

						// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
						// isn't very accurate on small inputs, we need to normalize
						if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
						{
							// quat_normalize_avx8
							const __m256 dot01 = _mm256_add_ps(_mm256_add_ps(xxxx0_xxxx1_squared, yyyy0_yyyy1_squared), _mm256_add_ps(zzzz0_zzzz1_squared, wwww0_wwww1_squared_abs));

							const __m256 length01 = _mm256_sqrt_ps(dot01);
							const __m256 inv_length01 = _mm256_div_ps(one_v, length01);

							xxxx0_xxxx1 = _mm256_mul_ps(xxxx0_xxxx1, inv_length01);
							yyyy0_yyyy1 = _mm256_mul_ps(yyyy0_yyyy1, inv_length01);
							zzzz0_zzzz1 = _mm256_mul_ps(zzzz0_zzzz1, inv_length01);
							wwww0_wwww1 = _mm256_mul_ps(wwww0_wwww1, inv_length01);
						}

						const __m256 x0x1y0y1_x4x5y4y5 = _mm256_shuffle_ps(xxxx0_xxxx1, yyyy0_yyyy1, _MM_SHUFFLE(1, 0, 1, 0));
						const __m256 x2x3y2y3_x6x7y6y7 = _mm256_shuffle_ps(xxxx0_xxxx1, yyyy0_yyyy1, _MM_SHUFFLE(3, 2, 3, 2));
						const __m256 z0z1w0w1_z4z5w4w5 = _mm256_shuffle_ps(zzzz0_zzzz1, wwww0_wwww1, _MM_SHUFFLE(1, 0, 1, 0));
						const __m256 z2z3w2w3_z6z7w6w7 = _mm256_shuffle_ps(zzzz0_zzzz1, wwww0_wwww1, _MM_SHUFFLE(3, 2, 3, 2));

						const __m256 x0y0z0w0_x4y4z4w4 = _mm256_shuffle_ps(x0x1y0y1_x4x5y4y5, z0z1w0w1_z4z5w4w5, _MM_SHUFFLE(2, 0, 2, 0));
						const __m256 x1y1z1w1_x5y5z5w5 = _mm256_shuffle_ps(x0x1y0y1_x4x5y4y5, z0z1w0w1_z4z5w4w5, _MM_SHUFFLE(3, 1, 3, 1));
						const __m256 x2y2z2w2_x6y6z6w6 = _mm256_shuffle_ps(x2x3y2y3_x6x7y6y7, z2z3w2w3_z6z7w6w7, _MM_SHUFFLE(2, 0, 2, 0));
						const __m256 x3y3z3w3_x7y7z7w7 = _mm256_shuffle_ps(x2x3y2y3_x6x7y6y7, z2z3w2w3_z6z7w6w7, _MM_SHUFFLE(3, 1, 3, 1));

						// Data is stored partially swizzled, see consume_rotation() for details
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 0, x0y0z0w0_x4y4z4w4);
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 8, x1y1z1w1_x5y5z5w5);

						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 16, x2y2z2w2_x6y6z6w6);
						_mm256_store_ps(bit_cast<float*>(cache_ptr) + 24, x3y3z3w3_x7y7z7w7);

						cache_ptr += 8;
					}
					else
					{
						// We'll read 1-4 values at a time
						// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
						// The last group contains no padding so we have to align our loads properly
						const uint32_t load_size = num_to_unpack * sizeof(float);

						rtm::vector4f xxxx = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 0));
						rtm::vector4f yyyy = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 1));
						rtm::vector4f zzzz = rtm::vector_load(bit_cast<const float*>(constant_track_data + load_size * 2));

						// Update our read ptr
						constant_track_data += load_size * 3;

						// quat_from_positive_w_x4
						// We inline it here manually to be able to re-use the same constants
						__m128 xxxx_squared = _mm_mul_ps(xxxx, xxxx);
						__m128 yyyy_squared = _mm_mul_ps(yyyy, yyyy);
						__m128 zzzz_squared = _mm_mul_ps(zzzz, zzzz);

						__m128 wwww_squared = _mm_sub_ps(_mm_sub_ps(_mm_sub_ps(_mm256_castps256_ps128(one_v), xxxx_squared), yyyy_squared), zzzz_squared);
						__m128 wwww_squared_abs = _mm_and_ps(wwww_squared, _mm256_castps256_ps128(abs_mask));
						rtm::vector4f wwww = _mm_sqrt_ps(wwww_squared_abs);

						// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
						// isn't very accurate on small inputs, we need to normalize
						if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
						{
							// quat_normalize_x4(xxxx, yyyy, zzzz, wwww);
							// We inline it here manually to be able to re-use the same variables and constant
							const __m128 dot = _mm_add_ps(_mm_add_ps(xxxx_squared, yyyy_squared), _mm_add_ps(zzzz_squared, wwww_squared_abs));

							const __m128 length = _mm_sqrt_ps(dot);
							const __m128 inv_length = _mm_div_ps(_mm256_castps256_ps128(one_v), length);

							xxxx = _mm_mul_ps(xxxx, inv_length);
							yyyy = _mm_mul_ps(yyyy, inv_length);
							zzzz = _mm_mul_ps(zzzz, inv_length);
							wwww = _mm_mul_ps(wwww, inv_length);
						}

						rtm::vector4f sample0;
						rtm::vector4f sample1;
						rtm::vector4f sample2;
						rtm::vector4f sample3;
						RTM_MATRIXF_TRANSPOSE_4X4(xxxx, yyyy, zzzz, wwww, sample0, sample1, sample2, sample3);

						// Data is stored partially swizzled, see consume_rotation() for details
						// 0, 4, 1, 5, 2, 6, 3, 7
						cache_ptr[0] = rtm::vector_to_quat(sample0);
						cache_ptr[2] = rtm::vector_to_quat(sample1);
						cache_ptr[4] = rtm::vector_to_quat(sample2);
						cache_ptr[6] = rtm::vector_to_quat(sample3);

						cache_ptr += 8;
					}
				}

				// Update our pointer
				constant_data_rotations = constant_track_data;
#endif
			}

			template<class decompression_settings_type>
			RTM_DISABLE_SECURITY_COOKIE_CHECK void skip_rotation_groups(const persistent_transform_decompression_context_v0& decomp_context, uint32_t num_groups_to_skip)
			{
				// We only support skipping full groups
				const uint32_t num_left_to_unpack = rotations.num_left_to_unpack;
				const uint32_t num_to_skip = num_groups_to_skip * 4;
				ACL_ASSERT(num_to_skip < num_left_to_unpack, "Cannot skip rotations that aren't present");

				rotations.num_left_to_unpack = num_left_to_unpack - num_to_skip;

				const uint8_t* constant_track_data = constant_data_rotations;

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
					constant_track_data += num_to_skip * sizeof(rtm::float4f);
				else
					constant_track_data += num_to_skip * sizeof(rtm::float3f);

				constant_data_rotations = constant_track_data;

				// Prefetch our group
				ACL_IMPL_CONSTANT_PREFETCH(constant_track_data);
			}

			template<class decompression_settings_type>
			RTM_DISABLE_SECURITY_COOKIE_CHECK rtm::quatf RTM_SIMD_CALL unpack_rotation_within_group(const persistent_transform_decompression_context_v0& decomp_context, uint32_t unpack_index) const
			{
				ACL_ASSERT(unpack_index < rotations.num_left_to_unpack && unpack_index < 4, "Cannot unpack sample that isn't present");

				rtm::quatf sample;

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					const uint8_t* constant_track_data = constant_data_rotations + (unpack_index * sizeof(rtm::float4f));
					sample = unpack_quat_128(constant_track_data);
				}
				else
				{
					// Data is in SOA form
					const uint32_t group_size = std::min<uint32_t>(rotations.num_left_to_unpack, 4);
					const float* constant_track_data = bit_cast<const float*>(constant_data_rotations) + unpack_index;
					const float x = constant_track_data[group_size * 0];
					const float y = constant_track_data[group_size * 1];
					const float z = constant_track_data[group_size * 2];
					const rtm::vector4f sample_v = rtm::vector_set(x, y, z, 0.0F);
					sample = quat_from_positive_w_stable(sample_v);

					// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
					// isn't very accurate on small inputs, we need to normalize
					if (decompression_settings_type::get_rotation_normalization_policy() == rotation_normalization_policy_t::always)
						sample = quat_normalize_stable(sample);
				}

				ACL_ASSERT(rtm::quat_is_finite(sample), "Sample is not valid!");
				ACL_ASSERT(rtm::quat_is_normalized(sample), "Sample is not normalized!");
				return sample;
			}

			RTM_DISABLE_SECURITY_COOKIE_CHECK const rtm::quatf& RTM_SIMD_CALL consume_rotation(constant_track_cache_read_cursor_v0& read_cursor) const
			{
				ACL_ASSERT(read_cursor.cache_read_index < rotations.cache_write_index, "Attempting to consume a constant sample that isn't cached");
				const uint32_t cache_read_index = read_cursor.cache_read_index++;

#if ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT == 2	// AVX 8-wide
				// Total overhead should be 1-2 cycles extra per consumed rotation vs non-swizzle
				// This assumes that the loopup table remains within a register
				// Clang: this is the case with Clang, the table remains in register
				//        4 instructions are added: MOV+ROL+AND+OR (clang uses ROL instead of ROR)
				//        and the original AND remains
				// GCC: this is also the case with GCC, the asm is comparable
				// MSVC: similar asm as well but it needed the help of a standalone read cursor to keep
				//       the cache index and table in register

				// These two lines and the next one computing the masked index are all independent
				// from one another and should execute in a single cycle (AND+ROR+AND instructions)
				const uint32_t swizzled_read_index = read_cursor.read_swizzle_lookup_table & 0x0F;
				read_cursor.read_swizzle_lookup_table = rotate_bits_right(read_cursor.read_swizzle_lookup_table, 4);

				// We take the modulo with 32, then mask out the bottom 7 bits that we'll replace
				// with our index. This can be accomplished in a single AND operation:
				// modulo 32 = AND 0x1F (retain bottom 5 bits)
				// masking our the bottom bits with AND ~0x07 = AND 0xFFFFFFF8 (retain top 29 bits)
				// Combined = 0x0000001F AND 0xFFFFFFF8 = AND 0x18 (retain middle 2 bits)
				const uint32_t masked_read_index = cache_read_index & 0x18;

				// This instruction depends on the prior two values and should execute in
				// a single cycle, comparable with the non-swizzle variant
				const uint32_t read_index = masked_read_index | swizzled_read_index;

				return rotations.cached_samples[0][read_index];
#else
				return rotations.cached_samples[0][cache_read_index % 32];
#endif
			}

			RTM_DISABLE_SECURITY_COOKIE_CHECK void skip_translation_groups(uint32_t num_groups_to_skip)
			{
				const uint8_t* constant_track_data = constant_data_translations;

				// We only support skipping full groups
				const uint32_t num_to_skip = num_groups_to_skip * 4;
				constant_track_data += num_to_skip * sizeof(rtm::float3f);

				constant_data_translations = constant_track_data;

				// Prefetch our group
				ACL_IMPL_CONSTANT_PREFETCH(constant_track_data);
			}

			RTM_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_translation_within_group(uint32_t unpack_index) const
			{
				ACL_ASSERT(unpack_index < 4, "Cannot unpack sample that isn't present");

				const uint8_t* constant_track_data = constant_data_translations + (unpack_index * sizeof(rtm::float3f));
				const rtm::vector4f sample = rtm::vector_load(constant_track_data);
				ACL_ASSERT(rtm::vector_is_finite3(sample), "Sample is not valid!");
				return sample;
			}

			RTM_DISABLE_SECURITY_COOKIE_CHECK void skip_scale_groups(uint32_t num_groups_to_skip)
			{
				const uint8_t* constant_track_data = constant_data_scales;

				// We only support skipping full groups
				const uint32_t num_to_skip = num_groups_to_skip * 4;
				constant_track_data += num_to_skip * sizeof(rtm::float3f);

				constant_data_scales = constant_track_data;

				// Prefetch our group
				ACL_IMPL_CONSTANT_PREFETCH(constant_track_data);
			}

			RTM_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_scale_within_group(uint32_t unpack_index) const
			{
				ACL_ASSERT(unpack_index < 4, "Cannot unpack sample that isn't present");

				const uint8_t* constant_track_data = constant_data_scales + (unpack_index * sizeof(rtm::float3f));
				const rtm::vector4f sample = rtm::vector_load(constant_track_data);
				ACL_ASSERT(rtm::vector_is_finite3(sample), "Sample is not valid!");
				return sample;
			}
		};
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif

#if defined(RTM_COMPILER_GCC)
	#pragma GCC diagnostic pop
#endif

#undef ACL_IMPL_CONSTANT_ROTATION_UNPACKING_VARIANT

ACL_IMPL_FILE_PRAGMA_POP
