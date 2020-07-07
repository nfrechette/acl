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
#include "acl/core/iallocator.h"
#include "acl/core/track_traits.h"
#include "acl/core/track_types.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// An untyped track of data. A track is a time series of values sampled
	// uniformly over time at a specific sample rate. Tracks can either own
	// their memory or reference an external buffer.
	// For convenience, this type can be cast with the `track_cast(..)` family
	// of functions. Each track type has the same size as every track description
	// is contained within a union.
	//////////////////////////////////////////////////////////////////////////
	class track
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// Creates an empty, untyped track.
		track() noexcept;

		//////////////////////////////////////////////////////////////////////////
		// Move constructor for a track.
		track(track&& other) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// Destroys the track. If it owns the memory referenced, it will be freed.
		~track();

		//////////////////////////////////////////////////////////////////////////
		// Move assignment for a track.
		track& operator=(track&& other) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// Returns a pointer to an untyped sample at the specified index.
		void* operator[](uint32_t index);

		//////////////////////////////////////////////////////////////////////////
		// Returns a pointer to an untyped sample at the specified index.
		const void* operator[](uint32_t index) const;

		//////////////////////////////////////////////////////////////////////////
		// Returns true if the track owns its memory, false otherwise.
		bool is_owner() const { return m_allocator != nullptr; }

		//////////////////////////////////////////////////////////////////////////
		// Returns true if the track owns its memory, false otherwise.
		bool is_ref() const { return m_allocator == nullptr; }

		//////////////////////////////////////////////////////////////////////////
		// Returns a pointer to the allocator instance or nullptr if there is none present.
		IAllocator* get_allocator() const { return m_allocator; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of samples contained within the track.
		uint32_t get_num_samples() const { return m_num_samples; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the stride in bytes in between samples as laid out in memory.
		// This is always sizeof(sample_type) unless the memory isn't owned internally.
		uint32_t get_stride() const { return m_stride; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track type.
		track_type8 get_type() const { return m_type; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track category.
		track_category8 get_category() const { return m_category; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of each track sample.
		uint32_t get_sample_size() const { return m_sample_size; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track sample rate.
		// A track has its sampled uniformly distributed in time at a fixed rate (e.g. 30 samples per second).
		float get_sample_rate() const { return m_sample_rate; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track output index.
		// When compressing, it is often desirable to strip or re-order the tracks we output.
		// This can be used to sort by LOD or to strip stale tracks. Tracks with an invalid
		// track index are stripped in the output.
		uint32_t get_output_index() const;

		//////////////////////////////////////////////////////////////////////////
		// Returns the track description.
		template<typename desc_type>
		desc_type& get_description();

		//////////////////////////////////////////////////////////////////////////
		// Returns the track description.
		template<typename desc_type>
		const desc_type& get_description() const;

		//////////////////////////////////////////////////////////////////////////
		// Returns a copy of the track where the memory will be owned by the copy.
		track get_copy(IAllocator& allocator) const;

		//////////////////////////////////////////////////////////////////////////
		// Returns a reference to the track where the memory isn't owned.
		track get_ref() const;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether a track is valid or not.
		// A track is valid if:
		//    - It is empty
		//    - It has a positive and finite sample rate
		//    - A valid description
		ErrorResult is_valid() const;

	protected:
		//////////////////////////////////////////////////////////////////////////
		// We prohibit copying, use get_copy() and get_ref() instead.
		track(const track&) = delete;
		track& operator=(const track&) = delete;

		//////////////////////////////////////////////////////////////////////////
		// Internal constructor.
		// Creates an empty, untyped track.
		track(track_type8 type, track_category8 category) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// Internal constructor.
		track(IAllocator* allocator, uint8_t* data, uint32_t num_samples, uint32_t stride, size_t data_size, float sample_rate, track_type8 type, track_category8 category, uint8_t sample_size) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// Internal helper.
		void get_copy_impl(IAllocator& allocator, track& out_track) const;

		//////////////////////////////////////////////////////////////////////////
		// Internal helper.
		void get_ref_impl(track& out_track) const;

		IAllocator*				m_allocator;		// Optional allocator that owns the memory
		uint8_t*				m_data;				// Pointer to the samples

		uint32_t				m_num_samples;		// The number of samples
		uint32_t				m_stride;			// The stride in bytes in between samples as layed out in memory
		size_t					m_data_size;		// The total size of the buffer used by the samples

		float					m_sample_rate;		// The track sample rate

		track_type8				m_type;				// The track type
		track_category8			m_category;			// The track category
		uint16_t				m_sample_size;		// The size in bytes of each sample

		//////////////////////////////////////////////////////////////////////////
		// A union of every track description.
		// This ensures every track has the same size regardless of its type.
		union desc_union
		{
			track_desc_scalarf		scalar;
			track_desc_transformf	transform;

			desc_union() : scalar() {}
			explicit desc_union(const track_desc_scalarf& desc) : scalar(desc) {}
			explicit desc_union(const track_desc_transformf& desc) : transform(desc) {}
		};

		desc_union				m_desc;				// The track description
	};

	//////////////////////////////////////////////////////////////////////////
	// A typed track of data. See `track` for details.
	//////////////////////////////////////////////////////////////////////////
	template<track_type8 track_type_>
	class track_typed final : public track
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// The track type.
		static constexpr track_type8 type = track_type_;

		//////////////////////////////////////////////////////////////////////////
		// The track category.
		static constexpr track_category8 category = track_traits<track_type_>::category;

		//////////////////////////////////////////////////////////////////////////
		// The type of each sample in this track.
		using sample_type = typename track_traits<track_type_>::sample_type;

		//////////////////////////////////////////////////////////////////////////
		// The type of the track description.
		using desc_type = typename track_traits<track_type_>::desc_type;

		//////////////////////////////////////////////////////////////////////////
		// Constructs an empty typed track.
		track_typed() noexcept : track(type, category) { static_assert(sizeof(track_typed) == sizeof(track), "You cannot add member variables to this class"); }

		//////////////////////////////////////////////////////////////////////////
		// Destroys the track and potentially frees any memory it might own.
		~track_typed() = default;

		//////////////////////////////////////////////////////////////////////////
		// Move assignment for a track.
		track_typed(track_typed&& other) noexcept : track(static_cast<track&&>(other)) {}

		//////////////////////////////////////////////////////////////////////////
		// Move assignment for a track.
		track_typed& operator=(track_typed&& other) noexcept { return static_cast<track_typed&>(track::operator=(static_cast<track&&>(other))); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the sample at the specified index.
		// If this track does not own the memory, mutable references aren't allowed and an
		// invalid reference will be returned, leading to a crash.
		sample_type& operator[](uint32_t index);

		//////////////////////////////////////////////////////////////////////////
		// Returns the sample at the specified index.
		const sample_type& operator[](uint32_t index) const;

		//////////////////////////////////////////////////////////////////////////
		// Returns the track description.
		desc_type& get_description();

		//////////////////////////////////////////////////////////////////////////
		// Returns the track description.
		const desc_type& get_description() const;

		//////////////////////////////////////////////////////////////////////////
		// Returns the track type.
		track_type8 get_type() const { return type; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the track category.
		track_category8 get_category() const { return category; }

		//////////////////////////////////////////////////////////////////////////
		// Returns a copy of the track where the memory will be owned by the copy.
		track_typed get_copy(IAllocator& allocator) const;

		//////////////////////////////////////////////////////////////////////////
		// Returns a reference to the track where the memory isn't owned.
		track_typed get_ref() const;

		//////////////////////////////////////////////////////////////////////////
		// Creates a track that copies the data and owns the memory.
		static track_typed<track_type_> make_copy(const desc_type& desc, IAllocator& allocator, const sample_type* data, uint32_t num_samples, float sample_rate, uint32_t stride = sizeof(sample_type));

		//////////////////////////////////////////////////////////////////////////
		// Creates a track and preallocates but does not initialize the memory that it owns.
		static track_typed<track_type_> make_reserve(const desc_type& desc, IAllocator& allocator, uint32_t num_samples, float sample_rate);

		//////////////////////////////////////////////////////////////////////////
		// Creates a track and takes ownership of the already allocated memory.
		static track_typed<track_type_> make_owner(const desc_type& desc, IAllocator& allocator, sample_type* data, uint32_t num_samples, float sample_rate, uint32_t stride = sizeof(sample_type));

		//////////////////////////////////////////////////////////////////////////
		// Creates a track that just references the data without owning it.
		static track_typed<track_type_> make_ref(const desc_type& desc, const sample_type* data, uint32_t num_samples, float sample_rate, uint32_t stride = sizeof(sample_type));

	private:
		//////////////////////////////////////////////////////////////////////////
		// We prohibit copying, use get_copy() and get_ref() instead.
		track_typed(const track_typed&) = delete;
		track_typed& operator=(const track_typed&) = delete;

		//////////////////////////////////////////////////////////////////////////
		// Internal constructor.
		track_typed(IAllocator* allocator, uint8_t* data, uint32_t num_samples, uint32_t stride, size_t data_size, float sample_rate, const desc_type& desc) noexcept;
	};

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track into the desired track type while asserting for safety.
	template<typename track_type>
	inline track_type& track_cast(track& track_)
	{
		ACL_ASSERT(track_type::type == track_.get_type() || track_.get_num_samples() == 0, "Unexpected track type");
		return static_cast<track_type&>(track_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track into the desired track type while asserting for safety.
	template<typename track_type>
	inline const track_type& track_cast(const track& track_)
	{
		ACL_ASSERT(track_type::type == track_.get_type() || track_.get_num_samples() == 0, "Unexpected track type");
		return static_cast<const track_type&>(track_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track into the desired track type. Returns nullptr if the types
	// are not compatible or if the input is nullptr.
	template<typename track_type>
	inline track_type* track_cast(track* track_)
	{
		if (track_ == nullptr || (track_type::type != track_->get_type() && track_->get_num_samples() != 0))
			return nullptr;

		return static_cast<track_type*>(track_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts an untyped track into the desired track type. Returns nullptr if the types
	// are not compatible or if the input is nullptr.
	template<typename track_type>
	inline const track_type* track_cast(const track* track_)
	{
		if (track_ == nullptr || (track_type::type != track_->get_type() && track_->get_num_samples() != 0))
			return nullptr;

		return static_cast<const track_type*>(track_);
	}

	//////////////////////////////////////////////////////////////////////////
	// Create aliases for the various typed track types.

	using track_float1f			= track_typed<track_type8::float1f>;
	using track_float2f			= track_typed<track_type8::float2f>;
	using track_float3f			= track_typed<track_type8::float3f>;
	using track_float4f			= track_typed<track_type8::float4f>;
	using track_vector4f		= track_typed<track_type8::vector4f>;
	using track_qvvf			= track_typed<track_type8::qvvf>;

	//////////////////////////////////////////////////////////////////////////
	// Implementation

	inline track::track() noexcept
		: m_allocator(nullptr)
		, m_data(nullptr)
		, m_num_samples(0)
		, m_stride(0)
		, m_data_size(0)
		, m_sample_rate(0.0F)
		, m_type(track_type8::float1f)
		, m_category(track_category8::scalarf)
		, m_sample_size(0)
		, m_desc()
	{}

	inline track::track(track&& other) noexcept
		: m_allocator(other.m_allocator)
		, m_data(other.m_data)
		, m_num_samples(other.m_num_samples)
		, m_stride(other.m_stride)
		, m_data_size(other.m_data_size)
		, m_sample_rate(other.m_sample_rate)
		, m_type(other.m_type)
		, m_category(other.m_category)
		, m_sample_size(other.m_sample_size)
		, m_desc(other.m_desc)
	{
		other.m_allocator = nullptr;
		other.m_data = nullptr;
	}

	inline track::~track()
	{
		if (is_owner())
		{
			// We own the memory, free it
			m_allocator->deallocate(m_data, m_data_size);
		}
	}

	inline track& track::operator=(track&& other) noexcept
	{
		std::swap(m_allocator, other.m_allocator);
		std::swap(m_data, other.m_data);
		std::swap(m_num_samples, other.m_num_samples);
		std::swap(m_stride, other.m_stride);
		std::swap(m_data_size, other.m_data_size);
		std::swap(m_sample_rate, other.m_sample_rate);
		std::swap(m_type, other.m_type);
		std::swap(m_category, other.m_category);
		std::swap(m_sample_size, other.m_sample_size);
		std::swap(m_desc, other.m_desc);
		return *this;
	}

	inline void* track::operator[](uint32_t index)
	{
		ACL_ASSERT(index < m_num_samples, "Invalid sample index. %u >= %u", index, m_num_samples);
		return m_data + (index * m_stride);
	}

	inline const void* track::operator[](uint32_t index) const
	{
		ACL_ASSERT(index < m_num_samples, "Invalid sample index. %u >= %u", index, m_num_samples);
		return m_data + (index * m_stride);
	}

	inline uint32_t track::get_output_index() const
	{
		switch (m_category)
		{
		default:
		case track_category8::scalarf:		return m_desc.scalar.output_index;
		case track_category8::transformf:	return m_desc.transform.output_index;
		}
	}

	template<>
	inline track_desc_scalarf& track::get_description()
	{
		ACL_ASSERT(track_desc_scalarf::category == m_category, "Unexpected track category");
		return m_desc.scalar;
	}

	template<>
	inline track_desc_transformf& track::get_description()
	{
		ACL_ASSERT(track_desc_transformf::category == m_category, "Unexpected track category");
		return m_desc.transform;
	}

	template<>
	inline const track_desc_scalarf& track::get_description() const
	{
		ACL_ASSERT(track_desc_scalarf::category == m_category, "Unexpected track category");
		return m_desc.scalar;
	}

	template<>
	inline const track_desc_transformf& track::get_description() const
	{
		ACL_ASSERT(track_desc_transformf::category == m_category, "Unexpected track category");
		return m_desc.transform;
	}

	inline track track::get_copy(IAllocator& allocator) const
	{
		track track_;
		get_copy_impl(allocator, track_);
		return track_;
	}

	inline track track::get_ref() const
	{
		track track_;
		get_ref_impl(track_);
		return track_;
	}

	inline ErrorResult track::is_valid() const
	{
		if (m_data == nullptr)
			return ErrorResult();

		if (m_num_samples == 0xFFFFFFFFU)
			return ErrorResult("Too many samples");

		if (m_sample_rate <= 0.0F || !rtm::scalar_is_finite(m_sample_rate))
			return ErrorResult("Invalid sample rate");

		switch (m_category)
		{
		case track_category8::scalarf:		return m_desc.scalar.is_valid();
		case track_category8::transformf:	return m_desc.transform.is_valid();
		default:							return ErrorResult("Invalid category");
		}
	}

	inline track::track(track_type8 type, track_category8 category) noexcept
		: m_allocator(nullptr)
		, m_data(nullptr)
		, m_num_samples(0)
		, m_stride(0)
		, m_data_size(0)
		, m_sample_rate(0.0F)
		, m_type(type)
		, m_category(category)
		, m_sample_size(0)
		, m_desc()
	{}

	inline track::track(IAllocator* allocator, uint8_t* data, uint32_t num_samples, uint32_t stride, size_t data_size, float sample_rate, track_type8 type, track_category8 category, uint8_t sample_size) noexcept
		: m_allocator(allocator)
		, m_data(data)
		, m_num_samples(num_samples)
		, m_stride(stride)
		, m_data_size(data_size)
		, m_sample_rate(sample_rate)
		, m_type(type)
		, m_category(category)
		, m_sample_size(sample_size)
		, m_desc()
	{}

	inline void track::get_copy_impl(IAllocator& allocator, track& out_track) const
	{
		out_track.m_allocator = &allocator;
		out_track.m_data = reinterpret_cast<uint8_t*>(allocator.allocate(m_data_size));
		out_track.m_num_samples = m_num_samples;
		out_track.m_stride = m_stride;
		out_track.m_data_size = m_data_size;
		out_track.m_sample_rate = m_sample_rate;
		out_track.m_type = m_type;
		out_track.m_category = m_category;
		out_track.m_sample_size = m_sample_size;
		out_track.m_desc = m_desc;

		std::memcpy(out_track.m_data, m_data, m_data_size);
	}

	inline void track::get_ref_impl(track& out_track) const
	{
		out_track.m_allocator = nullptr;
		out_track.m_data = m_data;
		out_track.m_num_samples = m_num_samples;
		out_track.m_stride = m_stride;
		out_track.m_data_size = m_data_size;
		out_track.m_sample_rate = m_sample_rate;
		out_track.m_type = m_type;
		out_track.m_category = m_category;
		out_track.m_sample_size = m_sample_size;
		out_track.m_desc = m_desc;
	}

	template<track_type8 track_type_>
	inline typename track_typed<track_type_>::sample_type& track_typed<track_type_>::operator[](uint32_t index)
	{
		ACL_ASSERT(index < m_num_samples, "Invalid sample index. %u >= %u", index, m_num_samples);
		return *reinterpret_cast<sample_type*>(m_data + (index * m_stride));
	}

	template<track_type8 track_type_>
	inline const typename track_typed<track_type_>::sample_type& track_typed<track_type_>::operator[](uint32_t index) const
	{
		ACL_ASSERT(index < m_num_samples, "Invalid sample index. %u >= %u", index, m_num_samples);
		return *reinterpret_cast<const sample_type*>(m_data + (index * m_stride));
	}

	template<track_type8 track_type_>
	inline typename track_typed<track_type_>::desc_type& track_typed<track_type_>::get_description()
	{
		return track::get_description<desc_type>();
	}

	template<track_type8 track_type_>
	inline const typename track_typed<track_type_>::desc_type& track_typed<track_type_>::get_description() const
	{
		return track::get_description<desc_type>();
	}

	template<track_type8 track_type_>
	inline track_typed<track_type_> track_typed<track_type_>::get_copy(IAllocator& allocator) const
	{
		track_typed track_;
		track::get_copy_impl(allocator, track_);
		return track_;
	}

	template<track_type8 track_type_>
	inline track_typed<track_type_> track_typed<track_type_>::get_ref() const
	{
		track_typed track_;
		track::get_ref_impl(track_);
		return track_;
	}

	template<track_type8 track_type_>
	inline track_typed<track_type_> track_typed<track_type_>::make_copy(const typename track_typed<track_type_>::desc_type& desc, IAllocator& allocator, const sample_type* data, uint32_t num_samples, float sample_rate, uint32_t stride)
	{
		const size_t data_size = size_t(num_samples) * sizeof(sample_type);
		const uint8_t* data_raw = reinterpret_cast<const uint8_t*>(data);

		// Copy the data manually to avoid preserving the stride
		sample_type* data_copy = reinterpret_cast<sample_type*>(allocator.allocate(data_size));
		for (uint32_t index = 0; index < num_samples; ++index)
			data_copy[index] = *reinterpret_cast<const sample_type*>(data_raw + (index * stride));

		return track_typed<track_type_>(&allocator, reinterpret_cast<uint8_t*>(data_copy), num_samples, sizeof(sample_type), data_size, sample_rate, desc);
	}

	template<track_type8 track_type_>
	inline track_typed<track_type_> track_typed<track_type_>::make_reserve(const typename track_typed<track_type_>::desc_type& desc, IAllocator& allocator, uint32_t num_samples, float sample_rate)
	{
		const size_t data_size = size_t(num_samples) * sizeof(sample_type);
		return track_typed<track_type_>(&allocator, reinterpret_cast<uint8_t*>(allocator.allocate(data_size)), num_samples, sizeof(sample_type), data_size, sample_rate, desc);
	}

	template<track_type8 track_type_>
	inline track_typed<track_type_> track_typed<track_type_>::make_owner(const typename track_typed<track_type_>::desc_type& desc, IAllocator& allocator, sample_type* data, uint32_t num_samples, float sample_rate, uint32_t stride)
	{
		const size_t data_size = size_t(num_samples) * stride;
		return track_typed<track_type_>(&allocator, reinterpret_cast<uint8_t*>(data), num_samples, stride, data_size, sample_rate, desc);
	}

	template<track_type8 track_type_>
	inline track_typed<track_type_> track_typed<track_type_>::make_ref(const typename track_typed<track_type_>::desc_type& desc, const sample_type* data, uint32_t num_samples, float sample_rate, uint32_t stride)
	{
		const size_t data_size = size_t(num_samples) * stride;
		return track_typed<track_type_>(nullptr, const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(data)), num_samples, stride, data_size, sample_rate, desc);
	}

	template<track_type8 track_type_>
	inline track_typed<track_type_>::track_typed(IAllocator* allocator, uint8_t* data, uint32_t num_samples, uint32_t stride, size_t data_size, float sample_rate, const typename track_typed<track_type_>::desc_type& desc) noexcept
		: track(allocator, data, num_samples, stride, data_size, sample_rate, type, category, sizeof(sample_type))
	{
		m_desc = desc_union(desc);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
