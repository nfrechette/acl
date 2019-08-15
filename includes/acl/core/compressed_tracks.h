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
#include "acl/core/algorithm_versions.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/utils.h"
#include "acl/core/impl/compressed_headers.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	class compressed_tracks;

	namespace acl_impl
	{
		////////////////////////////////////////////////////////////////////////////////
		// A known tag value to distinguish compressed tracks from other things.
		static constexpr uint32_t k_compressed_tracks_tag = 0xac11ac11;

		const tracks_header& get_tracks_header(const compressed_tracks& tracks);
	}

	////////////////////////////////////////////////////////////////////////////////
	// An instance of a compressed tracks.
	// The compressed data immediately follows this instance in memory.
	// The total size of the buffer can be queried with `get_size()`.
	////////////////////////////////////////////////////////////////////////////////
	class alignas(16) compressed_tracks final
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Returns the algorithm type used to compress the tracks.
		AlgorithmType8 get_algorithm_type() const { return m_tracks_header.algorithm_type; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of the compressed tracks.
		// Includes the 'compressed_tracks' instance size.
		uint32_t get_size() const { return m_buffer_header.size; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the hash for the compressed tracks.
		// This is only used for sanity checking in case of memory corruption.
		uint32_t get_hash() const { return m_buffer_header.hash; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of tracks contained.
		uint32_t get_num_tracks() const { return m_tracks_header.num_tracks; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of samples each track contains.
		uint32_t get_num_samples_per_track() const { return m_tracks_header.num_samples; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the duration of each track.
		float get_duration() const { return calculate_duration(m_tracks_header.num_samples, m_tracks_header.sample_rate); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the sample rate used by each track.
		float get_sample_rate() const { return m_tracks_header.sample_rate; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns true if the compressed tracks are valid and usable.
		// This mainly validates some invariants as well as ensuring that the
		// memory has not been corrupted.
		//
		// check_hash: If true, the compressed tracks hash will also be compared.
		ErrorResult is_valid(bool check_hash) const
		{
			if (!is_aligned_to(this, alignof(compressed_tracks)))
				return ErrorResult("Invalid alignment");

			if (m_tracks_header.tag != acl_impl::k_compressed_tracks_tag)
				return ErrorResult("Invalid tag");

			if (!is_valid_algorithm_type(m_tracks_header.algorithm_type))
				return ErrorResult("Invalid algorithm type");

			if (m_tracks_header.version != get_algorithm_version(m_tracks_header.algorithm_type))
				return ErrorResult("Invalid algorithm version");

			if (check_hash)
			{
				const uint32_t hash = hash32(safe_ptr_cast<const uint8_t>(&m_tracks_header), m_buffer_header.size - sizeof(acl_impl::raw_buffer_header));
				if (hash != m_buffer_header.hash)
					return ErrorResult("Invalid hash");
			}

			return ErrorResult();
		}

	private:
		////////////////////////////////////////////////////////////////////////////////
		// Hide everything
		compressed_tracks() = delete;
		compressed_tracks(const compressed_tracks&) = delete;
		compressed_tracks(compressed_tracks&&) = delete;
		compressed_tracks* operator=(const compressed_tracks&) = delete;
		compressed_tracks* operator=(compressed_tracks&&) = delete;

		////////////////////////////////////////////////////////////////////////////////
		// Raw buffer header that isn't included in the hash.
		////////////////////////////////////////////////////////////////////////////////

		acl_impl::raw_buffer_header		m_buffer_header;

		////////////////////////////////////////////////////////////////////////////////
		// Everything starting here is included in the hash.
		////////////////////////////////////////////////////////////////////////////////

		acl_impl::tracks_header			m_tracks_header;

		//////////////////////////////////////////////////////////////////////////
		// Compressed data follows here in memory.
		//////////////////////////////////////////////////////////////////////////

		friend const acl_impl::tracks_header& acl_impl::get_tracks_header(const compressed_tracks& tracks);
	};

	namespace acl_impl
	{
		// Hide this implementation, it shouldn't be needed in user-space
		inline const tracks_header& get_tracks_header(const compressed_tracks& tracks) { return tracks.m_tracks_header; }
	}
}

ACL_IMPL_FILE_PRAGMA_POP
