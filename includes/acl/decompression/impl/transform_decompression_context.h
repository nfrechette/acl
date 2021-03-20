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
#include "acl/decompression/database/impl/database_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C26495: Variable '...' is uninitialized. Always initialize a member variable (type.6).
	// We explicitly control initialization
	#pragma warning(disable : 26495)
#endif

namespace acl
{
	namespace acl_impl
	{
		struct alignas(64) persistent_transform_decompression_context_v0
		{
			// Clip related data								//   offsets
			// Only member used to detect if we are initialized, must be first
			const compressed_tracks* tracks;					//   0 |   0

			// Database context, optional
			const database_context_v0* db;						//   4 |   8

			uint32_t clip_hash;									//   8 |  16

			float clip_duration;								//  12 |  20

			rotation_format8 rotation_format;					//  16 |  24
			vector_format8 translation_format;					//  17 |  25
			vector_format8 scale_format;						//  18 |  26

			uint8_t has_scale;									//  19 |  27
			uint8_t has_segments;								//  20 |  28

			uint8_t padding0[22];								//  21 |  29

			// Seeking related data
			uint8_t uses_single_segment;						//  43 |  51

			float sample_time;									//  44 |  52

			// Offsets relative to the 'tracks' pointer
			ptr_offset32<segment_header> segment_offsets[2];	//  48 |  56

			const uint8_t* format_per_track_data[2];			//  56 |  64
			const uint8_t* segment_range_data[2];				//  64 |  80
			const uint8_t* animated_track_data[2];				//  72 |  96

			// Offsets relative to the 'animated_track_data' pointers
			uint32_t key_frame_bit_offsets[2];					//  80 | 112

			float interpolation_alpha;							//  88 | 120

			uint8_t padding1[sizeof(void*) == 4 ? 36 : 4];		//  92 | 124

			//										Total size:	   128 | 128

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
			return int32_t(decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
				+ int32_t(decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_full))
				+ int32_t(decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable));
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
			return int32_t(decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full))
				+ int32_t(decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable));
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

		// Returns whether or not we should interpolate our samples
		// If we support more than one format, we'll assume that we always need to interpolate
		// and if we support only the raw format, we'll check if we need to based on the interpolation alpha
		template<class decompression_settings_type>
		constexpr bool should_interpolate_samples(rotation_format8 format, float interpolation_alpha)
		{
			return num_supported_rotation_formats<decompression_settings_type>() > 1
				// More than one format is supported, always interpolate
				? true
				// If our format is raw, only interpolate if our alpha isn't <= 0.0 or >= 1.0
				// otherwise we always interpolate
				: format == rotation_format8::quatf_full ? (interpolation_alpha > 0.0F && interpolation_alpha < 1.0F) : true;
		}
	}
}

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif

ACL_IMPL_FILE_PRAGMA_POP
