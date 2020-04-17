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

#include "acl/core/compiler_utils.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/error.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/track_traits.h"
#include "acl/core/track_types.h"
#include "acl/core/track_writer.h"
#include "acl/decompression/impl/track_sampling_impl.h"
#include "acl/math/rtm_casts.h"
#include "acl/math/vector4_packing.h"

#include <rtm/types.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Deriving from this struct and overriding these constexpr functions
	// allow you to control which code is stripped for maximum performance.
	// With these, you can:
	//    - Support only a subset of the formats and statically strip the rest
	//    - Force a single format and statically strip the rest
	//    - Decide all of this at runtime by not making the overrides constexpr
	//
	// By default, all formats are supported.
	//////////////////////////////////////////////////////////////////////////
	struct decompression_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// Whether or not to clamp the sample time when `seek(..)` is called. Defaults to true.
		constexpr bool clamp_sample_time() const { return true; }

		//////////////////////////////////////////////////////////////////////////
		// Whether or not the specified track type is supported. Defaults to true.
		// If a track type is statically known not to be supported, the compiler can strip
		// the associated code.
		constexpr bool is_track_type_supported(track_type8 /*type*/) const { return true; }

		// Whether to explicitly disable floating point exceptions during decompression.
		// This has a cost, exceptions are usually disabled globally and do not need to be
		// explicitly disabled during decompression.
		// We assume that floating point exceptions are already disabled by the caller.
		constexpr bool disable_fp_exeptions() const { return false; }
	};

	//////////////////////////////////////////////////////////////////////////
	// These are debug settings, everything is enabled and nothing is stripped.
	// It will have the worst performance but allows every feature.
	//////////////////////////////////////////////////////////////////////////
	struct debug_decompression_settings : decompression_settings {};

	//////////////////////////////////////////////////////////////////////////
	// These are the default settings. Only the generally optimal settings
	// are enabled and will offer the overall best performance.
	//////////////////////////////////////////////////////////////////////////
	struct default_decompression_settings : decompression_settings {};

	//////////////////////////////////////////////////////////////////////////
	// Decompression context for the uniformly sampled algorithm. The context
	// allows various decompression actions to be performed on a compressed track list.
	//
	// Both the constructor and destructor are public because it is safe to place
	// instances of this context on the stack or as member variables.
	//
	// This compression algorithm is the simplest by far and as such it offers
	// the fastest compression and decompression. Every sample is retained and
	// every track has the same number of samples playing back at the same
	// sample rate. This means that when we sample at a particular time within
	// the track list, we can trivially calculate the offsets required to read the
	// desired data. All the data is sorted in order to ensure all reads are
	// as contiguous as possible for optimal cache locality during decompression.
	//////////////////////////////////////////////////////////////////////////
	template<class decompression_settings_type>
	class decompression_context
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// Constructs a context instance with an optional allocator instance.
		// The default constructor for the `decompression_settings_type` will be used.
		// If an allocator is provided, it will be used in `release()` to free the context.
		explicit decompression_context(IAllocator* allocator = nullptr);

		//////////////////////////////////////////////////////////////////////////
		// Constructs a context instance from a set of static settings and an optional allocator instance.
		// If an allocator is provided, it will be used in `release()` to free the context.
		decompression_context(const decompression_settings_type& settings, IAllocator* allocator = nullptr);

		//////////////////////////////////////////////////////////////////////////
		// Destructs a context instance.
		~decompression_context();

		//////////////////////////////////////////////////////////////////////////
		// Initializes the context instance to a particular compressed tracks instance.
		void initialize(const compressed_tracks& tracks);

		bool is_dirty(const compressed_tracks& tracks);

		//////////////////////////////////////////////////////////////////////////
		// Seeks within the compressed tracks to a particular point in time with the
		// desired rounding policy.
		void seek(float sample_time, SampleRoundingPolicy rounding_policy);

		//////////////////////////////////////////////////////////////////////////
		// Decompress every track at the current sample time.
		// The track_writer_type allows complete control over how the tracks are written out.
		template<class track_writer_type>
		void decompress_tracks(track_writer_type& writer);

		//////////////////////////////////////////////////////////////////////////
		// Decompress a single track at the current sample time.
		// The track_writer_type allows complete control over how the track is written out.
		template<class track_writer_type>
		void decompress_track(uint32_t track_index, track_writer_type& writer);

		//////////////////////////////////////////////////////////////////////////
		// Releases the context instance if it contains an allocator reference.
		void release();

	private:
		decompression_context(const decompression_context& other) = delete;
		decompression_context& operator=(const decompression_context& other) = delete;

		// Internal context data
		acl_impl::persistent_decompression_context m_context;

		// The static settings used to strip out code at runtime
		decompression_settings_type m_settings;

		// The optional allocator instance used to allocate this instance
		IAllocator* m_allocator;

		static_assert(std::is_base_of<decompression_settings, decompression_settings_type>::value, "decompression_settings_type must derive from decompression_settings!");
	};

	//////////////////////////////////////////////////////////////////////////
	// Allocates and constructs an instance of the decompression context
	template<class decompression_settings_type>
	inline decompression_context<decompression_settings_type>* make_decompression_context(IAllocator& allocator)
	{
		return allocate_type<decompression_context<decompression_settings_type>>(allocator, &allocator);
	}

	//////////////////////////////////////////////////////////////////////////
	// Allocates and constructs an instance of the decompression context
	template<class decompression_settings_type>
	inline decompression_context<decompression_settings_type>* make_decompression_context(IAllocator& allocator, const decompression_settings_type& settings)
	{
		return allocate_type<decompression_context<decompression_settings_type>>(allocator, settings, &allocator);
	}

	//////////////////////////////////////////////////////////////////////////
	// decompression_context implementation

	template<class decompression_settings_type>
	inline decompression_context<decompression_settings_type>::decompression_context(IAllocator* allocator)
		: m_context()
		, m_settings()
		, m_allocator(allocator)
	{
		m_context.tracks = nullptr;		// Only member used to detect if we are initialized
	}

	template<class decompression_settings_type>
	inline decompression_context<decompression_settings_type>::decompression_context(const decompression_settings_type& settings, IAllocator* allocator)
		: m_context()
		, m_settings(settings)
		, m_allocator(allocator)
	{
		m_context.tracks = nullptr;		// Only member used to detect if we are initialized
	}

	template<class decompression_settings_type>
	inline decompression_context<decompression_settings_type>::~decompression_context()
	{
		release();
	}

	template<class decompression_settings_type>
	inline void decompression_context<decompression_settings_type>::initialize(const compressed_tracks& tracks)
	{
		ACL_ASSERT(tracks.is_valid(false).empty(), "Compressed tracks are not valid");
		ACL_ASSERT(tracks.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(tracks.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));

		m_context.tracks = &tracks;
		m_context.tracks_hash = tracks.get_hash();
		m_context.duration = tracks.get_duration();
		m_context.sample_time = -1.0F;
		m_context.interpolation_alpha = 0.0;
	}

	template<class decompression_settings_type>
	inline bool decompression_context<decompression_settings_type>::is_dirty(const compressed_tracks& tracks)
	{
		if (m_context.tracks != &tracks)
			return true;

		if (m_context.tracks_hash != tracks.get_hash())
			return true;

		return false;
	}

	template<class decompression_settings_type>
	inline void decompression_context<decompression_settings_type>::seek(float sample_time, SampleRoundingPolicy rounding_policy)
	{
		ACL_ASSERT(m_context.is_initialized(), "Context is not initialized");
		ACL_ASSERT(rtm::scalar_is_finite(sample_time), "Invalid sample time");

		// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
		if (m_settings.clamp_sample_time())
			sample_time = rtm::scalar_clamp(sample_time, 0.0F, m_context.duration);

		if (m_context.sample_time == sample_time)
			return;

		m_context.sample_time = sample_time;

		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*m_context.tracks);

		uint32_t key_frame0;
		uint32_t key_frame1;
		find_linear_interpolation_samples_with_sample_rate(header.num_samples, header.sample_rate, sample_time, rounding_policy, key_frame0, key_frame1, m_context.interpolation_alpha);

		m_context.key_frame_bit_offsets[0] = key_frame0 * header.num_bits_per_frame;
		m_context.key_frame_bit_offsets[1] = key_frame1 * header.num_bits_per_frame;
	}

	template<class decompression_settings_type>
	template<class track_writer_type>
	inline void decompression_context<decompression_settings_type>::decompress_tracks(track_writer_type& writer)
	{
		static_assert(std::is_base_of<track_writer, track_writer_type>::value, "track_writer_type must derive from track_writer");
		ACL_ASSERT(m_context.is_initialized(), "Context is not initialized");

		// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
		// Disable floating point exceptions to avoid issues.
		fp_environment fp_env;
		if (m_settings.disable_fp_exeptions())
			disable_fp_exceptions(fp_env);

		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*m_context.tracks);

		const acl_impl::track_metadata* per_track_metadata = header.get_track_metadata();
		const float* constant_values = header.get_track_constant_values();
		const float* range_values = header.get_track_range_values();
		const uint8_t* animated_values = header.get_track_animated_values();

		uint32_t track_bit_offset0 = m_context.key_frame_bit_offsets[0];
		uint32_t track_bit_offset1 = m_context.key_frame_bit_offsets[1];

		for (uint32_t track_index = 0; track_index < header.num_tracks; ++track_index)
		{
			const acl_impl::track_metadata& metadata = per_track_metadata[track_index];
			const uint8_t bit_rate = metadata.bit_rate;
			const uint8_t num_bits_per_component = get_num_bits_at_bit_rate(bit_rate);

			if (header.track_type == track_type8::float1f && m_settings.is_track_type_supported(track_type8::float1f))
			{
				rtm::scalarf value;
				if (is_constant_bit_rate(bit_rate))
				{
					value = rtm::scalar_load(constant_values);
					constant_values += 1;
				}
				else
				{
					rtm::scalarf value0;
					rtm::scalarf value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = acl_impl::unpack_scalarf_96_unsafe(animated_values, track_bit_offset0);
						value1 = acl_impl::unpack_scalarf_96_unsafe(animated_values, track_bit_offset1);
					}
					else
					{
						value0 = acl_impl::unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0);
						value1 = acl_impl::unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1);

						const rtm::scalarf range_min = rtm::scalar_load(range_values);
						const rtm::scalarf range_extent = rtm::scalar_load(range_values + 1);
						value0 = rtm::scalar_mul_add(value0, range_extent, range_min);
						value1 = rtm::scalar_mul_add(value1, range_extent, range_min);
						range_values += 2;
					}

					value = rtm::scalar_lerp(value0, value1, m_context.interpolation_alpha);

					const uint32_t num_sample_bits = num_bits_per_component;
					track_bit_offset0 += num_sample_bits;
					track_bit_offset1 += num_sample_bits;
				}

				writer.write_float1(track_index, value);
			}
			else if (header.track_type == track_type8::float2f && m_settings.is_track_type_supported(track_type8::float2f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
				{
					value = rtm::vector_load(constant_values);
					constant_values += 2;
				}
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = vector_acl2rtm(unpack_vector2_64_unsafe(animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector2_64_unsafe(animated_values, track_bit_offset1));
					}
					else
					{
						value0 = vector_acl2rtm(unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1));

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + 2);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
						range_values += 4;
					}

					value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);

					const uint32_t num_sample_bits = num_bits_per_component * 2;
					track_bit_offset0 += num_sample_bits;
					track_bit_offset1 += num_sample_bits;
				}

				writer.write_float2(track_index, value);
			}
			else if (header.track_type == track_type8::float3f && m_settings.is_track_type_supported(track_type8::float3f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
				{
					value = rtm::vector_load(constant_values);
					constant_values += 3;
				}
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = vector_acl2rtm(unpack_vector3_96_unsafe(animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector3_96_unsafe(animated_values, track_bit_offset1));
					}
					else
					{
						value0 = vector_acl2rtm(unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1));

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + 3);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
						range_values += 6;
					}

					value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);

					const uint32_t num_sample_bits = num_bits_per_component * 3;
					track_bit_offset0 += num_sample_bits;
					track_bit_offset1 += num_sample_bits;
				}

				writer.write_float3(track_index, value);
			}
			else if (header.track_type == track_type8::float4f && m_settings.is_track_type_supported(track_type8::float4f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
				{
					value = rtm::vector_load(constant_values);
					constant_values += 4;
				}
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, track_bit_offset1));
					}
					else
					{
						value0 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1));

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + 4);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
						range_values += 8;
					}

					value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);

					const uint32_t num_sample_bits = num_bits_per_component * 4;
					track_bit_offset0 += num_sample_bits;
					track_bit_offset1 += num_sample_bits;
				}

				writer.write_float4(track_index, value);
			}
			else if (header.track_type == track_type8::vector4f && m_settings.is_track_type_supported(track_type8::vector4f))
			{
				rtm::vector4f value;
				if (is_constant_bit_rate(bit_rate))
				{
					value = rtm::vector_load(constant_values);
					constant_values += 4;
				}
				else
				{
					rtm::vector4f value0;
					rtm::vector4f value1;
					if (is_raw_bit_rate(bit_rate))
					{
						value0 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, track_bit_offset1));
					}
					else
					{
						value0 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset0));
						value1 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, track_bit_offset1));

						const rtm::vector4f range_min = rtm::vector_load(range_values);
						const rtm::vector4f range_extent = rtm::vector_load(range_values + 4);
						value0 = rtm::vector_mul_add(value0, range_extent, range_min);
						value1 = rtm::vector_mul_add(value1, range_extent, range_min);
						range_values += 8;
					}

					value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);

					const uint32_t num_sample_bits = num_bits_per_component * 4;
					track_bit_offset0 += num_sample_bits;
					track_bit_offset1 += num_sample_bits;
				}

				writer.write_vector4(track_index, value);
			}
		}

		if (m_settings.disable_fp_exeptions())
			restore_fp_exceptions(fp_env);
	}

	template<class decompression_settings_type>
	template<class track_writer_type>
	inline void decompression_context<decompression_settings_type>::decompress_track(uint32_t track_index, track_writer_type& writer)
	{
		static_assert(std::is_base_of<track_writer, track_writer_type>::value, "track_writer_type must derive from track_writer");
		ACL_ASSERT(m_context.is_initialized(), "Context is not initialized");
		ACL_ASSERT(track_index < m_context.tracks->get_num_tracks(), "Invalid track index");

		// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
		// Disable floating point exceptions to avoid issues.
		fp_environment fp_env;
		if (m_settings.disable_fp_exeptions())
			disable_fp_exceptions(fp_env);

		const acl_impl::tracks_header& header = acl_impl::get_tracks_header(*m_context.tracks);

		const float* constant_values = header.get_track_constant_values();
		const float* range_values = header.get_track_range_values();

		const uint32_t num_element_components = get_track_num_sample_elements(header.track_type);
		uint32_t track_bit_offset = 0;

		const acl_impl::track_metadata* per_track_metadata = header.get_track_metadata();
		for (uint32_t scan_track_index = 0; scan_track_index < track_index; ++scan_track_index)
		{
			const acl_impl::track_metadata& metadata = per_track_metadata[scan_track_index];
			const uint8_t bit_rate = metadata.bit_rate;
			const uint32_t num_bits_per_component = get_num_bits_at_bit_rate(bit_rate);
			track_bit_offset += num_bits_per_component * num_element_components;

			if (is_constant_bit_rate(bit_rate))
				constant_values += num_element_components;
			else if (!is_raw_bit_rate(bit_rate))
				range_values += num_element_components * 2;
		}

		const acl_impl::track_metadata& metadata = per_track_metadata[track_index];
		const uint8_t bit_rate = metadata.bit_rate;
		const uint8_t num_bits_per_component = get_num_bits_at_bit_rate(bit_rate);

		const uint8_t* animated_values = header.get_track_animated_values();

		if (header.track_type == track_type8::float1f && m_settings.is_track_type_supported(track_type8::float1f))
		{
			rtm::scalarf value;
			if (is_constant_bit_rate(bit_rate))
				value = rtm::scalar_load(constant_values);
			else
			{
				rtm::scalarf value0;
				rtm::scalarf value1;
				if (is_raw_bit_rate(bit_rate))
				{
					value0 = acl_impl::unpack_scalarf_96_unsafe(animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset);
					value1 = acl_impl::unpack_scalarf_96_unsafe(animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset);
				}
				else
				{
					value0 = acl_impl::unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset);
					value1 = acl_impl::unpack_scalarf_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset);

					const rtm::scalarf range_min = rtm::scalar_load(range_values);
					const rtm::scalarf range_extent = rtm::scalar_load(range_values + num_element_components);
					value0 = rtm::scalar_mul_add(value0, range_extent, range_min);
					value1 = rtm::scalar_mul_add(value1, range_extent, range_min);
				}

				value = rtm::scalar_lerp(value0, value1, m_context.interpolation_alpha);
			}

			writer.write_float1(track_index, value);
		}
		else if (header.track_type == track_type8::float2f && m_settings.is_track_type_supported(track_type8::float2f))
		{
			rtm::vector4f value;
			if (is_constant_bit_rate(bit_rate))
				value = rtm::vector_load(constant_values);
			else
			{
				rtm::vector4f value0;
				rtm::vector4f value1;
				if (is_raw_bit_rate(bit_rate))
				{
					value0 = vector_acl2rtm(unpack_vector2_64_unsafe(animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector2_64_unsafe(animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));
				}
				else
				{
					value0 = vector_acl2rtm(unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector2_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));

					const rtm::vector4f range_min = rtm::vector_load(range_values);
					const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
					value0 = rtm::vector_mul_add(value0, range_extent, range_min);
					value1 = rtm::vector_mul_add(value1, range_extent, range_min);
				}

				value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);
			}

			writer.write_float2(track_index, value);
		}
		else if (header.track_type == track_type8::float3f && m_settings.is_track_type_supported(track_type8::float3f))
		{
			rtm::vector4f value;
			if (is_constant_bit_rate(bit_rate))
				value = rtm::vector_load(constant_values);
			else
			{
				rtm::vector4f value0;
				rtm::vector4f value1;
				if (is_raw_bit_rate(bit_rate))
				{
					value0 = vector_acl2rtm(unpack_vector3_96_unsafe(animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector3_96_unsafe(animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));
				}
				else
				{
					value0 = vector_acl2rtm(unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector3_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));

					const rtm::vector4f range_min = rtm::vector_load(range_values);
					const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
					value0 = rtm::vector_mul_add(value0, range_extent, range_min);
					value1 = rtm::vector_mul_add(value1, range_extent, range_min);
				}

				value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);
			}

			writer.write_float3(track_index, value);
		}
		else if (header.track_type == track_type8::float4f && m_settings.is_track_type_supported(track_type8::float4f))
		{
			rtm::vector4f value;
			if (is_constant_bit_rate(bit_rate))
				value = rtm::vector_load(constant_values);
			else
			{
				rtm::vector4f value0;
				rtm::vector4f value1;
				if (is_raw_bit_rate(bit_rate))
				{
					value0 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));
				}
				else
				{
					value0 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));

					const rtm::vector4f range_min = rtm::vector_load(range_values);
					const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
					value0 = rtm::vector_mul_add(value0, range_extent, range_min);
					value1 = rtm::vector_mul_add(value1, range_extent, range_min);
				}

				value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);
			}

			writer.write_float4(track_index, value);
		}
		else if (header.track_type == track_type8::vector4f && m_settings.is_track_type_supported(track_type8::vector4f))
		{
			rtm::vector4f value;
			if (is_constant_bit_rate(bit_rate))
				value = rtm::vector_load(constant_values);
			else
			{
				rtm::vector4f value0;
				rtm::vector4f value1;
				if (is_raw_bit_rate(bit_rate))
				{
					value0 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector4_128_unsafe(animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));
				}
				else
				{
					value0 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[0] + track_bit_offset));
					value1 = vector_acl2rtm(unpack_vector4_uXX_unsafe(num_bits_per_component, animated_values, m_context.key_frame_bit_offsets[1] + track_bit_offset));

					const rtm::vector4f range_min = rtm::vector_load(range_values);
					const rtm::vector4f range_extent = rtm::vector_load(range_values + num_element_components);
					value0 = rtm::vector_mul_add(value0, range_extent, range_min);
					value1 = rtm::vector_mul_add(value1, range_extent, range_min);
				}

				value = rtm::vector_lerp(value0, value1, m_context.interpolation_alpha);
			}

			writer.write_vector4(track_index, value);
		}

		if (m_settings.disable_fp_exeptions())
			restore_fp_exceptions(fp_env);
	}

	template<class decompression_settings_type>
	inline void decompression_context<decompression_settings_type>::release()
	{
		IAllocator* allocator = m_allocator;
		if (allocator != nullptr)
		{
			m_allocator = nullptr;
			deallocate_type<decompression_context>(*allocator, this);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
