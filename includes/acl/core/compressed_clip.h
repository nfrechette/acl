#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/algorithm_versions.h"
#include "acl/core/buffer_tag.h"
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/memory_utils.h"
#include "acl/core/ptr_offset.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	class CompressedClip;

	namespace acl_impl
	{
		CompressedClip* make_compressed_clip(void* buffer, uint32_t size, algorithm_type8 type);
		void finalize_compressed_clip(CompressedClip& compressed_clip);
	}

	////////////////////////////////////////////////////////////////////////////////
	// An instance of a compressed clip.
	// The compressed data immediately follows the clip instance.
	////////////////////////////////////////////////////////////////////////////////
	class alignas(16) CompressedClip
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Returns the algorithm type used to compress the clip.
		algorithm_type8 get_algorithm_type() const { return m_type; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of the compressed clip.
		// Includes the 'CompressedClip' instance size.
		uint32_t get_size() const { return m_size; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the hash for this compressed clip.
		uint32_t get_hash() const { return m_hash; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the binary tag for the compressed clip.
		// This uniquely identifies the buffer as a proper 'CompressedClip' object.
		buffer_tag32 get_tag() const { return static_cast<buffer_tag32>(m_tag); }

		////////////////////////////////////////////////////////////////////////////////
		// Returns true if a compressed clip is valid and usable.
		// This mainly validates some invariants as well as ensuring that the
		// memory has not been corrupted.
		//
		// check_hash: If true, the compressed clip hash will also be compared.
		ErrorResult is_valid(bool check_hash) const
		{
			if (!is_aligned_to(this, alignof(CompressedClip)))
				return ErrorResult("Invalid alignment");

			if (m_tag != static_cast<uint32_t>(buffer_tag32::compressed_clip))
				return ErrorResult("Invalid tag");

			if (!is_valid_algorithm_type(m_type))
				return ErrorResult("Invalid algorithm type");

			if (m_version != get_algorithm_version(m_type))
				return ErrorResult("Invalid algorithm version");

			if (check_hash) {
				const uint32_t hash = hash32(safe_ptr_cast<const uint8_t>(this) + k_hash_skip_size, m_size - k_hash_skip_size);
				if (hash != m_hash)
					return ErrorResult("Invalid hash");
			}

			return ErrorResult();
		}

	private:
		////////////////////////////////////////////////////////////////////////////////
		// The number of bytes to skip in the header when calculating the hash.
		static constexpr uint32_t k_hash_skip_size = sizeof(uint32_t) + sizeof(uint32_t);	// m_size + m_hash

		////////////////////////////////////////////////////////////////////////////////
		// Constructs a compressed clip instance
		CompressedClip(uint32_t size, algorithm_type8 type)
			: m_size(size)
			, m_hash(hash32(safe_ptr_cast<const uint8_t>(this) + k_hash_skip_size, size - k_hash_skip_size))
			, m_tag(static_cast<uint32_t>(buffer_tag32::compressed_clip))
			, m_version(get_algorithm_version(type))
			, m_type(type)
			, m_padding(0)
		{
			(void)m_padding;	// Avoid unused warning
		}

		////////////////////////////////////////////////////////////////////////////////
		// 16 byte header, the rest of the data follows in memory.
		////////////////////////////////////////////////////////////////////////////////

		// Total size in bytes of the compressed clip. Includes 'sizeof(CompressedClip)'.
		uint32_t		m_size;

		// Hash of the compressed clip. Hashed memory starts immediately after this.
		uint32_t		m_hash;

		////////////////////////////////////////////////////////////////////////////////
		// Everything starting here is included in the hash.
		////////////////////////////////////////////////////////////////////////////////

		// Serialization tag used to distinguish raw buffer types.
		uint32_t		m_tag;

		// Serialization version used to compress the clip.
		uint16_t		m_version;

		// Algorithm type used to compress the clip.
		algorithm_type8	m_type;

		// Unused memory left as padding
		uint8_t			m_padding;

		////////////////////////////////////////////////////////////////////////////////
		// Friend function used to construct compressed clip instances. Should only
		// be called by encoders.
		friend CompressedClip* acl_impl::make_compressed_clip(void* buffer, uint32_t size, algorithm_type8 type);

		////////////////////////////////////////////////////////////////////////////////
		// Friend function to finalize a compressed clip once all memory has been written within.
		friend void acl_impl::finalize_compressed_clip(CompressedClip& compressed_clip);
	};

	static_assert(alignof(CompressedClip) == 16, "Invalid alignment for CompressedClip");
	static_assert(sizeof(CompressedClip) == 16, "Invalid size for CompressedClip");

	//////////////////////////////////////////////////////////////////////////
	// Create a CompressedClip instance in place from a raw memory buffer.
	// If the buffer does not contain a valid CompressedClip instance, nullptr is returned
	// along with an optional error result.
	//////////////////////////////////////////////////////////////////////////
	inline const CompressedClip* make_compressed_clip(const void* buffer, ErrorResult* out_error_result = nullptr)
	{
		if (buffer == nullptr)
		{
			if (out_error_result != nullptr)
				*out_error_result = ErrorResult("Buffer is not a valid pointer");

			return nullptr;
		}

		const CompressedClip* clip = static_cast<const CompressedClip*>(buffer);
		if (out_error_result != nullptr)
		{
			const ErrorResult result = clip->is_valid(false);
			*out_error_result = result;

			if (result.any())
				return nullptr;
		}

		return clip;
	}

	////////////////////////////////////////////////////////////////////////////////
	// A compressed clip segment header. Each segment is built from a uniform number
	// of samples per track. A clip is split into one or more segments.
	////////////////////////////////////////////////////////////////////////////////
	struct SegmentHeader
	{
		// Number of bits used by a fully animated pose (excludes default/constant tracks).
		uint32_t				animated_pose_bit_size;						// TODO: Calculate from bitsets and formats?

		// TODO: Only need one offset, calculate the others from the information we have?
		// Offset to the per animated track format data.
		PtrOffset32<uint8_t>	format_per_track_data_offset;				// TODO: Make this offset optional? Only present if variable

		// Offset to the segment range data.
		PtrOffset32<uint8_t>	range_data_offset;							// TODO: Make this offset optional? Only present if normalized

		// Offset to the segment animated tracks data.
		PtrOffset32<uint8_t>	track_data_offset;
	};

	////////////////////////////////////////////////////////////////////////////////
	// A compressed clip header.
	////////////////////////////////////////////////////////////////////////////////
	struct ClipHeader
	{
		// The number of bones compressed.
		uint16_t				num_bones;

		// The number of segments contained.
		uint16_t				num_segments;

		// The rotation/translation/scale format used.
		rotation_format8		rotation_format;
		vector_format8			translation_format;
		vector_format8			scale_format;								// TODO: Make this optional?

		// Whether or not we have scale (bool).
		uint8_t					has_scale;

		// Whether the default scale is 0,0,0 or 1,1,1 (bool/bit).
		uint8_t					default_scale;

		uint8_t padding[3];

		// The total number of samples per track our clip contained.
		uint32_t				num_samples;

		// The clip sample rate.
		float					sample_rate;								// TODO: Store duration as float instead

		// Offset to the segment headers data.
		PtrOffset16<uint32_t>		segment_start_indices_offset;
		PtrOffset16<SegmentHeader>	segment_headers_offset;

		// Offsets to the default/constant tracks bitsets.
		PtrOffset16<uint32_t>		default_tracks_bitset_offset;
		PtrOffset16<uint32_t>		constant_tracks_bitset_offset;

		// Offset to the constant tracks data.
		PtrOffset16<uint8_t>		constant_track_data_offset;

		// Offset to the clip range data.
		PtrOffset16<uint8_t>		clip_range_data_offset;					// TODO: Make this offset optional? Only present if normalized

		//////////////////////////////////////////////////////////////////////////
		// Utility functions that return pointers from their respective offsets.

		uint32_t*		get_segment_start_indices()			{ return segment_start_indices_offset.safe_add_to(this); }
		const uint32_t*	get_segment_start_indices() const	{ return segment_start_indices_offset.safe_add_to(this); }

		SegmentHeader*			get_segment_headers()		{ return segment_headers_offset.add_to(this); }
		const SegmentHeader*	get_segment_headers() const	{ return segment_headers_offset.add_to(this); }

		uint32_t*		get_default_tracks_bitset()			{ return default_tracks_bitset_offset.add_to(this); }
		const uint32_t*	get_default_tracks_bitset() const	{ return default_tracks_bitset_offset.add_to(this); }

		uint32_t*		get_constant_tracks_bitset()		{ return constant_tracks_bitset_offset.add_to(this); }
		const uint32_t*	get_constant_tracks_bitset() const	{ return constant_tracks_bitset_offset.add_to(this); }

		uint8_t*		get_constant_track_data()			{ return constant_track_data_offset.safe_add_to(this); }
		const uint8_t*	get_constant_track_data() const		{ return constant_track_data_offset.safe_add_to(this); }

		uint8_t*		get_format_per_track_data(const SegmentHeader& header)			{ return header.format_per_track_data_offset.safe_add_to(this); }
		const uint8_t*	get_format_per_track_data(const SegmentHeader& header) const	{ return header.format_per_track_data_offset.safe_add_to(this); }

		uint8_t*		get_clip_range_data()				{ return clip_range_data_offset.safe_add_to(this); }
		const uint8_t*	get_clip_range_data() const			{ return clip_range_data_offset.safe_add_to(this); }

		uint8_t*		get_track_data(const SegmentHeader& header)			{ return header.track_data_offset.safe_add_to(this); }
		const uint8_t*	get_track_data(const SegmentHeader& header) const	{ return header.track_data_offset.safe_add_to(this); }

		uint8_t*		get_segment_range_data(const SegmentHeader& header)			{ return header.range_data_offset.safe_add_to(this); }
		const uint8_t*	get_segment_range_data(const SegmentHeader& header) const	{ return header.range_data_offset.safe_add_to(this); }
	};

	// Returns the clip header for a compressed clip.
	inline ClipHeader& get_clip_header(CompressedClip& clip)
	{
		return *add_offset_to_ptr<ClipHeader>(&clip, sizeof(CompressedClip));
	}

	// Returns the clip header for a compressed clip.
	inline const ClipHeader& get_clip_header(const CompressedClip& clip)
	{
		return *add_offset_to_ptr<const ClipHeader>(&clip, sizeof(CompressedClip));
	}
}

ACL_IMPL_FILE_PRAGMA_POP
