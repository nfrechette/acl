#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/memory_utils.h"
#include "acl/core/impl/bit_cast.impl.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/impl/decompression_context.transform.h"
#include "acl/decompression/impl/steps/animated_unpack_config.h"
#include "acl/decompression/impl/steps/animated_unpack_types.h"
#include "acl/math/scalar_packing.h"

#include <rtm/math.h>
#include <rtm/vector4f.h>

#include <cstdint>

// A uniform scale (vqsf) stores a single component for each scale sub-track:
//    - The constant bit rate sample in the segment range data: 16 bits (2 bytes)
//    - The segment range data: min.x and extent.x with 8 bits each (2 bytes)
//    - The clip range data: min.x and extent.x as floats (8 bytes)
//    - The animated sample: 'num_bits' bits, or 32 bits when raw or with full precision
// The compressor keeps the same value in [xyz], see 'transform_track_utils.h'.
// To return the same value as a vector3 sub-track would, we use the same vector math.
// The functions below return the uniform scale in [xyzw].

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// The size in bytes of the segment range data of a uniform scale sub-track.
		constexpr uint32_t k_uniform_scale_segment_range_data_size = 2;

		//////////////////////////////////////////////////////////////////////////
		// The size in bytes of the clip range data of a uniform scale sub-track.
		constexpr uint32_t k_uniform_scale_clip_range_data_size = sizeof(float) * 2;

		//////////////////////////////////////////////////////////////////////////
		// Unpacks a uniform scale sample.
		// Returns the value in [xyzw] and the number of bits of the animated sample.
		// 'num_bits_at_bit_rate' is the value in the per track metadata, or 32 with full precision.
		inline RTM_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL unpack_uniform_scale_sample(
			uint32_t num_bits_at_bit_rate,
			bool has_segments,
			bool is_variable,
			const uint8_t* segment_range_data,
			const uint8_t* animated_track_data,
			uint32_t animated_track_data_bit_offset,
			const uint8_t* clip_range_data,
			uint32_t& out_num_animated_bits)
		{
			if (!is_variable)
			{
				out_num_animated_bits = 32;
				return rtm::vector_set(unpack_scalarf_32_unsafe(animated_track_data, animated_track_data_bit_offset));
			}

			rtm::vector4f sample;
			if (num_bits_at_bit_rate == 0) ACL_BRANCH_UNLIKELY
			{
				// Constant bit rate: the sample is in the segment range data, 16 bits
				const uint16_t sample_u16 = unaligned_load<uint16_t>(segment_range_data);
				sample = rtm::vector_mul(rtm::vector_set(float(sample_u16)), 1.0F / 65535.0F);
				out_num_animated_bits = 0;
			}
			else
			{
				// Remap 31 to 32
				// See write_format_per_track_data(..) for details
				num_bits_at_bit_rate = num_bits_at_bit_rate >= 31 ? 32 : num_bits_at_bit_rate;

				if (num_bits_at_bit_rate == 32)
					sample = rtm::vector_set(unpack_scalarf_32_unsafe(animated_track_data, animated_track_data_bit_offset));
				else
					sample = rtm::vector_set(unpack_scalarf_uXX_unsafe(num_bits_at_bit_rate, animated_track_data, animated_track_data_bit_offset));

				out_num_animated_bits = num_bits_at_bit_rate;
			}

			// If we aren't raw or constant (32 or 0 bits)
			if (has_segments && (num_bits_at_bit_rate & 0x1F) != 0)
			{
				// Apply segment range remapping
				const rtm::vector4f segment_range_min = rtm::vector_mul(rtm::vector_set(float(segment_range_data[0])), 1.0F / 255.0F);
				const rtm::vector4f segment_range_extent = rtm::vector_mul(rtm::vector_set(float(segment_range_data[1])), 1.0F / 255.0F);

				sample = rtm::vector_mul_add(sample, segment_range_extent, segment_range_min);
			}

			// If we aren't raw (32 bits)
			if ((num_bits_at_bit_rate & 0x20) == 0)
			{
				// Apply clip range remapping
				const rtm::vector4f clip_range_min = rtm::vector_broadcast(bit_cast<const float*>(clip_range_data));
				const rtm::vector4f clip_range_extent = rtm::vector_broadcast(bit_cast<const float*>(clip_range_data + sizeof(float)));

				sample = rtm::vector_mul_add(sample, clip_range_extent, clip_range_min);
			}

			return sample;
		}

		//////////////////////////////////////////////////////////////////////////
		// Unpacks a group of up to 4 uniform scale samples. Each value is in [xyzw].
		// The caller moves the clip range data pointer, like with 'unpack_grouped_animated_vector3'.
		template<class decompression_settings_adapter_type>
		ACL_IMPL_DEBUG_FORCE_INLINE
		RTM_DISABLE_SECURITY_COOKIE_CHECK
		void unpack_grouped_animated_uniform_scale(
			const persistent_transform_decompression_context_v0& decomp_context,
			rtm::vector4f output_scratch[4],
			uint32_t num_to_unpack,
			const clip_animated_sampling_context_v0& clip_sampling_context,
			segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));
			const bool is_variable = format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable);
			const bool has_segments = decomp_context.has_segments;

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

			for (uint32_t unpack_index = 0; unpack_index < num_to_unpack; ++unpack_index)
			{
				uint32_t num_bits_at_bit_rate = 32;
				if (is_variable)
				{
					num_bits_at_bit_rate = *format_per_track_data;
					format_per_track_data++;
				}

				uint32_t num_animated_bits;
				const rtm::vector4f sample = unpack_uniform_scale_sample(num_bits_at_bit_rate, has_segments, is_variable, segment_range_data, animated_track_data, animated_track_data_bit_offset, clip_range_data, num_animated_bits);

				ACL_ASSERT(rtm::vector_is_finite3(sample), "Scale is not valid!");

				animated_track_data_bit_offset += num_animated_bits;

				if (is_variable)
				{
					if (has_segments)
						segment_range_data += k_uniform_scale_segment_range_data_size;

					clip_range_data += k_uniform_scale_clip_range_data_size;
				}

				output_scratch[unpack_index] = sample;
			}

			// Update our pointers
			segment_sampling_context.format_per_track_data = format_per_track_data;
			segment_sampling_context.segment_range_data = segment_range_data;
			segment_sampling_context.animated_track_data_bit_offset = animated_track_data_bit_offset;

			// Prefetch the next cache line even if we don't have any data left
			// By the time we unpack again, it will have arrived in the CPU cache
			ACL_IMPL_ANIMATED_PREFETCH(format_per_track_data + 60);
			ACL_IMPL_ANIMATED_PREFETCH(animated_track_data + (animated_track_data_bit_offset / 8) + 63);
			ACL_IMPL_ANIMATED_PREFETCH(segment_range_data + 48);
		}

		//////////////////////////////////////////////////////////////////////////
		// Unpacks a single uniform scale sample within a group. The value is in [xyzw].
		template<class decompression_settings_adapter_type>
		inline RTM_DISABLE_SECURITY_COOKIE_CHECK rtm::vector4f RTM_SIMD_CALL
		unpack_single_animated_uniform_scale(
			const persistent_transform_decompression_context_v0& decomp_context,
			uint32_t unpack_index,
			const clip_animated_sampling_context_v0& clip_sampling_context,
			const segment_animated_sampling_context_v0& segment_sampling_context)
		{
			const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));
			const bool is_variable = format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable);
			const bool has_segments = decomp_context.has_segments;

			const uint8_t* format_per_track_data = segment_sampling_context.format_per_track_data;
			const uint8_t* segment_range_data = segment_sampling_context.segment_range_data;
			const uint8_t* animated_track_data = segment_sampling_context.animated_track_data;
			uint32_t animated_track_data_bit_offset = segment_sampling_context.animated_track_data_bit_offset;

			const uint8_t* clip_range_data = clip_sampling_context.clip_range_data;

			uint32_t num_bits_at_bit_rate = 32;
			if (is_variable)
			{
				// Skip prior samples
				for (uint32_t skip_index = 0; skip_index < unpack_index; ++skip_index)
				{
					const uint32_t num_bits = format_per_track_data[skip_index];
					animated_track_data_bit_offset += num_bits >= 31 ? 32 : num_bits;
				}

				if (has_segments)
					segment_range_data += k_uniform_scale_segment_range_data_size * unpack_index;

				clip_range_data += k_uniform_scale_clip_range_data_size * unpack_index;

				num_bits_at_bit_rate = format_per_track_data[unpack_index];
			}
			else
				animated_track_data_bit_offset += unpack_index * 32;

			uint32_t num_animated_bits;
			const rtm::vector4f sample = unpack_uniform_scale_sample(num_bits_at_bit_rate, has_segments, is_variable, segment_range_data, animated_track_data, animated_track_data_bit_offset, clip_range_data, num_animated_bits);
			(void)num_animated_bits;

			ACL_ASSERT(rtm::vector_is_finite3(sample), "Scale is not valid!");
			return sample;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
