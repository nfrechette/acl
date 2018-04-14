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
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/memory_utils.h"
#include "acl/core/ptr_offset.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_types.h"

#include <cstdint>

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// An instance of a compressed clip.
	// The compressed data immediately follows the clip instance.
	////////////////////////////////////////////////////////////////////////////////
	class alignas(16) CompressedClip
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Returns the algorithm type used to compress the clip.
		AlgorithmType8 get_algorithm_type() const { return m_type; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of the compressed clip.
		// Includes the 'CompressedClip' instance size.
		uint32_t get_size() const { return m_size; }

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

			if (m_tag != k_compressed_clip_tag)
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
		// A known tag value to distinguish compressed clips from other things.
		static constexpr uint32_t k_compressed_clip_tag = 0xac10ac10;

		////////////////////////////////////////////////////////////////////////////////
		// The number of bytes to skip in the header when calculating the hash.
		static constexpr uint32_t k_hash_skip_size = sizeof(uint32_t) + sizeof(uint32_t);	// m_size + m_hash

		////////////////////////////////////////////////////////////////////////////////
		// Constructs a compressed clip instance
		CompressedClip(uint32_t size, AlgorithmType8 type)
			: m_size(size)
			, m_hash(hash32(safe_ptr_cast<const uint8_t>(this) + k_hash_skip_size, size - k_hash_skip_size))
			, m_tag(k_compressed_clip_tag)
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
		AlgorithmType8	m_type;

		// Unused memory left as padding
		uint8_t			m_padding;

		////////////////////////////////////////////////////////////////////////////////
		// Friend function used to construct compressed clip instances. Should only
		// be called by encoders.
		friend CompressedClip* make_compressed_clip(void* buffer, uint32_t size, AlgorithmType8 type);

		////////////////////////////////////////////////////////////////////////////////
		// Friend function to finalize a compressed clip once all memory has been written within.
		friend void finalize_compressed_clip(CompressedClip& compressed_clip);
	};

	static_assert(alignof(CompressedClip) == 16, "Invalid alignment for CompressedClip");
	static_assert(sizeof(CompressedClip) == 16, "Invalid size for CompressedClip");

	////////////////////////////////////////////////////////////////////////////////
	// A compressed clip segment header. Each segment is built from a uniform number
	// of samples per track. A clip is split into one or more segments.
	////////////////////////////////////////////////////////////////////////////////
	struct SegmentHeader
	{
		// Number of samples the segment was constructed from per track.
		uint32_t				num_samples;

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
		RotationFormat8			rotation_format;
		VectorFormat8			translation_format;
		VectorFormat8			scale_format;								// TODO: Make this optional?

		// The clip/segment range reduction format used.
		RangeReductionFlags8	clip_range_reduction;
		RangeReductionFlags8	segment_range_reduction;

		// Whether or not we have scale (bool).
		uint8_t					has_scale;

		// The total number of samples per track our clip contained.
		uint32_t				num_samples;

		// The clip sample rate.
		uint32_t				sample_rate;								// TODO: Store duration as float instead

		// Offset to the segment headers data.
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
