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

#include "acl/config.h"
#include "acl/version.h"
#include "acl/compression/track_array.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/iallocator.h"
#include "acl/core/track_types.h"
#include "acl/core/track_writer.h"

#include <rtm/vqvf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// The debug_track_writer supports every track type and adds runtime type safety.
		//////////////////////////////////////////////////////////////////////////
		struct debug_track_writer : public track_writer
		{
			debug_track_writer(iallocator& allocator_, track_type8 type_, uint32_t num_tracks_)
				: allocator(allocator_)
				// Large enough to accommodate the largest type
				, tracks_buffer(allocator_.allocate(sizeof(rtm::vqvf) * num_tracks_, alignof(rtm::vqvf)))
				, buffer_size(sizeof(rtm::vqvf) * num_tracks_)
				, num_tracks(num_tracks_)
				, type(type_)
			{
			}

			~debug_track_writer()
			{
				allocator.deallocate(tracks_buffer, buffer_size);
			}

			// Cannot copy or move
			debug_track_writer(const debug_track_writer&) = delete;
			debug_track_writer& operator=(const debug_track_writer&) = delete;

			//////////////////////////////////////////////////////////////////////////
			// For performance reasons, this writer skips all default sub-tracks.
			// It is the responsibility of the caller to pre-populate them by calling initialize_with_defaults().
#if defined(ACL_IMPL_DISABLE_BIND_POSE_STRIPPING)
			static constexpr default_sub_track_mode get_default_rotation_mode() { return default_sub_track_mode::constant; }
			static constexpr default_sub_track_mode get_default_translation_mode() { return default_sub_track_mode::constant; }
			static constexpr default_sub_track_mode get_default_scale_mode() { return default_sub_track_mode::constant; }
#else
			static constexpr default_sub_track_mode get_default_rotation_mode() { return default_sub_track_mode::skipped; }
			static constexpr default_sub_track_mode get_default_translation_mode() { return default_sub_track_mode::skipped; }
			static constexpr default_sub_track_mode get_default_scale_mode() { return default_sub_track_mode::skipped; }
#endif

			//////////////////////////////////////////////////////////////////////////
			// Initializes the internal buffer with the sub-track default values provided.
			// This assumes that the debug_track_writer was allocated with the number of tracks
			// from the compressed tracks instance used to populate it and that the provided
			// track_array remaps to it properly with matching output indices.
			// The track_array can contain vqvf, vqf, or vqsf tracks.
			// With vqsf, the default scale is uniform: we use the X component of the default value scale.
			void initialize_with_defaults(const track_array& tracks)
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				ACL_ASSERT(tracks.is_empty() || is_transform_track_type(tracks.get_track_type()), "Expected transform tracks");

				const bool is_vqs = !tracks.is_empty() && tracks.get_track_type() == track_type8::vqsf;

				for (const track& track_ : tracks)
				{
					const track_desc_transformf& desc = track_.get_description<track_desc_transformf>();
					if (desc.output_index == k_invalid_track_index)
						continue;	// Stripped, skip it

					rtm::vqvf default_value = desc.default_value;
					if (is_vqs)
						default_value.scale = rtm::vector_dup_x(default_value.scale);

					get_vqvf_tracks()[desc.output_index] = default_value;
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Sets the scale of each transform.
			// A VQ track has no scale, thus its sampling does not write it.
			void RTM_SIMD_CALL set_all_scales(rtm::vector4f_arg0 scale)
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");

				for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
					get_vqvf_tracks()[track_index].scale = scale;
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float1(uint32_t track_index, rtm::scalarf_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float1f, "Unexpected track type access");
				rtm::scalar_store(value, &get_scalar_tracks<float>()[track_index]);
			}

			float RTM_SIMD_CALL read_float1(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float1f, "Unexpected track type access");
				return get_scalar_tracks<float>()[track_index];
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float2(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float2f, "Unexpected track type access");
				rtm::vector_store2(value, &get_scalar_tracks<rtm::float2f>()[track_index]);
			}

			rtm::vector4f RTM_SIMD_CALL read_float2(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float2f, "Unexpected track type access");
				return rtm::vector_load2(&get_scalar_tracks<rtm::float2f>()[track_index]);
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float3(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float3f, "Unexpected track type access");
				rtm::vector_store3(value, &get_scalar_tracks<rtm::float3f>()[track_index]);
			}

			rtm::vector4f RTM_SIMD_CALL read_float3(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float3f, "Unexpected track type access");
				return rtm::vector_load3(&get_scalar_tracks<rtm::float3f>()[track_index]);
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float4(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float4f, "Unexpected track type access");
				rtm::vector_store(value, &get_scalar_tracks<rtm::float4f>()[track_index]);
			}

			rtm::vector4f RTM_SIMD_CALL read_float4(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float4f, "Unexpected track type access");
				return rtm::vector_load(&get_scalar_tracks<rtm::float4f>()[track_index]);
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_vector4(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::vector4f, "Unexpected track type access");
				get_scalar_tracks<rtm::vector4f>()[track_index] = value;
			}

			rtm::vector4f RTM_SIMD_CALL read_vector4(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::vector4f, "Unexpected track type access");
				return get_scalar_tracks<rtm::vector4f>()[track_index];
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a quaternion rotation value for a specified bone index.
			void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation)
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				get_vqvf_tracks()[track_index].rotation = rotation;
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a translation value for a specified bone index.
			void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation)
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				get_vqvf_tracks()[track_index].translation = translation;
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a scale value for a specified bone index.
			void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale)
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				get_vqvf_tracks()[track_index].scale = scale;
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a uniform scale value for a specified bone index.
			// The decoder calls it for vqsf tracks. We store the value in [xyz].
			void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::scalarf_arg0 scale)
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				get_vqvf_tracks()[track_index].scale = rtm::vector_set(scale);
			}

			const rtm::vqvf& RTM_SIMD_CALL read_vqv(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				return get_vqvf_tracks()[track_index];
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the vqvf track values. The writer must hold vqvf tracks.
			rtm::vqvf* get_vqvf_tracks()
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				return static_cast<rtm::vqvf*>(tracks_buffer);
			}

			const rtm::vqvf* get_vqvf_tracks() const
			{
				ACL_ASSERT(type == track_type8::vqvf, "Unexpected track type access");
				return static_cast<const rtm::vqvf*>(tracks_buffer);
			}

		private:
			// Returns the track values as the type of a scalar track
			template<typename value_type_t>
			value_type_t* get_scalar_tracks() { return static_cast<value_type_t*>(tracks_buffer); }

			template<typename value_type_t>
			const value_type_t* get_scalar_tracks() const { return static_cast<const value_type_t*>(tracks_buffer); }

		public:

			iallocator& allocator;

			// The track values, their type is the track type of the writer
			void* tracks_buffer;
			size_t buffer_size;
			uint32_t num_tracks;

			track_type8 type;

			// Must pad to a multiple of 16 bytes for derived types
			uint8_t padding[sizeof(void*) == 4 ? 15 : 3] = {0};
		};

		//////////////////////////////////////////////////////////////////////////
		// Same as debug_track_writer but default sub-tracks are constant instead of skipped.
		// Only supports vqvf tracks.
		//////////////////////////////////////////////////////////////////////////
		struct debug_track_writer_constant_defaults final : public debug_track_writer
		{
			debug_track_writer_constant_defaults(iallocator& allocator_, track_type8 type_, uint32_t num_tracks_)
				: debug_track_writer(allocator_, type_, num_tracks_)
				, default_rotation(rtm::quat_identity())
				, default_translation(rtm::vector_zero())
				, default_scale(rtm::vector_set(1.0F))
			{
				ACL_ASSERT(type_ == track_type8::vqvf, "Only vqvf tracks are supported");
			}

			// Cannot copy or move
			debug_track_writer_constant_defaults(const debug_track_writer_constant_defaults&) = delete;
			debug_track_writer_constant_defaults& operator=(const debug_track_writer_constant_defaults&) = delete;

			static constexpr default_sub_track_mode get_default_rotation_mode() { return default_sub_track_mode::constant; }
			static constexpr default_sub_track_mode get_default_translation_mode() { return default_sub_track_mode::constant; }
			static constexpr default_sub_track_mode get_default_scale_mode() { return default_sub_track_mode::constant; }

			rtm::quatf RTM_SIMD_CALL get_constant_default_rotation() const { return default_rotation; }
			rtm::vector4f RTM_SIMD_CALL get_constant_default_translation() const { return default_translation; }
			rtm::vector4f RTM_SIMD_CALL get_constant_default_scale() const { return default_scale; }

			rtm::quatf default_rotation;
			rtm::vector4f default_translation;
			rtm::vector4f default_scale;
		};

		//////////////////////////////////////////////////////////////////////////
		// Same as debug_track_writer but default sub-tracks are variable instead of skipped.
		// Only supports vqvf tracks.
		//////////////////////////////////////////////////////////////////////////
		struct debug_track_writer_variable_defaults final : public debug_track_writer
		{
			debug_track_writer_variable_defaults(iallocator& allocator_, track_type8 type_, uint32_t num_tracks_)
				: debug_track_writer(allocator_, type_, num_tracks_)
				, default_sub_tracks(nullptr)
			{
				ACL_ASSERT(type_ == track_type8::vqvf, "Only vqvf tracks are supported");
			}

			// Cannot copy or move
			debug_track_writer_variable_defaults(const debug_track_writer_variable_defaults&) = delete;
			debug_track_writer_variable_defaults& operator=(const debug_track_writer_variable_defaults&) = delete;

			static constexpr default_sub_track_mode get_default_rotation_mode() { return default_sub_track_mode::variable; }
			static constexpr default_sub_track_mode get_default_translation_mode() { return default_sub_track_mode::variable; }
			static constexpr default_sub_track_mode get_default_scale_mode() { return default_sub_track_mode::variable; }

			rtm::quatf RTM_SIMD_CALL get_variable_default_rotation(uint32_t track_index) const { return default_sub_tracks[track_index].rotation; }
			rtm::vector4f RTM_SIMD_CALL get_variable_default_translation(uint32_t track_index) const { return default_sub_tracks[track_index].translation; }
			rtm::vector4f RTM_SIMD_CALL get_variable_default_scale(uint32_t track_index) const { return default_sub_tracks[track_index].scale; }

			const rtm::vqvf* default_sub_tracks;
		};

		//////////////////////////////////////////////////////////////////////////
		// Same as debug_track_writer but the rounding policy is supported per track.
		//////////////////////////////////////////////////////////////////////////
		struct debug_track_writer_per_track_rounding final : public debug_track_writer
		{
			debug_track_writer_per_track_rounding(iallocator& allocator_, track_type8 type_, uint32_t num_tracks_)
				: debug_track_writer(allocator_, type_, num_tracks_)
				, rounding_policy(sample_rounding_policy::per_track)
			{
			}

			// Cannot copy or move
			debug_track_writer_per_track_rounding(const debug_track_writer_per_track_rounding&) = delete;
			debug_track_writer_per_track_rounding& operator=(const debug_track_writer_per_track_rounding&) = delete;

			sample_rounding_policy get_rounding_policy(sample_rounding_policy /*seek_policy*/, uint32_t /*track_index*/) const { return rounding_policy; }

			sample_rounding_policy rounding_policy;
		};
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
