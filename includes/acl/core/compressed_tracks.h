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
#include "acl/core/buffer_tag.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/utils.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/impl/compressed_headers.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	class compressed_tracks;

	namespace acl_impl
	{
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
		algorithm_type8 get_algorithm_type() const { return acl_impl::get_tracks_header(*this).algorithm_type; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of the compressed tracks.
		// Includes the 'compressed_tracks' instance size.
		uint32_t get_size() const { return m_buffer_header.size; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the hash for the compressed tracks.
		// This is only used for sanity checking in case of memory corruption.
		uint32_t get_hash() const { return m_buffer_header.hash; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the binary tag for the compressed tracks.
		// This uniquely identifies the buffer as a proper 'compressed_tracks' object.
		buffer_tag32 get_tag() const { return static_cast<buffer_tag32>(acl_impl::get_tracks_header(*this).tag); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the binary format version.
		compressed_tracks_version16 get_version() const { return acl_impl::get_tracks_header(*this).version; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of tracks contained.
		uint32_t get_num_tracks() const { return acl_impl::get_tracks_header(*this).num_tracks; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of samples each track contains.
		uint32_t get_num_samples_per_track() const { return acl_impl::get_tracks_header(*this).num_samples; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the type of the compressed tracks.
		track_type8 get_track_type() const { return acl_impl::get_tracks_header(*this).track_type; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the duration of each track.
		float get_duration() const
		{
			const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
			return calculate_duration(header.num_samples, header.sample_rate);
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the sample rate used by each track.
		float get_sample_rate() const { return acl_impl::get_tracks_header(*this).sample_rate; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns true if the compressed tracks are valid and usable.
		// This mainly validates some invariants as well as ensuring that the
		// memory has not been corrupted.
		//
		// check_hash: If true, the compressed tracks hash will also be compared.
		error_result is_valid(bool check_hash) const
		{
			if (!is_aligned_to(this, alignof(compressed_tracks)))
				return error_result("Invalid alignment");

			const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
			if (header.tag != static_cast<uint32_t>(buffer_tag32::compressed_tracks))
				return error_result("Invalid tag");

			if (!is_valid_algorithm_type(header.algorithm_type))
				return error_result("Invalid algorithm type");

			if (header.version < compressed_tracks_version16::first || header.version > compressed_tracks_version16::latest)
				return error_result("Invalid algorithm version");

			if (check_hash)
			{
				const uint32_t hash = hash32(safe_ptr_cast<const uint8_t>(&m_padding[0]), m_buffer_header.size - sizeof(acl_impl::raw_buffer_header));
				if (hash != m_buffer_header.hash)
					return error_result("Invalid hash");
			}

			return error_result();
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

		// Here we define some unspecified padding but the 'tracks_header' starts here.
		// This is done to ensure that this class is 16 byte aligned without requiring further padding
		// if the 'tracks_header' ends up causing us to be unaligned.
		uint32_t m_padding[2];
		//acl_impl::tracks_header			m_tracks_header;

		//////////////////////////////////////////////////////////////////////////
		// Compressed data follows here in memory.
		//////////////////////////////////////////////////////////////////////////

		friend const acl_impl::tracks_header& acl_impl::get_tracks_header(const compressed_tracks& tracks);
	};

	//////////////////////////////////////////////////////////////////////////
	// Create a compressed_tracks instance in place from a raw memory buffer.
	// If the buffer does not contain a valid compressed_tracks instance, nullptr is returned
	// along with an optional error result.
	//////////////////////////////////////////////////////////////////////////
	inline const compressed_tracks* make_compressed_tracks(const void* buffer, error_result* out_error_result = nullptr)
	{
		if (buffer == nullptr)
		{
			if (out_error_result != nullptr)
				*out_error_result = error_result("Buffer is not a valid pointer");

			return nullptr;
		}

		const compressed_tracks* clip = static_cast<const compressed_tracks*>(buffer);
		if (out_error_result != nullptr)
		{
			const error_result result = clip->is_valid(false);
			*out_error_result = result;

			if (result.any())
				return nullptr;
		}

		return clip;
	}

	namespace acl_impl
	{
		// Hide these implementations, they shouldn't be needed in user-space
		inline const tracks_header& get_tracks_header(const compressed_tracks& tracks)
		{
			return *reinterpret_cast<const tracks_header*>(&tracks.m_padding[0]);
		}

		inline const scalar_tracks_header& get_scalar_tracks_header(const compressed_tracks& tracks)
		{
			return *reinterpret_cast<const scalar_tracks_header*>(reinterpret_cast<const uint8_t*>(&tracks) + sizeof(raw_buffer_header) + sizeof(tracks_header));
		}

		inline const transform_tracks_header& get_transform_tracks_header(const compressed_tracks& tracks)
		{
			return *reinterpret_cast<const transform_tracks_header*>(reinterpret_cast<const uint8_t*>(&tracks) + sizeof(raw_buffer_header) + sizeof(tracks_header));
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
