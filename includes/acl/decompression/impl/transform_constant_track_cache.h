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
//#define ACL_IMPL_VEC3_UNPACK

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
#if defined(ACL_IMPL_USE_CONSTANT_PREFETCH)
#define ACL_IMPL_CONSTANT_PREFETCH(ptr) memory_prefetch(ptr)
#else
#define ACL_IMPL_CONSTANT_PREFETCH(ptr) (void)(ptr)
#endif

	namespace acl_impl
	{
		template<class decompression_settings_type>
		ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_constant_quat(const persistent_transform_decompression_context_v0& decomp_context, track_cache_quatf_v0& track_cache, const uint8_t*& constant_data)
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

			uint32_t num_left_to_unpack = track_cache.num_left_to_unpack;
			if (num_left_to_unpack == 0)
				return;	// Nothing left to do, we are done

						// If we have less than 4 cached samples, unpack 4 more and prefetch the next cache line
			const uint32_t num_cached = track_cache.get_num_cached();
			if (num_cached >= 4)
				return;	// Enough cached, nothing to do

			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

			const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
			num_left_to_unpack -= num_to_unpack;
			track_cache.num_left_to_unpack = num_left_to_unpack;

			// Write index will be either 0 or 4 here since we always unpack 4 at a time
			uint32_t cache_write_index = track_cache.cache_write_index % 8;
			track_cache.cache_write_index += num_to_unpack;

			const uint8_t* constant_track_data = constant_data;

			if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
			{
				for (uint32_t unpack_index = num_to_unpack; unpack_index != 0; --unpack_index)
				{
					// Unpack
					const rtm::quatf sample = unpack_quat_128(constant_track_data);

					ACL_ASSERT(rtm::quat_is_finite(sample), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(sample), "Rotation is not normalized!");

					// Cache
					track_cache.cached_samples[cache_write_index] = sample;
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
				rtm::quatf* cache_ptr = &track_cache.cached_samples[cache_write_index];
				cache_ptr[0] = rtm::vector_to_quat(sample0);
				cache_ptr[1] = rtm::vector_to_quat(sample1);
				cache_ptr[2] = rtm::vector_to_quat(sample2);
				cache_ptr[3] = rtm::vector_to_quat(sample3);

#if defined(ACL_HAS_ASSERT_CHECKS)
				for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
				{
					ACL_ASSERT(rtm::quat_is_finite(track_cache.cached_samples[cache_write_index + unpack_index]), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(track_cache.cached_samples[cache_write_index + unpack_index]), "Rotation is not normalized!");
				}
#endif
			}

			// Update our pointer
			constant_data = constant_track_data;

			ACL_IMPL_CONSTANT_PREFETCH(constant_track_data + 63);
		}

#if defined(ACL_IMPL_VEC3_UNPACK)
		inline void unpack_constant_vector3(track_cache_vector4f_v0& track_cache, const uint8_t*& constant_data)
		{
			uint32_t num_left_to_unpack = track_cache.num_left_to_unpack;
			if (num_left_to_unpack == 0)
				return;	// Nothing left to do, we are done

			const uint32_t packed_size = get_packed_vector_size(vector_format8::vector3f_full);

			// If we have less than 4 cached samples, unpack 4 more and prefetch the next cache line
			const uint32_t num_cached = track_cache.get_num_cached();
			if (num_cached < 4)
			{
				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack, 4);
				num_left_to_unpack -= num_to_unpack;
				track_cache.num_left_to_unpack = num_left_to_unpack;

				// Write index will be either 0 or 4 here since we always unpack 4 at a time
				uint32_t cache_write_index = track_cache.cache_write_index % 8;
				track_cache.cache_write_index += num_to_unpack;

				const uint8_t* constant_track_data = constant_data;

				for (uint32_t unpack_index = num_to_unpack; unpack_index != 0; --unpack_index)
				{
					// Unpack
					// Constant vector3 tracks store the remaining sample with full precision
					const rtm::vector4f sample = unpack_vector3_96_unsafe(constant_track_data);
					ACL_ASSERT(rtm::vector_is_finite3(sample), "Vector3 is not valid!");

					// TODO: Fill in W component with something sensible?

					// Cache
					track_cache.cached_samples[cache_write_index] = sample;
					cache_write_index++;

					// Update our read ptr
					constant_track_data += packed_size;
				}

				constant_data = constant_track_data;

				// Prefetch the next cache line even if we don't have any data left
				// By the time we unpack again, it will have arrived in the CPU cache
				// With our full precision format, we have at most 5.33 samples per cache line

				// If our pointer was already aligned to a cache line before we unpacked our 4 values,
				// it now points to the first byte of the next cache line. Any offset between 0-63 will fetch it.
				// If our pointer had some offset into a cache line, we might have spanned 2 cache lines.
				// If this happens, we probably already read some data from the next cache line in which
				// case we don't need to prefetch it and we can go to the next one. Any offset after the end
				// of this cache line will fetch it. For safety, we prefetch 63 bytes ahead.
				// Prefetch 4 samples ahead in all levels of the CPU cache
				ACL_IMPL_CONSTANT_PREFETCH(constant_track_data + 63);
			}
		}
#endif

		struct constant_track_cache_v0
		{
			track_cache_quatf_v0 rotations;

#if defined(ACL_IMPL_VEC3_UNPACK)
			track_cache_vector4f_v0 translations;
			track_cache_vector4f_v0 scales;
#endif

#if defined(ACL_IMPL_USE_CONSTANT_GROUPS)
			// How many we have left to unpack in total
			uint32_t		num_left_to_unpack_translations;
			uint32_t		num_left_to_unpack_scales;

			// How many we have cached (faked for translations/scales)
			uint32_t		num_unpacked_translations = 0;
			uint32_t		num_unpacked_scales = 0;

			// How many we have left in our group
			uint32_t		num_group_translations[2];
			uint32_t		num_group_scales[2];

			const uint8_t*	constant_data;
			const uint8_t*	constant_data_translations[2];
			const uint8_t*	constant_data_scales[2];
#else
			// Points to our packed sub-track data
			const uint8_t*	constant_data_rotations;
			const uint8_t*	constant_data_translations;
			const uint8_t*	constant_data_scales;
#endif

			template<class decompression_settings_type>
			ACL_DISABLE_SECURITY_COOKIE_CHECK void initialize(const persistent_transform_decompression_context_v0& decomp_context)
			{
				const transform_tracks_header& transform_header = get_transform_tracks_header(*decomp_context.tracks);

				rotations.num_left_to_unpack = transform_header.num_constant_rotation_samples;

#if defined(ACL_IMPL_VEC3_UNPACK)
				translations.num_left_to_unpack = transform_header.num_constant_translation_samples;
				scales.num_left_to_unpack = transform_header.num_constant_scale_samples;
#endif

#if defined(ACL_IMPL_USE_CONSTANT_GROUPS)
				num_left_to_unpack_translations = transform_header.num_constant_translation_samples;
				num_left_to_unpack_scales = transform_header.num_constant_scale_samples;

				constant_data = decomp_context.constant_track_data;
				constant_data_translations[0] = constant_data_translations[1] = nullptr;
				constant_data_scales[0] = constant_data_scales[1] = nullptr;
				num_group_translations[0] = num_group_translations[1] = 0;
				num_group_scales[0] = num_group_scales[1] = 0;
#else
				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				const rotation_format8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
				const uint32_t packed_rotation_size = get_packed_rotation_size(packed_format);
				const uint32_t packed_translation_size = get_packed_vector_size(vector_format8::vector3f_full);

				constant_data_rotations = decomp_context.constant_track_data;
				constant_data_translations = constant_data_rotations + packed_rotation_size * transform_header.num_constant_rotation_samples;
				constant_data_scales = constant_data_translations + packed_translation_size * transform_header.num_constant_translation_samples;
#endif
			}

			template<class decompression_settings_type>
			ACL_FORCE_INLINE ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_rotation_group(const persistent_transform_decompression_context_v0& decomp_context)
			{
#if defined(ACL_IMPL_USE_CONSTANT_GROUPS)
				unpack_constant_quat<decompression_settings_type>(decomp_context, rotations, constant_data);
#else
				unpack_constant_quat<decompression_settings_type>(decomp_context, rotations, constant_data_rotations);
#endif
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

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::quatf RTM_SIMD_CALL consume_rotation()
			{
				ACL_ASSERT(rotations.cache_read_index < rotations.cache_write_index, "Attempting to consume a constant sample that isn't cached");
				const uint32_t cache_read_index = rotations.cache_read_index++;
				return rotations.cached_samples[cache_read_index % 8];
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_translation_group()
			{
#if defined(ACL_IMPL_VEC3_UNPACK)
				unpack_constant_vector3(translations, constant_data_translations);
#else
#if defined(ACL_IMPL_USE_CONSTANT_GROUPS)
				if (num_left_to_unpack_translations == 0 || num_unpacked_translations >= 4)
					return;	// Enough unpacked or nothing to do

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack_translations, 4);
				num_left_to_unpack_translations -= num_to_unpack;

				// If we have data already unpacked, store in index 1 otherwise store in 0
				const uint32_t unpack_index = num_unpacked_translations > 0 ? 1 : 0;
				constant_data_translations[unpack_index] = constant_data;
				num_group_translations[unpack_index] = num_to_unpack;
				constant_data += sizeof(rtm::float3f) * num_to_unpack;

				num_unpacked_translations += num_to_unpack;

				ACL_IMPL_CONSTANT_PREFETCH(constant_data + 63);
#else
				ACL_IMPL_CONSTANT_PREFETCH(constant_data_translations + 63);
#endif
#endif
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

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL consume_translation()
			{
#if defined(ACL_IMPL_VEC3_UNPACK)
				ACL_ASSERT(translations.cache_read_index < translations.cache_write_index, "Attempting to consume a constant sample that isn't cached");
				const uint32_t cache_read_index = translations.cache_read_index++;
				return translations.cached_samples[cache_read_index % 8];
#else
#if defined(ACL_IMPL_USE_CONSTANT_GROUPS)
				const rtm::vector4f sample = rtm::vector_load(constant_data_translations[0]);
				num_group_translations[0]--;
				num_unpacked_translations--;

				// If we finished reading from the first group, swap it out otherwise increment our entry
				if (num_group_translations[0] == 0)
				{
					constant_data_translations[0] = constant_data_translations[1];
					num_group_translations[0] = num_group_translations[1];
				}
				else
					constant_data_translations[0] += sizeof(rtm::float3f);
#else
				const rtm::vector4f sample = rtm::vector_load(constant_data_translations);
				ACL_ASSERT(rtm::vector_is_finite3(sample), "Sample is not valid!");
				constant_data_translations += sizeof(rtm::float3f);
#endif
				return sample;
#endif
			}

			ACL_DISABLE_SECURITY_COOKIE_CHECK void unpack_scale_group()
			{
#if defined(ACL_IMPL_VEC3_UNPACK)
				unpack_constant_vector3(scales, constant_data_scales);
#else
#if defined(ACL_IMPL_USE_CONSTANT_GROUPS)
				if (num_left_to_unpack_scales == 0 || num_unpacked_scales >= 4)
					return;	// Enough unpacked or nothing to do

				const uint32_t num_to_unpack = std::min<uint32_t>(num_left_to_unpack_scales, 4);
				num_left_to_unpack_scales -= num_to_unpack;

				// If we have data already unpacked, store in index 1 otherwise store in 0
				const uint32_t unpack_index = num_unpacked_scales > 0 ? 1 : 0;
				constant_data_scales[unpack_index] = constant_data;
				num_group_scales[unpack_index] = num_to_unpack;
				constant_data += sizeof(rtm::float3f) * num_to_unpack;

				num_unpacked_scales += num_to_unpack;

				ACL_IMPL_CONSTANT_PREFETCH(constant_data + 63);
#else
				ACL_IMPL_CONSTANT_PREFETCH(constant_data_scales + 63);
#endif
#endif
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

			ACL_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL consume_scale()
			{
#if defined(ACL_IMPL_VEC3_UNPACK)
				ACL_ASSERT(scales.cache_read_index < scales.cache_write_index, "Attempting to consume a constant sample that isn't cached");
				const uint32_t cache_read_index = scales.cache_read_index++;
				return scales.cached_samples[cache_read_index % 8];
#else
#if defined(ACL_IMPL_USE_CONSTANT_GROUPS)
				const rtm::vector4f scale = rtm::vector_load(constant_data_scales[0]);
				num_group_scales[0]--;
				num_unpacked_scales--;

				// If we finished reading from the first group, swap it out otherwise increment our entry
				if (num_group_scales[0] == 0)
				{
					constant_data_scales[0] = constant_data_scales[1];
					num_group_scales[0] = num_group_scales[1];
				}
				else
					constant_data_scales[0] += sizeof(rtm::float3f);
#else
				const rtm::vector4f scale = rtm::vector_load(constant_data_scales);
				constant_data_scales += sizeof(rtm::float3f);
#endif
				return scale;
#endif
			}
		};
	}
}

ACL_IMPL_FILE_PRAGMA_POP
