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
#include "acl/core/track_types.h"
#include "acl/core/track_writer.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		struct debug_track_writer final : public track_writer
		{
			debug_track_writer(iallocator& allocator_, track_type8 type_, uint32_t num_tracks_)
				: allocator(allocator_)
				, tracks_typed{ nullptr }
				, buffer_size(0)
				, num_tracks(num_tracks_)
				, type(type_)
			{
				// Large enough to accommodate the largest type
				buffer_size = sizeof(rtm::qvvf) * num_tracks_;
				tracks_typed.any = allocator_.allocate(buffer_size, alignof(rtm::qvvf));
			}

			~debug_track_writer()
			{
				allocator.deallocate(tracks_typed.any, buffer_size);
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float1(uint32_t track_index, rtm::scalarf_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float1f, "Unexpected track type access");
				rtm::scalar_store(value, &tracks_typed.float1f[track_index]);
			}

			float RTM_SIMD_CALL read_float1(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float1f, "Unexpected track type access");
				return tracks_typed.float1f[track_index];
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float2(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float2f, "Unexpected track type access");
				rtm::vector_store2(value, &tracks_typed.float2f[track_index]);
			}

			rtm::vector4f RTM_SIMD_CALL read_float2(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float2f, "Unexpected track type access");
				return rtm::vector_load2(&tracks_typed.float2f[track_index]);
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float3(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float3f, "Unexpected track type access");
				rtm::vector_store3(value, &tracks_typed.float3f[track_index]);
			}

			rtm::vector4f RTM_SIMD_CALL read_float3(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float3f, "Unexpected track type access");
				return rtm::vector_load3(&tracks_typed.float3f[track_index]);
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_float4(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::float4f, "Unexpected track type access");
				rtm::vector_store(value, &tracks_typed.float4f[track_index]);
			}

			rtm::vector4f RTM_SIMD_CALL read_float4(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::float4f, "Unexpected track type access");
				return rtm::vector_load(&tracks_typed.float4f[track_index]);
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a value for a specified track index.
			void RTM_SIMD_CALL write_vector4(uint32_t track_index, rtm::vector4f_arg0 value)
			{
				ACL_ASSERT(type == track_type8::vector4f, "Unexpected track type access");
				tracks_typed.vector4f[track_index] = value;
			}

			rtm::vector4f RTM_SIMD_CALL read_vector4(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::vector4f, "Unexpected track type access");
				return tracks_typed.vector4f[track_index];
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a quaternion rotation value for a specified bone index.
			void RTM_SIMD_CALL write_rotation(uint32_t track_index, rtm::quatf_arg0 rotation)
			{
				ACL_ASSERT(type == track_type8::qvvf, "Unexpected track type access");
				tracks_typed.qvvf[track_index].rotation = rotation;
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a translation value for a specified bone index.
			void RTM_SIMD_CALL write_translation(uint32_t track_index, rtm::vector4f_arg0 translation)
			{
				ACL_ASSERT(type == track_type8::qvvf, "Unexpected track type access");
				tracks_typed.qvvf[track_index].translation = translation;
			}

			//////////////////////////////////////////////////////////////////////////
			// Called by the decoder to write out a scale value for a specified bone index.
			void RTM_SIMD_CALL write_scale(uint32_t track_index, rtm::vector4f_arg0 scale)
			{
				ACL_ASSERT(type == track_type8::qvvf, "Unexpected track type access");
				tracks_typed.qvvf[track_index].scale = scale;
			}

			const rtm::qvvf& RTM_SIMD_CALL read_qvv(uint32_t track_index) const
			{
				ACL_ASSERT(type == track_type8::qvvf, "Unexpected track type access");
				return tracks_typed.qvvf[track_index];
			}

			union ptr_union
			{
				void*			any;
				float*			float1f;
				rtm::float2f*	float2f;
				rtm::float3f*	float3f;
				rtm::float4f*	float4f;
				rtm::vector4f*	vector4f;
				rtm::qvvf*		qvvf;
			};

			iallocator& allocator;

			ptr_union tracks_typed;
			size_t buffer_size;
			uint32_t num_tracks;

			track_type8 type;
		};
	}
}

ACL_IMPL_FILE_PRAGMA_POP
