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

#include "acl/core/bitset.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_formats.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		struct alignas(64) persistent_transform_decompression_context_v0
		{
			// Clip related data							//   offsets
			// Only member used to detect if we are initialized, must be first
			const compressed_tracks* tracks;				//   0 |   0

			const uint32_t* constant_tracks_bitset;			//   4 |   8
			const uint8_t* constant_track_data;				//   8 |  16
			const uint32_t* default_tracks_bitset;			//  12 |  24

			const uint8_t* clip_range_data;					//  16 |  32

			float clip_duration;							//  20 |  40

			bitset_description bitset_desc;					//  24 |  44

			uint32_t clip_hash;								//  28 |  48

			rotation_format8 rotation_format;				//  32 |  52
			vector_format8 translation_format;				//  33 |  53
			vector_format8 scale_format;					//  34 |  54
			range_reduction_flags8 range_reduction;			//  35 |  55

			uint8_t num_rotation_components;				//  36 |  56
			uint8_t has_segments;							//  37 |  57

			uint8_t padding0[2];							//  38 |  58

															// Seeking related data
			float sample_time;								//  40 |  60

			const uint8_t* format_per_track_data[2];		//  44 |  64
			const uint8_t* segment_range_data[2];			//  52 |  80
			const uint8_t* animated_track_data[2];			//  60 |  96

			uint32_t key_frame_bit_offsets[2];				//  68 | 112

			float interpolation_alpha;						//  76 | 120

			uint8_t padding1[sizeof(void*) == 4 ? 48 : 4];	//  80 | 124

															//									Total size:	   128 | 128

															//////////////////////////////////////////////////////////////////////////

			const compressed_tracks* get_compressed_tracks() const { return tracks; }
			compressed_tracks_version16 get_version() const { return tracks->get_version(); }
			bool is_initialized() const { return tracks != nullptr; }
			void reset() { tracks = nullptr; }
		};

		static_assert(sizeof(persistent_transform_decompression_context_v0) == 128, "Unexpected size");

		// We use adapters to wrap the decompression_settings
		// This allows us to re-use the code for skipping and decompressing Vector3 samples
		// Code generation will generate specialized code for each specialization
		template<class decompression_settings_type>
		struct translation_decompression_settings_adapter
		{
			// Forward to our decompression settings
			static constexpr range_reduction_flags8 get_range_reduction_flag() { return range_reduction_flags8::translations; }
			static constexpr vector_format8 get_vector_format(const persistent_transform_decompression_context_v0& context) { return context.translation_format; }
			static constexpr bool is_vector_format_supported(vector_format8 format) { return decompression_settings_type::is_translation_format_supported(format); }
		};

		template<class decompression_settings_type>
		struct scale_decompression_settings_adapter
		{
			// Forward to our decompression settings
			static constexpr range_reduction_flags8 get_range_reduction_flag() { return range_reduction_flags8::scales; }
			static constexpr vector_format8 get_vector_format(const persistent_transform_decompression_context_v0& context) { return context.scale_format; }
			static constexpr bool is_vector_format_supported(vector_format8 format) { return decompression_settings_type::is_scale_format_supported(format); }
		};

		// Returns the statically known number of rotation formats supported by the decompression settings
		template<class decompression_settings_type>
		constexpr int32_t num_supported_rotation_formats()
		{
			return decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full)
				+ decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_full)
				+ decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable);
		}

		// Returns the statically known rotation format supported if we only support one, otherwise we return the input value
		// which might not be known statically
		template<class decompression_settings_type>
		constexpr rotation_format8 get_rotation_format(rotation_format8 format)
		{
			return num_supported_rotation_formats<decompression_settings_type>() > 1
				// More than one format is supported, return the input value, whatever it may be
				? format
				// Only one format is supported, figure out statically which one it is and return it
				: (decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full) ? rotation_format8::quatf_full
					: (decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_full) ? rotation_format8::quatf_drop_w_full
						: rotation_format8::quatf_drop_w_variable));
		}

		// Returns the statically known number of vector formats supported by the decompression settings
		template<class decompression_settings_adapter_type>
		constexpr int32_t num_supported_vector_formats()
		{
			return decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full)
				+ decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable);
		}

		// Returns the statically known vector format supported if we only support one, otherwise we return the input value
		// which might not be known statically
		template<class decompression_settings_adapter_type>
		constexpr vector_format8 get_vector_format(vector_format8 format)
		{
			return num_supported_vector_formats<decompression_settings_adapter_type>() > 1
				// More than one format is supported, return the input value, whatever it may be
				? format
				// Only one format is supported, figure out statically which one it is and return it
				: (decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full) ? vector_format8::vector3f_full
					: vector_format8::vector3f_variable);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
