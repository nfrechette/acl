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

#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/track_writer.h"
#include "acl/core/variable_bit_rates.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/database/database.h"
#include "acl/math/scalar_packing.h"
#include "acl/math/vector4_packing.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C26451: Arithmetic overflow: Using operator '*' on a 4 byte value and then casting the result to a 8 byte value. Cast the value to the wider type before calling operator '*' to avoid overflow (io.2).
	// We can't overflow because compressed clips cannot contain more than 4 GB worth of data
	#pragma warning(disable : 26451)
#endif

namespace acl
{
	namespace acl_impl
	{
		struct alignas(64) persistent_scalar_decompression_context_v0
		{
			// Clip related data							//   offsets
			// Only member used to detect if we are initialized, must be first
			const compressed_tracks* tracks;				//   0 |   0

			uint32_t tracks_hash;							//   4 |   8

			float duration;									//   8 |  12

															// Seeking related data
			float interpolation_alpha;						//  12 |  16
			float sample_time;								//  16 |  20

			uint32_t key_frame_bit_offsets[2];				//  20 |  24	// Variable quantization

			uint8_t padding_tail[sizeof(void*) == 4 ? 36 : 32];

			//////////////////////////////////////////////////////////////////////////

			const compressed_tracks* get_compressed_tracks() const { return tracks; }
			compressed_tracks_version16 get_version() const { return tracks->get_version(); }
			bool is_initialized() const { return tracks != nullptr; }
			void reset() { tracks = nullptr; }
		};

		static_assert(sizeof(persistent_scalar_decompression_context_v0) == 64, "Unexpected size");

		template<class decompression_settings_type, class database_settings_type>
		inline bool initialize_v0(persistent_scalar_decompression_context_v0& context, const compressed_tracks& tracks, const database_context<database_settings_type>* database)
		{
			ACL_ASSERT(tracks.get_algorithm_type() == algorithm_type8::uniformly_sampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(tracks.get_algorithm_type()), get_algorithm_name(algorithm_type8::uniformly_sampled));

			if (database != nullptr)
				return false;	// Database decompression is not supported for scalar tracks

			context.tracks = &tracks;
			context.tracks_hash = tracks.get_hash();
			context.duration = tracks.get_duration();
			context.sample_time = -1.0F;
			context.interpolation_alpha = 0.0;

			return true;
		}

		inline bool is_dirty_v0(const persistent_scalar_decompression_context_v0& context, const compressed_tracks& tracks)
		{
			if (context.tracks != &tracks)
				return true;

			if (context.tracks_hash != tracks.get_hash())
				return true;

			return false;
		}

		template<class decompression_settings_type>
		inline void seek_v0(persistent_scalar_decompression_context_v0& context, float sample_time, sample_rounding_policy rounding_policy)
		{
			const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*context.tracks);
			if (header.num_samples == 0)
				return;	// Empty track list

			// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
			if (decompression_settings_type::clamp_sample_time())
				sample_time = rtm::scalar_clamp(sample_time, 0.0F, context.duration);

			if (context.sample_time == sample_time)
				return;

			context.sample_time = sample_time;

			uint32_t key_frame0;
			uint32_t key_frame1;
			find_linear_interpolation_samples_with_sample_rate(header.num_samples, header.sample_rate, sample_time, rounding_policy, key_frame0, key_frame1, context.interpolation_alpha);

			const acl_impl::scalar_tracks_header& scalars_header = acl_impl::get_scalar_tracks_header(*context.tracks);

			context.key_frame_bit_offsets[0] = key_frame0 * scalars_header.num_bits_per_frame;
			context.key_frame_bit_offsets[1] = key_frame1 * scalars_header.num_bits_per_frame;
		}

		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_tracks_v0(const persistent_scalar_decompression_context_v0& context, track_writer_type& writer)
		{
			const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*context.tracks);
			const uint32_t num_tracks = header.num_tracks;
			if (num_tracks == 0)
				return;	// Empty track list

			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");
			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			const acl_impl::scalar_tracks_header& scalars_header = acl_impl::get_scalar_tracks_header(*context.tracks);
			const rtm::scalarf interpolation_alpha = rtm::scalar_set(context.interpolation_alpha);

			const acl_impl::track_metadata* per_track_metadata = scalars_header.get_track_metadata();
			const float* constant_values = scalars_header.get_track_constant_values();
			const float* range_values = scalars_header.get_track_range_values();
			const uint8_t* animated_values = scalars_header.get_track_animated_values();

			uint32_t track_bit_offset0 = context.key_frame_bit_offsets[0];
			uint32_t track_bit_offset1 = context.key_frame_bit_offsets[1];

			const track_type8 track_type = header.track_type;

			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				const acl_impl::track_metadata& metadata = per_track_metadata[track_index];
				const uint8_t bit_rate = metadata.bit_rate;
				const uint32_t num_bits_per_component = get_num_bits_at_bit_rate(bit_rate);

				if (track_type == track_type8::float1f && decompression_settings_type::is_track_type_supported(track_type8::float1f))
				{
					rtm::scalarf value;
					if (is_constant_bit_rate(bit_rate))
					{
						value = rtm::scalar_load(constant_values);
						constant_values += 1;
					}
					else
					{
						rtm::scalarf value0;
						rtm::scalarf value1;
						if (is_raw_bit_rate(bit_rate))
						{
							value0 = unpack_scalarf_32_unsafe(animated_values, track_bit_offset0);
							value1 = unpack_scalarf_32_unsafe(animated_values, track_bit_offset1);
						}
						else
						{
							value0 = unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0);
							value1 = unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1);

							const rtm::scalarf range_min = rtm::scalar_load(range_values);
							const rtm::scalarf range_extent = rtm::scalar_load(range_values + 1);
							value0 = rtm::scalar_mul_add(value0, range_extent, range_min);
							value1 = rtm::scalar_mul_add(value1, range_extent, range_min);
							range_values += 2;
						}

						value = rtm::scalar_lerp(value0, value1, interpolation_alpha);

						const uint32_t num_sample_bits = num_bits_per_component;
						track_bit_offset0 += num_sample_bits;
						track_bit_offset1 += num_sample_bits;
					}

					writer.write_float1(track_index, value);
				}
				else if (track_type == track_type8::float2f && decompression_settings_type::is_track_type_supported(track_type8::float2f))
				{
					rtm::vector4f value;
					if (is_constant_bit_rate(bit_rate))
					{
						value = rtm::vector_load(constant_values);
						constant_values += 2;
					}
					else
					{
						rtm::vector4f value0;
						rtm::vector4f value1;
						if (is_raw_bit_rate(bit_rate))
						{
							value0 = unpack_vector2_64_unsafe(animated_values, track_bit_offset0);
							value1 = unpack_vector2_64_unsafe(animated_values, track_bit_offset1);
						}
						else
						{
							value0 = unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0);
							value1 = unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1);

							const rtm::vector4f range_min = rtm::vector_load(range_values);
							const rtm::vector4f range_extent = rtm::vector_load(range_values + 2);
							value0 = rtm::vector_mul_add(value0, range_extent, range_min);
							value1 = rtm::vector_mul_add(value1, range_extent, range_min);
							range_values += 4;
						}

						value = rtm::vector_lerp(value0, value1, interpolation_alpha);

						const uint32_t num_sample_bits = num_bits_per_component * 2;
						track_bit_offset0 += num_sample_bits;
						track_bit_offset1 += num_sample_bits;
					}

					writer.write_float2(track_index, value);
				}
				else if (track_type == track_type8::float3f && decompression_settings_type::is_track_type_supported(track_type8::float3f))
				{
					rtm::vector4f value;
					if (is_constant_bit_rate(bit_rate))
					{
						value = rtm::vector_load(constant_values);
						constant_values += 3;
					}
					else
					{
						rtm::vector4f value0;
						rtm::vector4f value1;
						if (is_raw_bit_rate(bit_rate))
						{
							value0 = unpack_vector3_96_unsafe(animated_values, track_bit_offset0);
							value1 = unpack_vector3_96_unsafe(animated_values, track_bit_offset1);
						}
						else
						{
							value0 = unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0);
							value1 = unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1);

							const rtm::vector4f range_min = rtm::vector_load(range_values);
							const rtm::vector4f range_extent = rtm::vector_load(range_values + 3);
							value0 = rtm::vector_mul_add(value0, range_extent, range_min);
							value1 = rtm::vector_mul_add(value1, range_extent, range_min);
							range_values += 6;
						}

						value = rtm::vector_lerp(value0, value1, interpolation_alpha);

						const uint32_t num_sample_bits = num_bits_per_component * 3;
						track_bit_offset0 += num_sample_bits;
						track_bit_offset1 += num_sample_bits;
					}

					writer.write_float3(track_index, value);
				}
				else if (track_type == track_type8::float4f && decompression_settings_type::is_track_type_supported(track_type8::float4f))
				{
					rtm::vector4f value;
					if (is_constant_bit_rate(bit_rate))
					{
						value = rtm::vector_load(constant_values);
						constant_values += 4;
					}
					else
					{
						rtm::vector4f value0;
						rtm::vector4f value1;
						if (is_raw_bit_rate(bit_rate))
						{
							value0 = unpack_vector4_128_unsafe(animated_values, track_bit_offset0);
							value1 = unpack_vector4_128_unsafe(animated_values, track_bit_offset1);
						}
						else
						{
							value0 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0);
							value1 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1);

							const rtm::vector4f range_min = rtm::vector_load(range_values);
							const rtm::vector4f range_extent = rtm::vector_load(range_values + 4);
							value0 = rtm::vector_mul_add(value0, range_extent, range_min);
							value1 = rtm::vector_mul_add(value1, range_extent, range_min);
							range_values += 8;
						}

						value = rtm::vector_lerp(value0, value1, interpolation_alpha);

						const uint32_t num_sample_bits = num_bits_per_component * 4;
						track_bit_offset0 += num_sample_bits;
						track_bit_offset1 += num_sample_bits;
					}

					writer.write_float4(track_index, value);
				}
				else if (track_type == track_type8::vector4f && decompression_settings_type::is_track_type_supported(track_type8::vector4f))
				{
					rtm::vector4f value;
					if (is_constant_bit_rate(bit_rate))
					{
						value = rtm::vector_load(constant_values);
						constant_values += 4;
					}
					else
					{
						rtm::vector4f value0;
						rtm::vector4f value1;
						if (is_raw_bit_rate(bit_rate))
						{
							value0 = unpack_vector4_128_unsafe(animated_values, track_bit_offset0);
							value1 = unpack_vector4_128_unsafe(animated_values, track_bit_offset1);
						}
						else
						{
							value0 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0);
							value1 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1);

							const rtm::vector4f range_min = rtm::vector_load(range_values);
							const rtm::vector4f range_extent = rtm::vector_load(range_values + 4);
							value0 = rtm::vector_mul_add(value0, range_extent, range_min);
							value1 = rtm::vector_mul_add(value1, range_extent, range_min);
							range_values += 8;
						}

						value = rtm::vector_lerp(value0, value1, interpolation_alpha);

						const uint32_t num_sample_bits = num_bits_per_component * 4;
						track_bit_offset0 += num_sample_bits;
						track_bit_offset1 += num_sample_bits;
					}

					writer.write_vector4(track_index, value);
				}
			}

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_track_v0(const persistent_scalar_decompression_context_v0& context, uint32_t track_index, track_writer_type& writer)
		{
			const tracks_header& header = get_tracks_header(*context.tracks);
			if (header.num_tracks == 0)
				return;	// Empty track list

			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");
			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			ACL_ASSERT(track_index < header.num_tracks, "Invalid track index");
			if (track_index >= header.num_tracks)
				return;	// Invalid track index

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			const scalar_tracks_header& scalars_header = get_scalar_tracks_header(*context.tracks);
			const rtm::scalarf interpolation_alpha = rtm::scalar_set(context.interpolation_alpha);

			const float* constant_values = scalars_header.get_track_constant_values();
			const float* range_values = scalars_header.get_track_range_values();

			const track_type8 track_type = header.track_type;
			const uint32_t num_element_components = get_track_num_sample_elements(track_type);
			uint32_t track_bit_offset = 0;

			const acl_impl::track_metadata* per_track_metadata = scalars_header.get_track_metadata();
			for (uint32_t scan_track_index = 0; scan_track_index < track_index; ++scan_track_index)
			{
				const acl_impl::track_metadata& metadata = per_track_metadata[scan_track_index];
				const uint8_t bit_rate = metadata.bit_rate;
				const uint32_t num_bits_per_component = get_num_bits_at_bit_rate(bit_rate);
				track_bit_offset += num_bits_per_component * num_element_components;

				if (is_constant_bit_rate(bit_rate))
					constant_values += num_element_components;
				else if (!is_raw_bit_rate(bit_rate))
					range_values += num_element_components * 2;
			}

			const acl_impl::track_metadata& metadata = per_track_metadata[track_index];
			const uint8_t bit_rate = metadata.bit_rate;
			const uint32_t num_bits_per_component = get_num_bits_at_bit_rate(bit_rate);

			const uint8_t* animated_values = scalars_header.get_track_animated_values();

			if (track_type == track_type8::float1f && decompression_settings_type::is_track_type_supported(track_type8::float1f))
			{
				rtm::scalarf value;
				if (is_constant_bit_rate(bit_rate))
					value = rtm::scalar_load(constant_values);
				else
				{
					rtm::scalarf value0;
					rtm::scalarf value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = unpack_scalarf_32_unsafe(animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_scalarf_32_unsafe(animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);
					}
					else
					{
						value0 = unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);

						const rtm::scalarf range_min = rtm::scalar_load(range_values);
						const rtm::scalarf range_extent = rtm::scalar_load(range_values + num_element_components);
						value0 = rtm::scalar_mul_add(value0, range_extent, range_min);
						value1 = rtm::scalar_mul_add(value1, range_extent, range_min);
					}

					value = rtm::scalar_lerp(value0, value1, interpolation_alpha);
				}

				writer.write_float1(track_index, value);
			}
			else if (track_type == track_type8::float2f && decompression_settings_type::is_track_type_supported(track_type8::float2f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
					value = rtm::vector_load(constant_values);
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = unpack_vector2_64_unsafe(animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector2_64_unsafe(animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);
					}
					else
					{
						value0 = unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
					}

					value = rtm::vector_lerp(value0, value1, interpolation_alpha);
				}

				writer.write_float2(track_index, value);
			}
			else if (track_type == track_type8::float3f && decompression_settings_type::is_track_type_supported(track_type8::float3f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
					value = rtm::vector_load(constant_values);
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = unpack_vector3_96_unsafe(animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector3_96_unsafe(animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);
					}
					else
					{
						value0 = unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
					}

					value = rtm::vector_lerp(value0, value1, interpolation_alpha);
				}

				writer.write_float3(track_index, value);
			}
			else if (track_type == track_type8::float4f && decompression_settings_type::is_track_type_supported(track_type8::float4f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
					value = rtm::vector_load(constant_values);
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = unpack_vector4_128_unsafe(animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector4_128_unsafe(animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);
					}
					else
					{
						value0 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
					}

					value = rtm::vector_lerp(value0, value1, interpolation_alpha);
				}

				writer.write_float4(track_index, value);
			}
			else if (track_type == track_type8::vector4f && decompression_settings_type::is_track_type_supported(track_type8::vector4f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
					value = rtm::vector_load(constant_values);
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = unpack_vector4_128_unsafe(animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector4_128_unsafe(animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);
					}
					else
					{
						value0 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[0] + track_bit_offset);
						value1 = unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, context.key_frame_bit_offsets[1] + track_bit_offset);

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
					}

					value = rtm::vector_lerp(value0, value1, interpolation_alpha);
				}

				writer.write_vector4(track_index, value);
			}

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}
	}
}

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif

ACL_IMPL_FILE_PRAGMA_POP
