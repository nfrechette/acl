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
#include "acl/core/assert.h"
#include "acl/core/utils.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_64.h"
#include "acl/compression/animation_track_range.h"

#include <stdint.h>
#include <utility>

namespace acl
{
	static constexpr double		TRACK_CONSTANT_THRESHOLD			= 0.00001;

	// TODO: Bake tracks before we start compressing. Calculate the range, if it is constant, default, etc.

	class AnimationTrack
	{
	public:
		bool is_initialized() const { return m_allocator != nullptr; }

		uint32_t get_num_samples() const { return m_num_samples; }

		AnimationTrackRange get_range() const
		{
			if (m_is_range_dirty)
			{
				m_range = calculate_range();
				m_is_range_dirty = false;
			}

			return m_range;
		}

		bool is_constant(double threshold = TRACK_CONSTANT_THRESHOLD) const { return get_range().is_constant(threshold); }

	protected:
		enum class AnimationTrackType : uint8_t
		{
			Rotation = 0,
			Translation = 1,
			// TODO: Scale
		};

		AnimationTrack()
			: m_allocator(nullptr)
			, m_sample_data(nullptr)
			, m_time_data(nullptr)
			, m_num_samples(0)
			, m_sample_rate(0)
			, m_type(AnimationTrackType::Rotation)
		{}

		AnimationTrack(AnimationTrack&& track)
			: m_allocator(track.m_allocator)
			, m_sample_data(track.m_sample_data)
			, m_time_data(track.m_time_data)
			, m_num_samples(track.m_num_samples)
			, m_sample_rate(track.m_sample_rate)
			, m_is_range_dirty(track.m_is_range_dirty)
			, m_type(track.m_type)
			, m_range(track.m_range)
		{}

		AnimationTrack(Allocator& allocator, uint32_t num_samples, uint32_t sample_rate, AnimationTrackType type)
			: m_allocator(&allocator)
			, m_sample_data(allocate_type_array<double>(allocator, num_samples * get_animation_track_sample_size(type)))
			, m_time_data(allocate_type_array<double>(allocator, num_samples))
			, m_num_samples(num_samples)
			, m_sample_rate(sample_rate)
			, m_is_range_dirty(true)
			, m_type(type)
			, m_range(AnimationTrackRange())
		{}

		~AnimationTrack()
		{
			if (is_initialized())
			{
				m_allocator->deallocate(m_sample_data);
				m_allocator->deallocate(m_time_data);
			}
		}

		AnimationTrack& operator=(AnimationTrack&& track)
		{
			std::swap(m_allocator, track.m_allocator);
			std::swap(m_sample_data, track.m_sample_data);
			std::swap(m_time_data, track.m_time_data);
			std::swap(m_num_samples, track.m_num_samples);
			std::swap(m_sample_rate, track.m_sample_rate);
			std::swap(m_is_range_dirty, track.m_is_range_dirty);
			std::swap(m_type, track.m_type);
			std::swap(m_range, track.m_range);
			return *this;
		}

		AnimationTrack(const AnimationTrack&) = delete;
		AnimationTrack& operator=(const AnimationTrack&) = delete;

		AnimationTrackRange calculate_range() const
		{
			ensure(is_initialized());

			if (m_num_samples == 0)
				return AnimationTrackRange();

			size_t sample_size = get_animation_track_sample_size(m_type);

			uint32_t sample_index = 0;
			const double* sample = &m_sample_data[sample_index * sample_size];

			double x = sample[0];
			double y = sample[1];
			double z = sample[2];
			// TODO: Add padding and avoid the branch altogether
			double w = sample_size == 4 ? sample[3] : z;	// Constant branch, trivially predicted

			Vector4_64 value = vector_set(x, y, z, w);

			Vector4_64 min = value;
			Vector4_64 max = value;

			for (sample_index = 1; sample_index < m_num_samples; ++sample_index)
			{
				sample = &m_sample_data[sample_index * sample_size];

				x = sample[0];
				y = sample[1];
				z = sample[2];
				// TODO: Add padding and avoid the branch altogether
				w = sample_size == 4 ? sample[3] : z;	// Constant branch, trivially predicted

				value = vector_set(x, y, z, w);

				min = vector_min(min, value);
				max = vector_max(max, value);
			}

			return AnimationTrackRange(min, max);
		}

		// TODO: constexpr
		// Returns the number of values per sample
		static inline size_t get_animation_track_sample_size(AnimationTrackType type)
		{
			switch (type)
			{
			default:
			case AnimationTrackType::Rotation:		return 4;
			case AnimationTrackType::Translation:	return 3;
			}
		}

		Allocator*						m_allocator;
		double*							m_sample_data;
		double*							m_time_data;

		uint32_t						m_num_samples;
		uint32_t						m_sample_rate;
		mutable bool					m_is_range_dirty;		// TODO: Do we really need to cache this? nasty with mutable...

		AnimationTrackType				m_type;

		mutable AnimationTrackRange		m_range;

		// TODO: Support different sampling methods: linear, cubic
	};

	class AnimationRotationTrack : public AnimationTrack
	{
	public:
		AnimationRotationTrack()
			: AnimationTrack()
		{}

		AnimationRotationTrack(Allocator& allocator, uint32_t num_samples, uint32_t sample_rate)
			: AnimationTrack(allocator, num_samples, sample_rate, AnimationTrackType::Rotation)
		{}

		AnimationRotationTrack(AnimationRotationTrack&& track)
			: AnimationTrack(std::forward<AnimationTrack>(track))
		{}

		AnimationRotationTrack& operator=(AnimationRotationTrack&& track)
		{
			AnimationTrack::operator=(std::forward<AnimationTrack>(track));
			return *this;
		}

		bool is_default(double threshold = TRACK_CONSTANT_THRESHOLD) const
		{
			AnimationTrackRange range = get_range();
			if (!range.is_constant(threshold))
				return false;

			// For a rotation track, the extent only tells us if the track is constant or not
			// since the min/max we maintain aren't valid rotations.
			// Similarly, the center isn't a valid rotation and is meaningless.
			Quat_64 sample0 = get_sample(0);
			double angle = quat_get_angle(sample0);

			return abs(angle) < threshold;
		}

		bool is_animated(double threshold = TRACK_CONSTANT_THRESHOLD) const
		{
			return !is_constant(threshold) && !is_default(threshold);
		}

		void set_sample(uint32_t sample_index, const Quat_64& rotation, double sample_time)
		{
			ensure(is_initialized());

			size_t sample_size = get_animation_track_sample_size(m_type);
			ensure(sample_size == 4);

			double* sample = &m_sample_data[sample_index * sample_size];
			sample[0] = quat_get_x(rotation);
			sample[1] = quat_get_y(rotation);
			sample[2] = quat_get_z(rotation);
			sample[3] = quat_get_w(rotation);

			m_time_data[sample_index] = sample_time;
			m_is_range_dirty = true;
		}

		Quat_64 get_sample(uint32_t sample_index) const
		{
			ensure(is_initialized());
			ensure(m_type == AnimationTrackType::Rotation);

			size_t sample_size = get_animation_track_sample_size(m_type);

			const double* sample = &m_sample_data[sample_index * sample_size];
			return quat_unaligned_load(sample);
		}

		Quat_64 sample_track(double sample_time) const
		{
			double track_duration = double(m_num_samples - 1) / double(m_sample_rate);

			uint32_t sample_frame0;
			uint32_t sample_frame1;
			double interpolation_alpha;
			calculate_interpolation_keys(m_num_samples, track_duration, sample_time, sample_frame0, sample_frame1, interpolation_alpha);

			Quat_64 sample0 = get_sample(sample_frame0);
			Quat_64 sample1 = get_sample(sample_frame1);
			return quat_lerp(sample0, sample1, interpolation_alpha);
		}

		AnimationRotationTrack(const AnimationRotationTrack&) = delete;
		AnimationRotationTrack& operator=(const AnimationRotationTrack&) = delete;
	};

	class AnimationTranslationTrack : public AnimationTrack
	{
	public:
		AnimationTranslationTrack()
			: AnimationTrack()
		{}

		AnimationTranslationTrack(Allocator& allocator, uint32_t num_samples, uint32_t sample_rate)
			: AnimationTrack(allocator, num_samples, sample_rate, AnimationTrackType::Translation)
		{}

		AnimationTranslationTrack(AnimationTranslationTrack&& track)
			: AnimationTrack(std::forward<AnimationTrack>(track))
		{}

		AnimationTranslationTrack& operator=(AnimationTranslationTrack&& track)
		{
			AnimationTrack::operator=(std::forward<AnimationTrack>(track));
			return *this;
		}

		bool is_default(double threshold = TRACK_CONSTANT_THRESHOLD) const
		{
			AnimationTrackRange range = get_range();
			if (!range.is_constant(threshold))
				return false;

			double distance = vector_length3(range.get_center());

			return distance < threshold;
		}

		bool is_animated(double threshold = TRACK_CONSTANT_THRESHOLD) const
		{
			return !is_constant(threshold) && !is_default(threshold);
		}

		void set_sample(uint32_t sample_index, const Vector4_64& translation, double sample_time)
		{
			ensure(is_initialized());

			size_t sample_size = get_animation_track_sample_size(m_type);
			ensure(sample_size == 3);

			double* sample = &m_sample_data[sample_index * sample_size];
			sample[0] = vector_get_x(translation);
			sample[1] = vector_get_y(translation);
			sample[2] = vector_get_z(translation);

			m_time_data[sample_index] = sample_time;
			m_is_range_dirty = true;
		}

		Vector4_64 get_sample(uint32_t sample_index) const
		{
			ensure(is_initialized());
			ensure(m_type == AnimationTrackType::Translation);

			size_t sample_size = get_animation_track_sample_size(m_type);

			const double* sample = &m_sample_data[sample_index * sample_size];
			return vector_unaligned_load3(sample);
		}

		Vector4_64 sample_track(double sample_time) const
		{
			double track_duration = double(m_num_samples - 1) / double(m_sample_rate);

			uint32_t sample_frame0;
			uint32_t sample_frame1;
			double interpolation_alpha;
			calculate_interpolation_keys(m_num_samples, track_duration, sample_time, sample_frame0, sample_frame1, interpolation_alpha);

			Vector4_64 sample0 = get_sample(sample_frame0);
			Vector4_64 sample1 = get_sample(sample_frame1);
			return vector_lerp(sample0, sample1, interpolation_alpha);
		}

		AnimationTranslationTrack(const AnimationTranslationTrack&) = delete;
		AnimationTranslationTrack& operator=(const AnimationTranslationTrack&) = delete;
	};
}
