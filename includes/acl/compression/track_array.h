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

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error_result.h"
#include "acl/core/iallocator.h"
#include "acl/core/track_writer.h"
#include "acl/compression/track.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <limits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// An array of tracks.
	// Although each track contained within is untyped, each track must have
	// the same type. They must all have the same sample rate and the same
	// number of samples.
	//////////////////////////////////////////////////////////////////////////
	class track_array
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// Constructs an empty track array.
		track_array()
			: m_allocator(nullptr)
			, m_tracks(nullptr)
			, m_num_tracks(0)
		{}

		//////////////////////////////////////////////////////////////////////////
		// Constructs an array with the specified number of tracks.
		// Tracks will be empty and untyped by default.
		track_array(IAllocator& allocator, uint32_t num_tracks)
			: m_allocator(&allocator)
			, m_tracks(allocate_type_array<track>(allocator, num_tracks))
			, m_num_tracks(num_tracks)
		{}

		//////////////////////////////////////////////////////////////////////////
		// Move constructor for a track array.
		track_array(track_array&& other)
			: m_allocator(other.m_allocator)
			, m_tracks(other.m_tracks)
			, m_num_tracks(other.m_num_tracks)
		{
			other.m_allocator = nullptr;	// Make sure we don't free our data since we no longer own it
		}

		//////////////////////////////////////////////////////////////////////////
		// Destroys a track array.
		~track_array()
		{
			if (m_allocator != nullptr)
				deallocate_type_array(*m_allocator, m_tracks, m_num_tracks);
		}

		//////////////////////////////////////////////////////////////////////////
		// Move assignment for a track array.
		track_array& operator=(track_array&& other)
		{
			std::swap(m_allocator, other.m_allocator);
			std::swap(m_tracks, other.m_tracks);
			std::swap(m_num_tracks, other.m_num_tracks);
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of tracks contained in this array.
		uint32_t get_num_tracks() const { return m_num_tracks; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of samples per track in this array.
		uint32_t get_num_samples_per_track() const { return m_allocator != nullptr && m_num_tracks > 0 ? m_tracks->get_num_samples() : 0; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track type for tracks in this array.
		track_type8 get_track_type() const { return m_allocator != nullptr && m_num_tracks > 0 ? m_tracks->get_type() : track_type8::float1f; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track category for tracks in this array.
		track_category8 get_track_category() const { return m_allocator != nullptr && m_num_tracks > 0 ? m_tracks->get_category() : track_category8::scalarf; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the sample rate for tracks in this array.
		float get_sample_rate() const { return m_allocator != nullptr && m_num_tracks > 0 ? m_tracks->get_sample_rate() : 0.0F; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the duration for tracks in this array.
		float get_duration() const { return m_allocator != nullptr && m_num_tracks > 0 ? calculate_duration(uint32_t(m_tracks->get_num_samples()), m_tracks->get_sample_rate()) : 0.0F; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track at the specified index.
		track& operator[](uint32_t index)
		{
			ACL_ASSERT(index < m_num_tracks, "Invalid track index. %u >= %u", index, m_num_tracks);
			return m_tracks[index];
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the track at the specified index.
		const track& operator[](uint32_t index) const
		{
			ACL_ASSERT(index < m_num_tracks, "Invalid track index. %u >= %u", index, m_num_tracks);
			return m_tracks[index];
		}

		//////////////////////////////////////////////////////////////////////////
		// Iterator begin() and end() implementations.
		track* begin() { return m_tracks; }
		const track* begin() const { return m_tracks; }
		const track* end() { return m_tracks + m_num_tracks; }
		const track* end() const { return m_tracks + m_num_tracks; }

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a track array is valid or not.
		// An array is valid if:
		//    - It is empty
		//    - All tracks have the same type
		//    - All tracks have the same number of samples
		//    - All tracks have the same sample rate
		ErrorResult is_valid() const
		{
			const track_type8 type = get_track_type();
			const uint32_t num_samples = get_num_samples_per_track();
			const float sample_rate = get_sample_rate();

			for (uint32_t track_index = 0; track_index < m_num_tracks; ++track_index)
			{
				const track& track_ = m_tracks[track_index];
				if (track_.get_type() != type)
					return ErrorResult("Tracks must all have the same type within an array");

				if (track_.get_num_samples() != num_samples)
					return ErrorResult("Track array requires the same number of samples in every track");

				if (track_.get_sample_rate() != sample_rate)
					return ErrorResult("Track array requires the same sample rate in every track");
			}

			return ErrorResult();
		}

		//////////////////////////////////////////////////////////////////////////
		// Sample all tracks within this array at the specified sample time and
		// desired rounding policy. Track samples are written out using the `track_writer` provided.
		template<class track_writer_type>
		void sample_tracks(float sample_time, sample_rounding_policy rounding_policy, track_writer_type& writer) const;

		//////////////////////////////////////////////////////////////////////////
		// Sample a single track within this array at the specified sample time and
		// desired rounding policy. The track sample is written out using the `track_writer` provided.
		template<class track_writer_type>
		void sample_track(uint32_t track_index, float sample_time, sample_rounding_policy rounding_policy, track_writer_type& writer) const;

		//////////////////////////////////////////////////////////////////////////
		// Returns the raw size for this track array. Note that this differs from the actual
		// memory used by an instance of this class. It is meant for comparison against
		// the compressed size.
		uint32_t get_raw_size() const;

	protected:
		//////////////////////////////////////////////////////////////////////////
		// We prohibit copying
		track_array(const track_array&) = delete;
		track_array& operator=(const track_array&) = delete;

		IAllocator*		m_allocator;		// The allocator used to allocate our tracks
		track*			m_tracks;			// The track list
		uint32_t		m_num_tracks;		// The number of tracks
	};

	//////////////////////////////////////////////////////////////////////////
	// A typed track array. See `track_array` for details.
	//////////////////////////////////////////////////////////////////////////
	template<track_type8 track_type_>
	class track_array_typed final : public track_array
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// The track type.
		static constexpr track_type8 type = track_type_;

		//////////////////////////////////////////////////////////////////////////
		// The track category.
		static constexpr track_category8 category = track_traits<track_type_>::category;

		//////////////////////////////////////////////////////////////////////////
		// The track member type.
		using track_member_type = track_typed<track_type_>;

		//////////////////////////////////////////////////////////////////////////
		// Constructs an empty track array.
		track_array_typed() : track_array() { static_assert(sizeof(track_array_typed) == sizeof(track_array), "You cannot add member variables to this class"); }

		//////////////////////////////////////////////////////////////////////////
		// Constructs an array with the specified number of tracks.
		// Tracks will be empty and untyped by default.
		track_array_typed(IAllocator& allocator, uint32_t num_tracks) : track_array(allocator, num_tracks) {}

		//////////////////////////////////////////////////////////////////////////
		// Move constructor for a track array.
		track_array_typed(track_array_typed&& other) : track_array(static_cast<track_array&&>(other)) {}

		//////////////////////////////////////////////////////////////////////////
		// Destroys a track array.
		~track_array_typed() = default;

		//////////////////////////////////////////////////////////////////////////
		// Move assignment for a track array.
		track_array_typed& operator=(track_array_typed&& other) { return static_cast<track_array_typed&>(track_array::operator=(static_cast<track_array&&>(other))); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track type for tracks in this array.
		track_type8 get_track_type() const { return type; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track category for tracks in this array.
		track_category8 get_track_category() const { return category; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track at the specified index.
		track_member_type& operator[](uint32_t index)
		{
			ACL_ASSERT(index < m_num_tracks, "Invalid track index. %u >= %u", index, m_num_tracks);
			return track_cast<track_member_type>(m_tracks[index]);
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the track at the specified index.
		const track_member_type& operator[](uint32_t index) const
		{
			ACL_ASSERT(index < m_num_tracks, "Invalid track index. %u >= %u", index, m_num_tracks);
			return track_cast<track_member_type>(m_tracks[index]);
		}

		//////////////////////////////////////////////////////////////////////////
		// Iterator begin() and end() implementations.
		track_member_type* begin() { return track_cast<track_member_type>(m_tracks); }
		const track_member_type* begin() const { return track_cast<track_member_type>(m_tracks); }
		const track_member_type* end() { return track_cast<track_member_type>(m_tracks) + m_num_tracks; }
		const track_member_type* end() const { return track_cast<track_member_type>(m_tracks) + m_num_tracks; }
	};

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track array into the desired track array type while asserting for safety.
	template<typename track_array_type>
	inline track_array_type& track_array_cast(track_array& track_array_)
	{
		ACL_ASSERT(track_array_type::type == track_array_.get_track_type() || track_array_.get_num_tracks() == 0, "Unexpected track type");
		return static_cast<track_array_type&>(track_array_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track array into the desired track array type while asserting for safety.
	template<typename track_array_type>
	inline const track_array_type& track_array_cast(const track_array& track_array_)
	{
		ACL_ASSERT(track_array_type::type == track_array_.get_track_type() || track_array_.get_num_tracks() == 0, "Unexpected track type");
		return static_cast<const track_array_type&>(track_array_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track array into the desired track array type. Returns nullptr if the types
	// are not compatible or if the input is nullptr.
	template<typename track_array_type>
	inline track_array_type* track_array_cast(track_array* track_array_)
	{
		if (track_array_ == nullptr || (track_array_type::type != track_array_->get_track_type() && track_array_->get_num_tracks() != 0))
			return nullptr;

		return static_cast<track_array_type*>(track_array_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track array into the desired track array type. Returns nullptr if the types
	// are not compatible or if the input is nullptr.
	template<typename track_array_type>
	inline const track_array_type* track_array_cast(const track_array* track_array_)
	{
		if (track_array_ == nullptr || (track_array_type::type != track_array_->get_track_type() && track_array_->get_num_tracks() != 0))
			return nullptr;

		return static_cast<const track_array_type*>(track_array_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Create aliases for the various typed track array types.

	using track_array_float1f = track_array_typed<track_type8::float1f>;
	using track_array_float2f = track_array_typed<track_type8::float2f>;
	using track_array_float3f = track_array_typed<track_type8::float3f>;
	using track_array_float4f = track_array_typed<track_type8::float4f>;
	using track_array_vector4f = track_array_typed<track_type8::vector4f>;

	//////////////////////////////////////////////////////////////////////////

	template<class track_writer_type>
	inline void track_array::sample_tracks(float sample_time, sample_rounding_policy rounding_policy, track_writer_type& writer) const
	{
		ACL_ASSERT(is_valid().empty(), "Invalid track array");

		for (uint32_t track_index = 0; track_index < m_num_tracks; ++track_index)
			sample_track(track_index, sample_time, rounding_policy, writer);
	}

	template<class track_writer_type>
	inline void track_array::sample_track(uint32_t track_index, float sample_time, sample_rounding_policy rounding_policy, track_writer_type& writer) const
	{
		ACL_ASSERT(is_valid().empty(), "Invalid track array");
		ACL_ASSERT(track_index < m_num_tracks, "Invalid track index");

		const track& track_ = m_tracks[track_index];
		const uint32_t num_samples = track_.get_num_samples();
		const float sample_rate = track_.get_sample_rate();

		uint32_t key_frame0;
		uint32_t key_frame1;
		float interpolation_alpha;
		find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, sample_time, rounding_policy, key_frame0, key_frame1, interpolation_alpha);

		switch (track_.get_type())
		{
		case track_type8::float1f:
		{
			const track_float1f& track__ = track_cast<track_float1f>(track_);

			const rtm::scalarf value0 = rtm::scalar_load(&track__[key_frame0]);
			const rtm::scalarf value1 = rtm::scalar_load(&track__[key_frame1]);
			const rtm::scalarf value = rtm::scalar_lerp(value0, value1, rtm::scalar_set(interpolation_alpha));
			writer.write_float1(track_index, value);
			break;
		}
		case track_type8::float2f:
		{
			const track_float2f& track__ = track_cast<track_float2f>(track_);

			const rtm::vector4f value0 = rtm::vector_load2(&track__[key_frame0]);
			const rtm::vector4f value1 = rtm::vector_load2(&track__[key_frame1]);
			const rtm::vector4f value = rtm::vector_lerp(value0, value1, interpolation_alpha);
			writer.write_float2(track_index, value);
			break;
		}
		case track_type8::float3f:
		{
			const track_float3f& track__ = track_cast<track_float3f>(track_);

			const rtm::vector4f value0 = rtm::vector_load3(&track__[key_frame0]);
			const rtm::vector4f value1 = rtm::vector_load3(&track__[key_frame1]);
			const rtm::vector4f value = rtm::vector_lerp(value0, value1, interpolation_alpha);
			writer.write_float3(track_index, value);
			break;
		}
		case track_type8::float4f:
		{
			const track_float4f& track__ = track_cast<track_float4f>(track_);

			const rtm::vector4f value0 = rtm::vector_load(&track__[key_frame0]);
			const rtm::vector4f value1 = rtm::vector_load(&track__[key_frame1]);
			const rtm::vector4f value = rtm::vector_lerp(value0, value1, interpolation_alpha);
			writer.write_float4(track_index, value);
			break;
		}
		case track_type8::vector4f:
		{
			const track_vector4f& track__ = track_cast<track_vector4f>(track_);

			const rtm::vector4f value0 = track__[key_frame0];
			const rtm::vector4f value1 = track__[key_frame1];
			const rtm::vector4f value = rtm::vector_lerp(value0, value1, interpolation_alpha);
			writer.write_vector4(track_index, value);
			break;
		}
		default:
			ACL_ASSERT(false, "Invalid track type");
			break;
		}
	}

	inline uint32_t track_array::get_raw_size() const
	{
		const uint32_t num_samples = get_num_samples_per_track();

		uint32_t total_size = 0;
		for (uint32_t track_index = 0; track_index < m_num_tracks; ++track_index)
		{
			const track& track_ = m_tracks[track_index];
			total_size += num_samples * track_.get_sample_size();
		}

		return total_size;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
