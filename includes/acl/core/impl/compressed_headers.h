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

#include "acl/core/algorithm_types.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/ptr_offset.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	class compressed_tracks;

	namespace acl_impl
	{
		// Common header to all binary formats
		struct raw_buffer_header
		{
			// Total size in bytes of the raw buffer.
			uint32_t		size;

			// Hash of the raw buffer.
			uint32_t		hash;
		};

		// Header for 'compressed_tracks'
		struct tracks_header
		{
			// Serialization tag used to distinguish raw buffer types.
			uint32_t						tag;

			// Serialization version used to compress the tracks.
			compressed_tracks_version16		version;

			// Algorithm type used to compress the tracks.
			algorithm_type8					algorithm_type;

			// Type of the tracks contained in this compressed stream.
			track_type8						track_type;

			// The total number of tracks.
			uint32_t						num_tracks;

			// The total number of samples per track.
			uint32_t						num_samples;

			// The sample rate our tracks use.
			float							sample_rate;					// TODO: Store duration as float instead?

			// Miscellaneous packed values
			uint32_t						misc_packed;

			//////////////////////////////////////////////////////////////////////////
			// Accessors for 'misc_packed'

			// Scalar tracks use it like this (listed from LSB):
			// Bits [0, 31): unused (31 bits)
			// Bit [31, 32): has metadata?

			// Transform tracks use it like this (listed from LSB):
			// Bit 0: has scale?
			// Bit 1: default scale: 0,0,0 or 1,1,1 (bool/bit)
			// Bit 2: scale format
			// Bit 3: translation format
			// Bits [4, 8): rotation format (4 bits)
			// Bits [8, 31): unused (23 bits)
			// Bit [31, 32): has metadata?

			rotation_format8 get_rotation_format() const { return static_cast<rotation_format8>((misc_packed >> 4) & 15); }
			void set_rotation_format(rotation_format8 format) { misc_packed = (misc_packed & ~(15 << 4)) | (static_cast<uint32_t>(format) << 4); }
			vector_format8 get_translation_format() const { return static_cast<vector_format8>((misc_packed >> 3) & 1); }
			void set_translation_format(vector_format8 format) { misc_packed = (misc_packed & ~(1 << 3)) | (static_cast<uint32_t>(format) << 3); }
			vector_format8 get_scale_format() const { return static_cast<vector_format8>((misc_packed >> 2) & 1); }
			void set_scale_format(vector_format8 format) { misc_packed = (misc_packed & ~(1 << 2)) | (static_cast<uint32_t>(format) << 2); }
			int32_t get_default_scale() const { return (misc_packed >> 1) & 1; }
			void set_default_scale(uint32_t scale) { ACL_ASSERT(scale == 0 || scale == 1, "Invalid default scale"); misc_packed = (misc_packed & ~(1 << 1)) | (scale << 1); }
			bool get_has_scale() const { return (misc_packed & 1) != 0; }
			void set_has_scale(bool has_scale) { misc_packed = (misc_packed & ~1) | static_cast<uint32_t>(has_scale); }
			bool get_has_metadata() const { return (misc_packed >> 31) != 0; }
			void set_has_metadata(bool has_metadata) { misc_packed = (misc_packed & ~(1 << 31)) | (static_cast<uint32_t>(has_metadata) << 31); }
		};

		// Scalar track metadata
		struct track_metadata
		{
			uint8_t			bit_rate;
		};

		// Header for scalar 'compressed_tracks'
		struct scalar_tracks_header
		{
			// The number of bits used for a whole frame of data.
			// The sum of one sample per track with all bit rates taken into account.
			uint32_t						num_bits_per_frame;

			// Various data offsets relative to the start of this header.
			ptr_offset32<track_metadata>	metadata_per_track;
			ptr_offset32<float>				track_constant_values;
			ptr_offset32<float>				track_range_values;
			ptr_offset32<uint8_t>			track_animated_values;

			//////////////////////////////////////////////////////////////////////////

			track_metadata*					get_track_metadata() { return metadata_per_track.add_to(this); }
			const track_metadata*			get_track_metadata() const { return metadata_per_track.add_to(this); }

			float*							get_track_constant_values() { return track_constant_values.add_to(this); }
			const float*					get_track_constant_values() const { return track_constant_values.add_to(this); }

			float*							get_track_range_values() { return track_range_values.add_to(this); }
			const float*					get_track_range_values() const { return track_range_values.add_to(this); }

			uint8_t*						get_track_animated_values() { return track_animated_values.add_to(this); }
			const uint8_t*					get_track_animated_values() const { return track_animated_values.add_to(this); }
		};

		////////////////////////////////////////////////////////////////////////////////
		// A compressed clip segment header. Each segment is built from a uniform number
		// of samples per track. A clip is split into one or more segments.
		////////////////////////////////////////////////////////////////////////////////
		struct segment_header
		{
			// Number of bits used by a fully animated pose (excludes default/constant tracks).
			uint32_t						animated_pose_bit_size;				// TODO: Calculate from bitsets and formats?

			// TODO: Only need one offset, calculate the others from the information we have?
			// Offset to the per animated track format data.
			ptr_offset32<uint8_t>			format_per_track_data_offset;		// TODO: Make this offset optional? Only present if variable

			// Offset to the segment range data.
			ptr_offset32<uint8_t>			range_data_offset;					// TODO: Make this offset optional? Only present if normalized

			// Offset to the segment animated tracks data.
			ptr_offset32<uint8_t>			track_data_offset;
		};

		// Header for transform 'compressed_tracks'
		struct transform_tracks_header
		{
			// The number of segments contained.
			uint16_t						num_segments;

			// Offset to the segment headers data.
			ptr_offset16<uint32_t>			segment_start_indices_offset;
			ptr_offset32<segment_header>	segment_headers_offset;

			// Offsets to the default/constant tracks bitsets.
			ptr_offset32<uint32_t>			default_tracks_bitset_offset;
			ptr_offset32<uint32_t>			constant_tracks_bitset_offset;

			// Offset to the constant tracks data.
			ptr_offset32<uint8_t>			constant_track_data_offset;

			// Offset to the clip range data.
			ptr_offset32<uint8_t>			clip_range_data_offset;				// TODO: Make this offset optional? Only present if normalized

			//////////////////////////////////////////////////////////////////////////
			// Utility functions that return pointers from their respective offsets.

			uint32_t*					get_segment_start_indices() { return segment_start_indices_offset.safe_add_to(this); }
			const uint32_t*				get_segment_start_indices() const { return segment_start_indices_offset.safe_add_to(this); }

			segment_header*				get_segment_headers() { return segment_headers_offset.add_to(this); }
			const segment_header*		get_segment_headers() const { return segment_headers_offset.add_to(this); }

			uint32_t*					get_default_tracks_bitset() { return default_tracks_bitset_offset.add_to(this); }
			const uint32_t*				get_default_tracks_bitset() const { return default_tracks_bitset_offset.add_to(this); }

			uint32_t*					get_constant_tracks_bitset() { return constant_tracks_bitset_offset.add_to(this); }
			const uint32_t*				get_constant_tracks_bitset() const { return constant_tracks_bitset_offset.add_to(this); }

			uint8_t*					get_constant_track_data() { return constant_track_data_offset.safe_add_to(this); }
			const uint8_t*				get_constant_track_data() const { return constant_track_data_offset.safe_add_to(this); }

			uint8_t*					get_format_per_track_data(const segment_header& header) { return header.format_per_track_data_offset.safe_add_to(this); }
			const uint8_t*				get_format_per_track_data(const segment_header& header) const { return header.format_per_track_data_offset.safe_add_to(this); }

			uint8_t*					get_clip_range_data() { return clip_range_data_offset.safe_add_to(this); }
			const uint8_t*				get_clip_range_data() const { return clip_range_data_offset.safe_add_to(this); }

			uint8_t*					get_track_data(const segment_header& header) { return header.track_data_offset.safe_add_to(this); }
			const uint8_t*				get_track_data(const segment_header& header) const { return header.track_data_offset.safe_add_to(this); }

			uint8_t*					get_segment_range_data(const segment_header& header) { return header.range_data_offset.safe_add_to(this); }
			const uint8_t*				get_segment_range_data(const segment_header& header) const { return header.range_data_offset.safe_add_to(this); }
		};

		// Header for optional track metadata, must be at least 15 bytes
		struct optional_metadata_header
		{
			ptr_offset32<char>				track_list_name;
			ptr_offset32<uint32_t>			track_name_offsets;
			ptr_offset32<uint32_t>			parent_track_indices;
			ptr_offset32<uint8_t>			track_descriptions;

			//////////////////////////////////////////////////////////////////////////
			// Utility functions that return pointers from their respective offsets.

			char*						get_track_list_name(compressed_tracks& tracks) { return track_list_name.safe_add_to(&tracks); }
			const char*					get_track_list_name(const compressed_tracks& tracks) const { return track_list_name.safe_add_to(&tracks); }
			uint32_t*					get_track_name_offsets(compressed_tracks& tracks) { return track_name_offsets.safe_add_to(&tracks); }
			const uint32_t*				get_track_name_offsets(const compressed_tracks& tracks) const { return track_name_offsets.safe_add_to(&tracks); }
			uint32_t*					get_parent_track_indices(compressed_tracks& tracks) { return parent_track_indices.safe_add_to(&tracks); }
			const uint32_t*				get_parent_track_indices(const compressed_tracks& tracks) const { return parent_track_indices.safe_add_to(&tracks); }
			uint8_t*					get_track_descriptions(compressed_tracks& tracks) { return track_descriptions.safe_add_to(&tracks); }
			const uint8_t*				get_track_descriptions(const compressed_tracks& tracks) const { return track_descriptions.safe_add_to(&tracks); }
		};

		static_assert(sizeof(optional_metadata_header) >= 15, "Optional metadata must be at least 15 bytes");
	}
}

ACL_IMPL_FILE_PRAGMA_POP
