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
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_formats.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

// This is a bit slower because of the added bookkeeping when we unpack
//#define ACL_IMPL_USE_CONSTANT_GROUPS

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
			uint32_t						animated_pose_bit_size;

			// Offset to the animated segment data
			// Segment data is partitioned as follows:
			//    - format per variable track (no alignment)
			//    - range data per variable track (only when more than one segment) (2 byte alignment)
			//    - track data sorted per sample then per track (4 byte alignment)
			ptr_offset32<uint8_t>			segment_data;
		};

		//////////////////////////////////////////////////////////////////////////
		// A packed structure with metadata for animated groups.
		//////////////////////////////////////////////////////////////////////////
		struct animated_group_metadata
		{
			// Bits [0, 14): the group size
			// Bits [14, 16): the group type
			uint16_t						metadata;

			bool							is_valid() const { return metadata != 0xFFFF; }

			animation_track_type8			get_type() const { return static_cast<animation_track_type8>(metadata >> 6); }
			void							set_type(animation_track_type8 type) { metadata = (metadata & ~(3 << 6)) | static_cast<uint16_t>(type) << 6; }

			uint32_t						get_size() const { return static_cast<uint32_t>(metadata) & ((1 << 14) - 1); }
			void							set_size(uint32_t size) { ACL_ASSERT(size < (1 << 14), "Group size too large"); metadata = (metadata & ~((1 << 14) - 1)) | static_cast<uint16_t>(size); }
		};

		// Header for transform 'compressed_tracks'
		struct transform_tracks_header
		{
			// The number of segments contained.
			uint32_t						num_segments;

			// The number of animated rot/trans/scale tracks.
			uint32_t						num_animated_variable_sub_tracks;		// Might be padded with dummy tracks for alignment
			uint32_t						num_animated_rotation_sub_tracks;
			uint32_t						num_animated_translation_sub_tracks;
			uint32_t						num_animated_scale_sub_tracks;			// TODO: Not needed?

			// The number of constant sub-track samples stored, does not include default samples
			uint32_t						num_constant_rotation_samples;
			uint32_t						num_constant_translation_samples;
			uint32_t						num_constant_scale_samples;			// TODO: Not needed?

			// Offset to the segment headers data.
			ptr_offset32<segment_header>	segment_headers_offset;

			// Offsets to the default/constant tracks bitsets.
			ptr_offset32<uint32_t>			default_tracks_bitset_offset;
			ptr_offset32<uint32_t>			constant_tracks_bitset_offset;

			// Offset to the constant tracks data.
			ptr_offset32<uint8_t>			constant_track_data_offset;

			// Offset to the clip range data.
			ptr_offset32<uint8_t>			clip_range_data_offset;				// TODO: Make this offset optional? Only present if normalized

			// Offset to the animated group types. Ends with an invalid group type of 0xFF.
			ptr_offset32<animation_track_type8>	animated_group_types_offset;

			//////////////////////////////////////////////////////////////////////////
			// Utility functions that return pointers from their respective offsets.

			uint32_t*					get_segment_start_indices() { return num_segments > 1 ? add_offset_to_ptr<uint32_t>(this, align_to(sizeof(transform_tracks_header), 4)) : 0; }
			const uint32_t*				get_segment_start_indices() const { return num_segments > 1 ? add_offset_to_ptr<const uint32_t>(this, align_to(sizeof(transform_tracks_header), 4)) : 0; }

			segment_header*				get_segment_headers() { return segment_headers_offset.add_to(this); }
			const segment_header*		get_segment_headers() const { return segment_headers_offset.add_to(this); }

			animation_track_type8*			get_animated_group_types() { return animated_group_types_offset.add_to(this); }
			const animation_track_type8*	get_animated_group_types() const { return animated_group_types_offset.add_to(this); }

			uint32_t*					get_default_tracks_bitset() { return default_tracks_bitset_offset.add_to(this); }
			const uint32_t*				get_default_tracks_bitset() const { return default_tracks_bitset_offset.add_to(this); }

			uint32_t*					get_constant_tracks_bitset() { return constant_tracks_bitset_offset.add_to(this); }
			const uint32_t*				get_constant_tracks_bitset() const { return constant_tracks_bitset_offset.add_to(this); }

			uint8_t*					get_constant_track_data() { return constant_track_data_offset.safe_add_to(this); }
			const uint8_t*				get_constant_track_data() const { return constant_track_data_offset.safe_add_to(this); }

			uint8_t*					get_clip_range_data() { return clip_range_data_offset.safe_add_to(this); }
			const uint8_t*				get_clip_range_data() const { return clip_range_data_offset.safe_add_to(this); }

			void						get_segment_data(const segment_header& header, uint8_t*& out_format_per_track_data, uint8_t*& out_range_data, uint8_t*& out_animated_data)
			{
				uint8_t* segment_data = header.segment_data.add_to(this);

				uint8_t* format_per_track_data = segment_data;

				uint8_t* range_data = align_to(format_per_track_data + num_animated_variable_sub_tracks, 2);
				const uint32_t range_data_size = num_segments > 1 ? (k_segment_range_reduction_num_bytes_per_component * 6 * num_animated_variable_sub_tracks) : 0;

				uint8_t* animated_data = align_to(range_data + range_data_size, 4);

				out_format_per_track_data = format_per_track_data;
				out_range_data = range_data;
				out_animated_data = animated_data;
			}

			void						get_segment_data(const segment_header& header, const uint8_t*& out_format_per_track_data, const uint8_t*& out_range_data, const uint8_t*& out_animated_data) const
			{
				const uint8_t* segment_data = header.segment_data.add_to(this);

				const uint8_t* format_per_track_data = segment_data;

				const uint8_t* range_data = align_to(format_per_track_data + num_animated_variable_sub_tracks, 2);
				const uint32_t range_data_size = num_segments > 1 ? (k_segment_range_reduction_num_bytes_per_component * 6 * num_animated_variable_sub_tracks) : 0;

				const uint8_t* animated_data = align_to(range_data + range_data_size, 4);

				out_format_per_track_data = format_per_track_data;
				out_range_data = range_data;
				out_animated_data = animated_data;
			}
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
