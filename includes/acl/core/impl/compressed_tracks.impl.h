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

// Included only once from compressed_tracks.h

namespace acl
{
	namespace acl_impl
	{
		// Hide these implementations, they shouldn't be needed in user-space
		inline const tracks_header& get_tracks_header(const compressed_tracks& tracks)
		{
			return *reinterpret_cast<const tracks_header*>(reinterpret_cast<const uint8_t*>(&tracks) + sizeof(raw_buffer_header));
		}

		inline const scalar_tracks_header& get_scalar_tracks_header(const compressed_tracks& tracks)
		{
			return *reinterpret_cast<const scalar_tracks_header*>(reinterpret_cast<const uint8_t*>(&tracks) + sizeof(raw_buffer_header) + sizeof(tracks_header));
		}

		inline const transform_tracks_header& get_transform_tracks_header(const compressed_tracks& tracks)
		{
			return *reinterpret_cast<const transform_tracks_header*>(reinterpret_cast<const uint8_t*>(&tracks) + sizeof(raw_buffer_header) + sizeof(tracks_header));
		}

		inline const optional_metadata_header& get_optional_metadata_header(const compressed_tracks& tracks)
		{
			return *reinterpret_cast<const optional_metadata_header*>(reinterpret_cast<const uint8_t*>(&tracks) + tracks.get_size() - sizeof(optional_metadata_header));
		}
	}

	algorithm_type8 compressed_tracks::get_algorithm_type() const { return acl_impl::get_tracks_header(*this).algorithm_type; }

	buffer_tag32 compressed_tracks::get_tag() const { return static_cast<buffer_tag32>(acl_impl::get_tracks_header(*this).tag); }

	compressed_tracks_version16 compressed_tracks::get_version() const { return acl_impl::get_tracks_header(*this).version; }

	uint32_t compressed_tracks::get_num_tracks() const { return acl_impl::get_tracks_header(*this).num_tracks; }

	uint32_t compressed_tracks::get_num_samples_per_track() const { return acl_impl::get_tracks_header(*this).num_samples; }

	track_type8 compressed_tracks::get_track_type() const { return acl_impl::get_tracks_header(*this).track_type; }

	float compressed_tracks::get_duration() const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		return calculate_duration(header.num_samples, header.sample_rate);
	}

	float compressed_tracks::get_sample_rate() const { return acl_impl::get_tracks_header(*this).sample_rate; }

	const char* compressed_tracks::get_name() const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (!header.get_has_metadata())
			return nullptr;	// No metadata is stored

		const acl_impl::optional_metadata_header& metadata_header = acl_impl::get_optional_metadata_header(*this);
		if (!metadata_header.track_name_offsets.is_valid())
			return nullptr;	// Track names aren't stored

		return metadata_header.get_track_list_name(*this);
	}

	const char* compressed_tracks::get_track_name(uint32_t track_index) const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (!header.get_has_metadata())
			return nullptr;	// No metadata is stored

		ACL_ASSERT(track_index < header.num_tracks, "Invalid track index");
		if (track_index >= header.num_tracks)
			return nullptr;	// Invalid track index

		const acl_impl::optional_metadata_header& metadata_header = acl_impl::get_optional_metadata_header(*this);
		if (!metadata_header.track_name_offsets.is_valid())
			return nullptr;	// Track names aren't stored

		const uint32_t* track_names_offsets = metadata_header.get_track_name_offsets(*this);
		const ptr_offset32<char> offset = track_names_offsets[track_index];
		return offset.add_to(track_names_offsets);
	}

	error_result compressed_tracks::is_valid(bool check_hash) const
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

	inline const compressed_tracks* make_compressed_tracks(const void* buffer, error_result* out_error_result)
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
}
