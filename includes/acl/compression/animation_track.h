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

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/iallocator.h"
#include "acl/core/error.h"
#include "acl/core/track_types.h"

#include <rtm/quatd.h>
#include <rtm/vector4d.h>

#include <cstdint>
#include <utility>

// VS2015 sometimes dies when it attempts to compile too many inlined functions
#if defined(_MSC_VER)
	#define VS2015_HACK_NO_INLINE __declspec(noinline)
#else
	#define VS2015_HACK_NO_INLINE
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// A raw animation track.
	//
	// This is the base class for the three track types: rotation, translation, and scale.
	// It holds and owns the raw data.
	//////////////////////////////////////////////////////////////////////////
	class AnimationTrack
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// Returns if the animation track has been initialized or not
		bool is_initialized() const { return m_allocator != nullptr; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of samples in this track
		uint32_t get_num_samples() const { return m_num_samples; }

	protected:
		AnimationTrack() noexcept
			: m_allocator(nullptr)
			, m_sample_data(nullptr)
			, m_num_samples(0)
			, m_sample_rate(0.0F)
			, m_type(animation_track_type8::rotation)
		{}

		AnimationTrack(AnimationTrack&& other) noexcept
			: m_allocator(other.m_allocator)
			, m_sample_data(other.m_sample_data)
			, m_num_samples(other.m_num_samples)
			, m_sample_rate(other.m_sample_rate)
			, m_type(other.m_type)
		{
			// Safe because our derived classes do not add any data and aren't virtual
			new(&other) AnimationTrack();
		}

		//////////////////////////////////////////////////////////////////////////
		// Constructs a new track instance
		//    - allocator: The allocator instance to use to allocate and free memory
		//    - num_samples: The number of samples in this track
		//    - sample_rate: The rate at which samples are recorded (e.g. 30 means 30 FPS)
		//    - type: The track type
		AnimationTrack(IAllocator& allocator, uint32_t num_samples, float sample_rate, animation_track_type8 type)
			: m_allocator(&allocator)
			, m_sample_data(allocate_type_array_aligned<double>(allocator, size_t(num_samples) * get_animation_track_sample_size(type), alignof(rtm::vector4d)))
			, m_num_samples(num_samples)
			, m_sample_rate(sample_rate)
			, m_type(type)
		{}

		~AnimationTrack()
		{
			if (is_initialized())
				deallocate_type_array(*m_allocator, m_sample_data, size_t(m_num_samples) * get_animation_track_sample_size(m_type));
		}

		AnimationTrack& operator=(AnimationTrack&& track) noexcept
		{
			std::swap(m_allocator, track.m_allocator);
			std::swap(m_sample_data, track.m_sample_data);
			std::swap(m_num_samples, track.m_num_samples);
			std::swap(m_sample_rate, track.m_sample_rate);
			std::swap(m_type, track.m_type);
			return *this;
		}

		AnimationTrack(const AnimationTrack&) = delete;
		AnimationTrack& operator=(const AnimationTrack&) = delete;

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of values per sample
		// TODO: constexpr
		static uint32_t get_animation_track_sample_size(animation_track_type8 type)
		{
			switch (type)
			{
			default:
			case animation_track_type8::rotation:		return 4;
			case animation_track_type8::translation:	return 3;
			case animation_track_type8::scale:		return 3;
			}
		}

		// The allocator instance used to allocate and free memory by this clip instance
		IAllocator*						m_allocator;

		// The raw track data. There are 'get_animation_track_sample_size(m_type)' entries.
		double*							m_sample_data;

		// The number of samples in this track
		uint32_t						m_num_samples;

		// The rate at which the samples were recorded
		float							m_sample_rate;

		// The track type
		animation_track_type8				m_type;
	};

	//////////////////////////////////////////////////////////////////////////
	// A raw rotation track.
	//
	// Holds a track made of 'rtm::quatd' entries.
	//////////////////////////////////////////////////////////////////////////
	class AnimationRotationTrack final : public AnimationTrack
	{
	public:
		AnimationRotationTrack() noexcept : AnimationTrack() {}
		~AnimationRotationTrack() = default;

		//////////////////////////////////////////////////////////////////////////
		// Constructs a new rotation track instance
		//    - allocator: The allocator instance to use to allocate and free memory
		//    - num_samples: The number of samples in this track
		//    - sample_rate: The rate at which samples are recorded (e.g. 30 means 30 FPS)
		AnimationRotationTrack(IAllocator& allocator, uint32_t num_samples, float sample_rate)
			: AnimationTrack(allocator, num_samples, sample_rate, animation_track_type8::rotation)
		{
			rtm::quatd* samples = safe_ptr_cast<rtm::quatd>(&m_sample_data[0]);
			std::fill(samples, samples + num_samples, rtm::quat_identity());
		}

		AnimationRotationTrack(AnimationRotationTrack&& other) noexcept
			: AnimationTrack(static_cast<AnimationTrack&&>(other))
		{}

		AnimationRotationTrack& operator=(AnimationRotationTrack&& track) noexcept
		{
			AnimationTrack::operator=(static_cast<AnimationTrack&&>(track));
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////
		// Sets a sample value at a particular index
		VS2015_HACK_NO_INLINE void set_sample(uint32_t sample_index, const rtm::quatd& rotation)
		{
			ACL_ASSERT(is_initialized(), "Track is not initialized");
			ACL_ASSERT(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);
			ACL_ASSERT(rtm::quat_is_finite(rotation), "Invalid rotation: [%f, %f, %f, %f]", (float)rtm::quat_get_x(rotation), (float)rtm::quat_get_y(rotation), (float)rtm::quat_get_z(rotation), (float)rtm::quat_get_w(rotation));
			ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation not normalized: [%f, %f, %f, %f]", (float)rtm::quat_get_x(rotation), (float)rtm::quat_get_y(rotation), (float)rtm::quat_get_z(rotation), (float)rtm::quat_get_w(rotation));

			const uint32_t sample_size = get_animation_track_sample_size(m_type);
			ACL_ASSERT(sample_size == 4, "Invalid sample size. %u != 4", sample_size);

			double* sample = &m_sample_data[sample_index * sample_size];
			rtm::quat_store(rotation, sample);
		}

		//////////////////////////////////////////////////////////////////////////
		// Retrieves a sample value at a particular index
		rtm::quatd get_sample(uint32_t sample_index) const
		{
			ACL_ASSERT(is_initialized(), "Track is not initialized");
			ACL_ASSERT(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);

			const uint32_t sample_size = get_animation_track_sample_size(m_type);

			const double* sample = &m_sample_data[sample_index * sample_size];
			return rtm::quat_load(sample);
		}

		AnimationRotationTrack(const AnimationRotationTrack&) = delete;
		AnimationRotationTrack& operator=(const AnimationRotationTrack&) = delete;
	};

	//////////////////////////////////////////////////////////////////////////
	// A raw translation track.
	//
	// Holds a track made of 3x 'double' entries.
	//////////////////////////////////////////////////////////////////////////
	class AnimationTranslationTrack final : public AnimationTrack
	{
	public:
		AnimationTranslationTrack() noexcept : AnimationTrack() {}
		~AnimationTranslationTrack() = default;

		//////////////////////////////////////////////////////////////////////////
		// Constructs a new translation track instance
		//    - allocator: The allocator instance to use to allocate and free memory
		//    - num_samples: The number of samples in this track
		//    - sample_rate: The rate at which samples are recorded (e.g. 30 means 30 FPS)
		AnimationTranslationTrack(IAllocator& allocator, uint32_t num_samples, float sample_rate)
			: AnimationTrack(allocator, num_samples, sample_rate, animation_track_type8::translation)
		{
			std::fill(m_sample_data, m_sample_data + (num_samples * 3), 0.0);
		}

		AnimationTranslationTrack(AnimationTranslationTrack&& other) noexcept
			: AnimationTrack(static_cast<AnimationTrack&&>(other))
		{}

		AnimationTranslationTrack& operator=(AnimationTranslationTrack&& track) noexcept
		{
			AnimationTrack::operator=(static_cast<AnimationTrack&&>(track));
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////
		// Sets a sample value at a particular index
		void set_sample(uint32_t sample_index, const rtm::vector4d& translation)
		{
			ACL_ASSERT(is_initialized(), "Track is not initialized");
			ACL_ASSERT(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);
			ACL_ASSERT(rtm::vector_is_finite3(translation), "Invalid translation: [%f, %f, %f]", (float)rtm::vector_get_x(translation), (float)rtm::vector_get_y(translation), (float)rtm::vector_get_z(translation));

			const uint32_t sample_size = get_animation_track_sample_size(m_type);
			ACL_ASSERT(sample_size == 3, "Invalid sample size. %u != 3", sample_size);

			double* sample = &m_sample_data[sample_index * sample_size];
			rtm::vector_store3(translation, sample);
		}

		//////////////////////////////////////////////////////////////////////////
		// Retrieves a sample value at a particular index
		rtm::vector4d get_sample(uint32_t sample_index) const
		{
			ACL_ASSERT(is_initialized(), "Track is not initialized");
			ACL_ASSERT(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);

			const uint32_t sample_size = get_animation_track_sample_size(m_type);

			const double* sample = &m_sample_data[sample_index * sample_size];
			return rtm::vector_load3(sample);
		}

		AnimationTranslationTrack(const AnimationTranslationTrack&) = delete;
		AnimationTranslationTrack& operator=(const AnimationTranslationTrack&) = delete;
	};

	//////////////////////////////////////////////////////////////////////////
	// A raw scale track.
	//
	// Holds a track made of 3x 'double' entries.
	//////////////////////////////////////////////////////////////////////////
	class AnimationScaleTrack final : public AnimationTrack
	{
	public:
		AnimationScaleTrack() noexcept : AnimationTrack() {}
		~AnimationScaleTrack() = default;

		//////////////////////////////////////////////////////////////////////////
		// Constructs a new scale track instance
		//    - allocator: The allocator instance to use to allocate and free memory
		//    - num_samples: The number of samples in this track
		//    - sample_rate: The rate at which samples are recorded (e.g. 30 means 30 FPS)
		AnimationScaleTrack(IAllocator& allocator, uint32_t num_samples, float sample_rate)
			: AnimationTrack(allocator, num_samples, sample_rate, animation_track_type8::scale)
		{
			rtm::vector4d defaultScale = rtm::vector_set(1.0);
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				rtm::vector_store3(defaultScale, m_sample_data + (sample_index * 3));
		}

		AnimationScaleTrack(AnimationScaleTrack&& other) noexcept
			: AnimationTrack(static_cast<AnimationTrack&&>(other))
		{}

		AnimationScaleTrack& operator=(AnimationScaleTrack&& track) noexcept
		{
			AnimationTrack::operator=(static_cast<AnimationTrack&&>(track));
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////
		// Sets a sample value at a particular index
		VS2015_HACK_NO_INLINE void set_sample(uint32_t sample_index, const rtm::vector4d& scale)
		{
			ACL_ASSERT(is_initialized(), "Track is not initialized");
			ACL_ASSERT(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);
			ACL_ASSERT(rtm::vector_is_finite3(scale), "Invalid scale: [%f, %f, %f]", (float)rtm::vector_get_x(scale), (float)rtm::vector_get_y(scale), (float)rtm::vector_get_z(scale));

			const uint32_t sample_size = get_animation_track_sample_size(m_type);
			ACL_ASSERT(sample_size == 3, "Invalid sample size. %u != 3", sample_size);

			double* sample = &m_sample_data[sample_index * sample_size];
			rtm::vector_store3(scale, sample);
		}

		//////////////////////////////////////////////////////////////////////////
		// Retrieves a sample value at a particular index
		rtm::vector4d get_sample(uint32_t sample_index) const
		{
			ACL_ASSERT(is_initialized(), "Track is not initialized");
			ACL_ASSERT(sample_index < m_num_samples, "Invalid sample index. %u >= %u", sample_index, m_num_samples);

			const uint32_t sample_size = get_animation_track_sample_size(m_type);

			const double* sample = &m_sample_data[sample_index * sample_size];
			return rtm::vector_load3(sample);
		}

		AnimationScaleTrack(const AnimationScaleTrack&) = delete;
		AnimationScaleTrack& operator=(const AnimationScaleTrack&) = delete;
	};
}

ACL_IMPL_FILE_PRAGMA_POP
