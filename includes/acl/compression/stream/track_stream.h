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

#include "acl/core/memory.h"
#include "acl/core/error.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_64.h"

#include <stdint.h>

namespace acl
{
	class TrackStream
	{
	public:
		TrackStream() : m_allocator(nullptr), m_samples(nullptr), m_num_samples(0), m_sample_size(0) {}
		TrackStream(Allocator& allocator, uint32_t num_samples, uint32_t sample_size)
			: m_allocator(&allocator)
			, m_samples(reinterpret_cast<uint8_t*>(allocator.allocate(sample_size * num_samples, 16)))
			, m_num_samples(num_samples)
			, m_sample_size(sample_size)
		{}
		TrackStream(const TrackStream&) = delete;

		TrackStream(TrackStream&& other)
			: m_allocator(other.m_allocator)
			, m_samples(other.m_samples)
			, m_num_samples(other.m_num_samples)
			, m_sample_size(other.m_sample_size)
		{}

		~TrackStream()
		{
			if (m_allocator != nullptr && m_num_samples != 0)
				m_allocator->deallocate(m_samples, m_sample_size * m_num_samples);
		}

		TrackStream& operator=(const TrackStream&) = delete;
		TrackStream& operator=(TrackStream&& rhs)
		{
			std::swap(m_allocator, rhs.m_allocator);
			std::swap(m_samples, rhs.m_samples);
			std::swap(m_num_samples, rhs.m_num_samples);
			std::swap(m_sample_size, rhs.m_sample_size);
			return *this;
		}

		uint8_t* get_sample_ptr(uint32_t sample_index)
		{
			ACL_ENSURE(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);
			uint32_t offset = sample_index * m_sample_size;
			return m_samples + offset;
		}

		const uint8_t* get_sample_ptr(uint32_t sample_index) const
		{
			ACL_ENSURE(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);
			uint32_t offset = sample_index * m_sample_size;
			return m_samples + offset;
		}

		template<typename SampleType>
		SampleType get_sample(uint32_t sample_index)
		{
			uint8_t* ptr = get_sample_ptr(sample_index);
			return *safe_ptr_cast<SampleType>(ptr);
		}

		template<typename SampleType>
		void set_sample(uint32_t sample_index, const SampleType& sample)
		{
			ACL_ENSURE(m_sample_size == sizeof(SampleType), "Unexpected sample size. %u != %u", m_sample_size, sizeof(SampleType));
			uint8_t* ptr = get_sample_ptr(sample_index);
			*safe_ptr_cast<SampleType>(ptr) = sample;
		}

		uint32_t get_num_samples() const { return m_num_samples; }
		uint32_t get_sample_size() const { return m_sample_size; }

	protected:
		Allocator*	m_allocator;
		uint8_t*	m_samples;
		uint32_t	m_num_samples;
		uint32_t	m_sample_size;
	};

	// For a rotation track, the extent only tells us if the track is constant or not
	// since the min/max we maintain aren't valid rotations.
	// Similarly, the center isn't a valid rotation and is meaningless.
	class TrackStreamRange
	{
	public:
		TrackStreamRange()
			: m_min(vector_set(0.0))
			, m_max(vector_set(0.0))
		{}

		TrackStreamRange(const Vector4_64& min, const Vector4_64& max)
			: m_min(min)
			, m_max(max)
		{}

		Vector4_64 get_min() const { return m_min; }
		Vector4_64 get_max() const { return m_max; }

		Vector4_64 get_center() const { return vector_mul(vector_add(m_max, m_min), 0.5); }
		Vector4_64 get_extent() const { return vector_sub(m_max, m_min); }

		bool is_constant(double threshold) const { return vector_all_less_than(vector_abs(vector_sub(m_max, m_min)), vector_set(threshold)); }

	private:
		Vector4_64	m_min;
		Vector4_64	m_max;
	};

	struct BoneStreams
	{
		TrackStream rotations;
		TrackStream translations;

		TrackStreamRange rotation_range;
		TrackStreamRange translation_range;

		bool is_rotation_constant;
		bool is_rotation_default;
		bool is_translation_constant;
		bool is_translation_default;
		bool are_rotations_normalized;
		bool are_translations_normalized;

		bool is_rotation_animated() const { return !is_rotation_constant && !is_rotation_default; }
		bool is_translation_animated() const { return !is_translation_constant && !is_translation_default; }
	};

}
