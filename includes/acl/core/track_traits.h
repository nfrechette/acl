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

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/track_desc.h"
#include "acl/core/track_extended_sample.h"
#include "acl/core/track_types.h"

#include <rtm/types.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// Type tracks for tracks.
	// Each trait contains:
	//    - The category of the track
	//    - The type of each sample in the track
	//    - The type of the track description
	//////////////////////////////////////////////////////////////////////////
	template<track_type8 track_type, track_interpolator_t track_interpolator = track_interpolator_t::linear>
	struct track_traits {};

	//////////////////////////////////////////////////////////////////////////
	// Specializations for each track type.

	template<track_interpolator_t track_interpolator>
	struct track_traits<track_type8::float1f, track_interpolator>
	{
		static constexpr track_category8 category = track_category8::scalarf;

		using sample_type = typename std::conditional<
			track_interpolator == track_interpolator_t::linear,
			float,
			track_extended_sample_t<float>>::type;

		using desc_type = track_desc_scalarf;

		static rtm::vector4f RTM_SIMD_CALL load_as_vector(const sample_type* ptr) { return rtm::vector_set(*ptr, 0.0F, 0.0F, 0.0F); }
	};

	template<track_interpolator_t track_interpolator>
	struct track_traits<track_type8::float2f, track_interpolator>
	{
		static constexpr track_category8 category = track_category8::scalarf;

		using sample_type = typename std::conditional<
			track_interpolator == track_interpolator_t::linear,
			rtm::float2f,
			track_extended_sample_t<rtm::float2f>>::type;

		using desc_type = track_desc_scalarf;

		static rtm::vector4f RTM_SIMD_CALL load_as_vector(const sample_type* ptr) { return rtm::vector_load2(ptr); }
	};

	template<track_interpolator_t track_interpolator>
	struct track_traits<track_type8::float3f, track_interpolator>
	{
		static constexpr track_category8 category = track_category8::scalarf;

		using sample_type = typename std::conditional<
			track_interpolator == track_interpolator_t::linear,
			rtm::float3f,
			track_extended_sample_t<rtm::float3f>>::type;

		using desc_type = track_desc_scalarf;

		static rtm::vector4f RTM_SIMD_CALL load_as_vector(const sample_type* ptr) { return rtm::vector_load3(ptr); }
	};

	template<track_interpolator_t track_interpolator>
	struct track_traits<track_type8::float4f, track_interpolator>
	{
		static constexpr track_category8 category = track_category8::scalarf;

		using sample_type = typename std::conditional<
			track_interpolator == track_interpolator_t::linear,
			rtm::float4f,
			track_extended_sample_t<rtm::float4f>>::type;

		using desc_type = track_desc_scalarf;

		static rtm::vector4f RTM_SIMD_CALL load_as_vector(const sample_type* ptr) { return rtm::vector_load(ptr); }
	};

	template<track_interpolator_t track_interpolator>
	struct track_traits<track_type8::vector4f, track_interpolator>
	{
		static constexpr track_category8 category = track_category8::scalarf;

		// Suppress GCC warning about may-alias warning being ignored due to type being used as template argument
	#if defined(RTM_COMPILER_GCC)
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wignored-attributes"
	#endif

		using sample_type = typename std::conditional<
			track_interpolator == track_interpolator_t::linear,
			rtm::vector4f,
			track_extended_sample_t<rtm::vector4f>>::type;

	#if defined(RTM_COMPILER_GCC)
		#pragma GCC diagnostic pop
	#endif

		using desc_type = track_desc_scalarf;

		static rtm::vector4f RTM_SIMD_CALL load_as_vector(const sample_type* ptr) { return *ptr; }
	};

	template<track_interpolator_t track_interpolator>
	struct track_traits<track_type8::qvvf, track_interpolator>
	{
		static_assert(track_interpolator == track_interpolator_t::linear, "Extended interpolator is not supported for this type");

		static constexpr track_category8 category = track_category8::transformf;

		using sample_type = rtm::qvvf;
		using desc_type = track_desc_transformf;
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
