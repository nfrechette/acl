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

#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/error.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/track_formats.h"
#include "acl/core/track_traits.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/decompression_settings.h"
#include "acl/decompression/database/database.h"
#include "acl/decompression/impl/decompression_context_selector.h"
#include "acl/decompression/impl/decompression_version_selector.h"
#include "acl/decompression/impl/scalar_track_decompression.h"
#include "acl/decompression/impl/transform_track_decompression.h"
#include "acl/decompression/impl/universal_track_decompression.h"
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
		// An alias to the decompression settings type.
		using settings_type = decompression_settings_type;

		//////////////////////////////////////////////////////////////////////////
		// An alias to the database settings type.
		using db_settings_type = typename decompression_settings_type::database_settings_type;

		//////////////////////////////////////////////////////////////////////////
		// Constructs a context instance.
		decompression_context();

		//////////////////////////////////////////////////////////////////////////
		// Returns the compressed tracks bound to this context instance.
		const compressed_tracks* get_compressed_tracks() const { return m_context.get_compressed_tracks(); }

		//////////////////////////////////////////////////////////////////////////
		// Initializes the context instance to a particular compressed tracks instance.
		// Returns whether initialization was successful or not.
		bool initialize(const compressed_tracks& tracks);

		//////////////////////////////////////////////////////////////////////////
		// Initializes the context instance to a particular compressed tracks instance and its database instance.
		// Returns whether initialization was successful or not.
		bool initialize(const compressed_tracks& tracks, const database_context<db_settings_type>& database);

		//////////////////////////////////////////////////////////////////////////
		// Returns true if this context instance is bound to a compressed tracks instance, false otherwise.
		bool is_initialized() const { return m_context.is_initialized(); }

		//////////////////////////////////////////////////////////////////////////
		// Returns true if this context instance is bound to the specified compressed tracks instance, false otherwise.
		bool is_dirty(const compressed_tracks& tracks) const;

		//////////////////////////////////////////////////////////////////////////
		// Seeks within the compressed tracks to a particular point in time with the
		// desired rounding policy.
		void seek(float sample_time, sample_rounding_policy rounding_policy);

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

	private:
		decompression_context(const decompression_context& other) = delete;
		decompression_context& operator=(const decompression_context& other) = delete;

		// Whether the decompression context should support scalar tracks
		static constexpr bool k_supports_scalar_tracks = settings_type::is_track_type_supported(track_type8::float1f)
			|| settings_type::is_track_type_supported(track_type8::float2f)
			|| settings_type::is_track_type_supported(track_type8::float3f)
			|| settings_type::is_track_type_supported(track_type8::float4f)
			|| settings_type::is_track_type_supported(track_type8::vector4f);

		// Whether the decompression context should support transform tracks
		static constexpr bool k_supports_transform_tracks = settings_type::is_track_type_supported(track_type8::qvvf);

		// The type of our persistent context based on what track types we support
		using context_type = typename acl_impl::persistent_decompression_context_selector<k_supports_scalar_tracks, k_supports_transform_tracks>::type;

		// The type of our algorithm implementation based on the supported version
		using version_impl_type = acl_impl::decompression_version_selector<settings_type::version_supported()>;

		// Internal context data
		context_type m_context;

		static_assert(std::is_base_of<decompression_settings, settings_type>::value, "decompression_settings_type must derive from decompression_settings!");
		static_assert(std::is_base_of<database_settings, db_settings_type>::value, "database_settings_type must derive from database_settings!");
		static_assert(settings_type::version_supported() != compressed_tracks_version16::none, "decompression_settings_type must support at least one version");
		static_assert(db_settings_type::version_supported() == compressed_tracks_version16::none || db_settings_type::version_supported() == settings_type::version_supported(), "database_settings_type's supported version must be none or match the supported version from decompression_settings_type");
	};

	//////////////////////////////////////////////////////////////////////////
	// Allocates and constructs an instance of the decompression context
	template<class decompression_settings_type>
	inline decompression_context<decompression_settings_type>* make_decompression_context(iallocator& allocator)
	{
		return allocate_type<decompression_context<decompression_settings_type>>(allocator);
	}
}

#include "acl/decompression/impl/decompress.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
