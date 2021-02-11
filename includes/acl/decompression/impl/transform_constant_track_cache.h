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

#include "acl/core/track_formats.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/impl/track_cache.h"
#include "acl/decompression/impl/transform_decompression_context.h"
#include "acl/math/quat_packing.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

#define ACL_IMPL_USE_CONSTANT_PREFETCH

ACL_IMPL_FILE_PRAGMA_PUSH

// We only initialize some variables when we need them which prompts the compiler to complain
// The usage is perfectly safe and because this code is VERY hot and needs to be as fast as possible,
// we disable the warning to avoid zeroing out things we don't need
#if defined(ACL_COMPILER_GCC)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

namespace acl
{
#if defined(ACL_IMPL_USE_CONSTANT_PREFETCH)
#define ACL_IMPL_CONSTANT_PREFETCH(ptr) memory_prefetch(ptr)
#else
#define ACL_IMPL_CONSTANT_PREFETCH(ptr) (void)(ptr)
#endif

	namespace acl_impl
	{
		struct constant_track_cache_v0
		{
			track_cache_quatf_v0 rotations;

			// Points to our packed sub-track data
			const uint8_t*	constant_data_rotations;
			const uint8_t*	constant_data_translations;
			const uint8_t*	constant_data_scales;

			template<class decompression_settings_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void initialize(const persistent_transform_decompression_context_v0& decomp_context)
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
			ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_rotation_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
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

				uint32_t num_left_to_unpack = rotations.num_left_to_unpack;
				if (num_left_to_unpack == 0)
					return;	// Nothing left to do, we are done

							// If we have less than 4 cached samples, unpack 4 more and prefetch the next cache line
				const uint32_t num_cached = rotations.get_num_cached();
				if (num_cached >= 4)
					return;	// Enough cached, nothing to do

				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				num_left_to_unpack -= num_to_unpack;
				rotations.num_left_to_unpack = num_left_to_unpack;

				// Write index will be either 0 or 4 here since we always unpack 4 at a time
				uint32_t cache_write_index = rotations.cache_write_index % 8;
				rotations.cache_write_index += num_to_unpack;

				const uint8_t* constant_track_data = constant_data_rotations;

				if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				{
					for (uint32_t unpack_index = num_to_unpack; unpack_index != 0; --unpack_index)
					{
						// Unpack
						const rtm::quatf sample = unpack_quat_128(constant_track_data);

						ACL_ASSERT(rtm::quat_is_finite(sample), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(sample), "Rotation is not normalized!");

						// Cache
						rotations.cached_samples[cache_write_index] = sample;
						cache_write_index++;

						// Update our read ptr
						constant_track_data += sizeof(rtm::float4f);
					}
				}
				else
				{
					// Unpack
					// Always load 4x rotations, we might contain garbage in a few lanes but it's fine
					const uint32_t load_size = num_to_unpack * sizeof(float);

					const rtm::vector4f xxxx = rtm::vector_load(reinterpret_cast<const float*>(constant_track_data + load_size * 0));
					const rtm::vector4f yyyy = rtm::vector_load(reinterpret_cast<const float*>(constant_track_data + load_size * 1));
					const rtm::vector4f zzzz = rtm::vector_load(reinterpret_cast<const float*>(constant_track_data + load_size * 2));

					// Update our read ptr
					constant_track_data += load_size * 3;

					// quat_from_positive_w_soa
					const rtm::vector4f wwww_squared = rtm::vector_sub(rtm::vector_sub(rtm::vector_sub(rtm::vector_set(1.0F), rtm::vector_mul(xxxx, xxxx)), rtm::vector_mul(yyyy, yyyy)), rtm::vector_mul(zzzz, zzzz));

					// w_squared can be negative either due to rounding or due to quantization imprecision, we take the absolute value
					// to ensure the resulting quaternion is always normalized with a positive W component
					const rtm::vector4f wwww = rtm::vector_sqrt(rtm::vector_abs(wwww_squared));

					rtm::vector4f sample0;
					rtm::vector4f sample1;
					rtm::vector4f sample2;
					rtm::vector4f sample3;
					RTM_MATRIXF_TRANSPOSE_4X4(xxxx, yyyy, zzzz, wwww, sample0, sample1, sample2, sample3);

					// Cache
					rtm::quatf* cache_ptr = &rotations.cached_samples[cache_write_index];
					cache_ptr[0] = rtm::vector_to_quat(sample0);
					cache_ptr[1] = rtm::vector_to_quat(sample1);
					cache_ptr[2] = rtm::vector_to_quat(sample2);
					cache_ptr[3] = rtm::vector_to_quat(sample3);

#if defined(ACL_HAS_ASSERT_CHECKS)
					for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
					{
						ACL_ASSERT(rtm::quat_is_finite(rotations.cached_samples[cache_write_index + unpack_index]), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotations.cached_samples[cache_write_index + unpack_index]), "Rotation is not normalized!");
					}
#endif
				}

				// Update our pointer
				constant_data_rotations = constant_track_data;
			}

			template<class decompression_settings_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void skip_rotation_groups(const persistent_transform_decompression_context_v0& decomp_context, uint32_t num_groups_to_skip)
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
			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::quatf RTM_SIMD_CALL unpack_rotation_within_group(const persistent_transform_decompression_context_v0& decomp_context, uint32_t unpack_index)
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
					const float* constant_track_data = reinterpret_cast<const float*>(constant_data_rotations) + unpack_index;
					const float x = constant_track_data[group_size * 0];
					const float y = constant_track_data[group_size * 1];
					const float z = constant_track_data[group_size * 2];
					const rtm::vector4f sample_v = rtm::vector_set(x, y, z, 0.0F);
					sample = rtm::quat_from_positive_w(sample_v);
				}

				ACL_ASSERT(rtm::quat_is_finite(sample), "Sample is not valid!");
				ACL_ASSERT(rtm::quat_is_normalized(sample), "Sample is not normalized!");
				return sample;
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK const rtm::quatf& RTM_SIMD_CALL consume_rotation()
			{
				ACL_ASSERT(rotations.cache_read_index < rotations.cache_write_index, "Attempting to consume a constant sample that isn't cached");
				const uint32_t cache_read_index = rotations.cache_read_index++;
				return rotations.cached_samples[cache_read_index % 8];
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK void skip_translation_groups(uint32_t num_groups_to_skip)
			{
				const uint8_t* constant_track_data = constant_data_translations;

				// We only support skipping full groups
				const uint32_t num_to_skip = num_groups_to_skip * 4;
				constant_track_data += num_to_skip * sizeof(rtm::float3f);

				constant_data_translations = constant_track_data;

				// Prefetch our group
				ACL_IMPL_CONSTANT_PREFETCH(constant_track_data);
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_translation_within_group(uint32_t unpack_index)
			{
				ACL_ASSERT(unpack_index < 4, "Cannot unpack sample that isn't present");

				const uint8_t* constant_track_data = constant_data_translations + (unpack_index * sizeof(rtm::float3f));
				const rtm::vector4f sample = rtm::vector_load(constant_track_data);
				ACL_ASSERT(rtm::vector_is_finite3(sample), "Sample is not valid!");
				return sample;
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK const uint8_t* consume_translation()
			{
				const uint8_t* sample_ptr = constant_data_translations;
				constant_data_translations += sizeof(rtm::float3f);
				return sample_ptr;
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK void skip_scale_groups(uint32_t num_groups_to_skip)
			{
				const uint8_t* constant_track_data = constant_data_scales;

				// We only support skipping full groups
				const uint32_t num_to_skip = num_groups_to_skip * 4;
				constant_track_data += num_to_skip * sizeof(rtm::float3f);

				constant_data_scales = constant_track_data;

				// Prefetch our group
				ACL_IMPL_CONSTANT_PREFETCH(constant_track_data);
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_scale_within_group(uint32_t unpack_index)
			{
				ACL_ASSERT(unpack_index < 4, "Cannot unpack sample that isn't present");

				const uint8_t* constant_track_data = constant_data_scales + (unpack_index * sizeof(rtm::float3f));
				const rtm::vector4f sample = rtm::vector_load(constant_track_data);
				ACL_ASSERT(rtm::vector_is_finite3(sample), "Sample is not valid!");
				return sample;
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK const uint8_t* consume_scale()
			{
				const uint8_t* sample_ptr = constant_data_scales;
				constant_data_scales += sizeof(rtm::float3f);
				return sample_ptr;
			}
		};
	}
}

#if defined(ACL_COMPILER_GCC)
	#pragma GCC diagnostic pop
#endif

ACL_IMPL_FILE_PRAGMA_POP
