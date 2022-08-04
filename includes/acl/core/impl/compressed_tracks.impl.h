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

#include "acl/version.h"
#include "acl/core/track_desc.h"

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

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

		inline transform_tracks_header& get_transform_tracks_header(compressed_tracks& tracks)
		{
			return *reinterpret_cast<transform_tracks_header*>(reinterpret_cast<uint8_t*>(&tracks) + sizeof(raw_buffer_header) + sizeof(tracks_header));
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

	inline algorithm_type8 compressed_tracks::get_algorithm_type() const { return acl_impl::get_tracks_header(*this).algorithm_type; }

	inline buffer_tag32 compressed_tracks::get_tag() const { return static_cast<buffer_tag32>(acl_impl::get_tracks_header(*this).tag); }

	inline compressed_tracks_version16 compressed_tracks::get_version() const { return acl_impl::get_tracks_header(*this).version; }

	inline uint32_t compressed_tracks::get_num_tracks() const { return acl_impl::get_tracks_header(*this).num_tracks; }

	inline uint32_t compressed_tracks::get_num_samples_per_track() const { return acl_impl::get_tracks_header(*this).num_samples; }

	inline track_type8 compressed_tracks::get_track_type() const { return acl_impl::get_tracks_header(*this).track_type; }

	inline float compressed_tracks::get_duration(sample_looping_policy looping_policy) const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);

		if (looping_policy == sample_looping_policy::as_compressed)
		{
			if (header.version <= compressed_tracks_version16::v02_00_00)
				looping_policy = sample_looping_policy::clamp;	// Older versions used clamp
			else if (header.get_is_wrap_optimized())
				looping_policy = sample_looping_policy::wrap;
			else
				looping_policy = sample_looping_policy::clamp;
		}

		// When we wrap, we artificially insert a repeating first sample at the end of non-empty clips
		uint32_t num_samples = header.num_samples;
		if (looping_policy == sample_looping_policy::wrap && num_samples != 0)
			num_samples++;

		return calculate_duration(num_samples, header.sample_rate);
	}

	inline float compressed_tracks::get_finite_duration(sample_looping_policy looping_policy) const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);

		if (looping_policy == sample_looping_policy::as_compressed)
		{
			if (header.version <= compressed_tracks_version16::v02_00_00)
				looping_policy = sample_looping_policy::clamp;	// Older versions used clamp
			else if (header.get_is_wrap_optimized())
				looping_policy = sample_looping_policy::wrap;
			else
				looping_policy = sample_looping_policy::clamp;
		}

		// When we wrap, we artificially insert a repeating first sample at the end of non-empty clips
		uint32_t num_samples = header.num_samples;
		if (looping_policy == sample_looping_policy::wrap && num_samples != 0)
			num_samples++;

		return calculate_finite_duration(num_samples, header.sample_rate);
	}

	inline float compressed_tracks::get_sample_rate() const { return acl_impl::get_tracks_header(*this).sample_rate; }

	inline bool compressed_tracks::has_database() const { return acl_impl::get_tracks_header(*this).get_has_database(); }

	inline bool compressed_tracks::has_trivial_default_values() const { return acl_impl::get_tracks_header(*this).get_has_trivial_default_values(); }

	inline int32_t compressed_tracks::get_default_scale() const { return acl_impl::get_tracks_header(*this).get_default_scale(); }

	inline sample_looping_policy compressed_tracks::get_looping_policy() const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (header.version <= compressed_tracks_version16::v02_00_00)
			return sample_looping_policy::clamp;	// Older versions used clamp

		return header.get_is_wrap_optimized() ? sample_looping_policy::wrap : sample_looping_policy::clamp;
	}

	inline const char* compressed_tracks::get_name() const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (!header.get_has_metadata())
			return "";	// No metadata is stored

		const acl_impl::optional_metadata_header& metadata_header = acl_impl::get_optional_metadata_header(*this);
		if (!metadata_header.track_name_offsets.is_valid())
			return "";	// Metadata isn't stored

		return metadata_header.get_track_list_name(*this);
	}

	inline const char* compressed_tracks::get_track_name(uint32_t track_index) const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (!header.get_has_metadata())
			return "";	// No metadata is stored

		ACL_ASSERT(track_index < header.num_tracks, "Invalid track index");
		if (track_index >= header.num_tracks)
			return "";	// Invalid track index

		const acl_impl::optional_metadata_header& metadata_header = acl_impl::get_optional_metadata_header(*this);
		if (!metadata_header.track_name_offsets.is_valid())
			return "";	// Metadata isn't stored

		const uint32_t* track_names_offsets = metadata_header.get_track_name_offsets(*this);
		const ptr_offset32<char> offset = track_names_offsets[track_index];
		return offset.add_to(track_names_offsets);
	}

	inline uint32_t compressed_tracks::get_parent_track_index(uint32_t track_index) const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (!header.get_has_metadata())
			return k_invalid_track_index;	// No metadata is stored

		ACL_ASSERT(track_index < header.num_tracks, "Invalid track index");
		if (track_index >= header.num_tracks)
			return k_invalid_track_index;	// Invalid track index

		const acl_impl::optional_metadata_header& metadata_header = acl_impl::get_optional_metadata_header(*this);
		if (!metadata_header.parent_track_indices.is_valid())
			return k_invalid_track_index;	// Metadata isn't stored

		const uint32_t* parent_track_indices = metadata_header.get_parent_track_indices(*this);
		return parent_track_indices[track_index];
	}

	inline bool compressed_tracks::get_track_description(uint32_t track_index, track_desc_scalarf& out_description) const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (!header.get_has_metadata())
			return false;	// No metadata is stored

		ACL_ASSERT(track_index < header.num_tracks, "Invalid track index");
		if (track_index >= header.num_tracks)
			return false;	// Invalid track index

		const acl_impl::optional_metadata_header& metadata_header = acl_impl::get_optional_metadata_header(*this);
		if (!metadata_header.track_descriptions.is_valid())
			return false;	// Metadata isn't stored

		const uint8_t* descriptions = metadata_header.get_track_descriptions(*this);
		const float* description_data = reinterpret_cast<const float*>(descriptions + (track_index * sizeof(float) * 1));

		out_description.output_index = track_index;
		out_description.precision = description_data[0];

		return true;
	}

	inline bool compressed_tracks::get_track_description(uint32_t track_index, track_desc_transformf& out_description) const
	{
		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*this);
		if (!header.get_has_metadata())
			return false;	// No metadata is stored

		ACL_ASSERT(track_index < header.num_tracks, "Invalid track index");
		if (track_index >= header.num_tracks)
			return false;	// Invalid track index

		const acl_impl::optional_metadata_header& metadata_header = acl_impl::get_optional_metadata_header(*this);
		if (!metadata_header.track_descriptions.is_valid())
			return false;	// Metadata isn't stored

		if (!metadata_header.parent_track_indices.is_valid())
			return false;	// Metadata isn't stored

		const compressed_tracks_version16 version = header.version;
		const uint32_t* parent_track_indices = metadata_header.get_parent_track_indices(*this);
		const uint8_t* descriptions = metadata_header.get_track_descriptions(*this);

		// ACL 2.0 track description has:
		//    precision, shell_distance,
		//    constant_rotation_threshold_angle, constant_translation_threshold, constant_scale_threshold
		uint32_t track_description_size = sizeof(float) * 5;

		// ACL 2.1 adds: default_value
		if (version >= compressed_tracks_version16::v02_01_99)
			track_description_size += sizeof(float) * 10;

		const float* description_data = reinterpret_cast<const float*>(descriptions + (size_t(track_index) * track_description_size));

		// Because the data has already been compressed, any track output remapping has already happened
		// which means the output_index is just the track_index
		out_description.output_index = track_index;
		out_description.parent_index = parent_track_indices[track_index];
		out_description.precision = description_data[0];
		out_description.shell_distance = description_data[1];
		out_description.constant_rotation_threshold_angle = description_data[2];
		out_description.constant_translation_threshold = description_data[3];
		out_description.constant_scale_threshold = description_data[4];

		if (version >= compressed_tracks_version16::v02_01_99)
		{
			out_description.default_value.rotation = rtm::quat_load(description_data + 5);
			out_description.default_value.translation = rtm::vector_load3(description_data + 9);
			out_description.default_value.scale = rtm::vector_load3(description_data + 12);
		}
		else
		{
			out_description.default_value = rtm::qvv_identity();
		}

		return true;
	}

	inline error_result compressed_tracks::is_valid(bool check_hash) const
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

	namespace acl_impl
	{
		inline const compressed_tracks* make_compressed_tracks_impl(const void* buffer, error_result* out_error_result)
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

	inline const compressed_tracks* make_compressed_tracks(const void* buffer, error_result* out_error_result)
	{
		return acl_impl::make_compressed_tracks_impl(buffer, out_error_result);
	}

	inline compressed_tracks* make_compressed_tracks(void* buffer, error_result* out_error_result)
	{
		return const_cast<compressed_tracks*>(acl_impl::make_compressed_tracks_impl(buffer, out_error_result));
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}
