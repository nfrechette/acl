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
		// Common decompression settings

		//////////////////////////////////////////////////////////////////////////
		// Whether or not to clamp the sample time when `seek(..)` is called. Defaults to true.
		// Must be static constexpr!
		static constexpr bool clamp_sample_time() { return true; }

		//////////////////////////////////////////////////////////////////////////
		// Whether or not the specified track type is supported. Defaults to true.
		// If a track type is statically known not to be supported, the compiler can strip
		// the associated code.
		// Must be static constexpr!
		static constexpr bool is_track_type_supported(track_type8 /*type*/) { return true; }

		//////////////////////////////////////////////////////////////////////////
		// Whether to explicitly disable floating point exceptions during decompression.
		// This has a cost, exceptions are usually disabled globally and do not need to be
		// explicitly disabled during decompression.
		// We assume that floating point exceptions are already disabled by the caller.
		// Must be static constexpr!
		static constexpr bool disable_fp_exeptions() { return false; }

		//////////////////////////////////////////////////////////////////////////
		// Which version we should optimize for.
		// If 'any' is specified, the decompression context will support every single version
		// with full backwards compatibility.
		// Using a specific version allows the compiler to statically strip code for all other
		// versions. This allows the creation of context objects specialized for specific
		// versions which yields optimal performance.
		// Must be static constexpr!
		static constexpr compressed_tracks_version16 version_supported() { return compressed_tracks_version16::any; }

		//////////////////////////////////////////////////////////////////////////
		// Transform decompression settings

		//////////////////////////////////////////////////////////////////////////
		// Whether the specified rotation/translation/scale format are supported or not.
		// Use this to strip code related to formats you do not need.
		// Must be static constexpr!
		static constexpr bool is_rotation_format_supported(rotation_format8 /*format*/) { return true; }
		static constexpr bool is_translation_format_supported(vector_format8 /*format*/) { return true; }
		static constexpr bool is_scale_format_supported(vector_format8 /*format*/) { return true; }

		//////////////////////////////////////////////////////////////////////////
		// Whether rotations should be normalized before being output or not. Some animation
		// runtimes will normalize in a separate step and do not need the explicit normalization.
		// Enabled by default for safety.
		// Must be static constexpr!
		static constexpr bool normalize_rotations() { return true; }
	};

	//////////////////////////////////////////////////////////////////////////
	// These are debug settings, everything is enabled and nothing is stripped.
	// It will have the worst performance but allows every feature.
	//////////////////////////////////////////////////////////////////////////
	struct debug_scalar_decompression_settings : public decompression_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// Only support scalar tracks
		static constexpr bool is_track_type_supported(track_type8 type) { return type != track_type8::qvvf; }
	};

	//////////////////////////////////////////////////////////////////////////
	// These are debug settings, everything is enabled and nothing is stripped.
	// It will have the worst performance but allows every feature.
	//////////////////////////////////////////////////////////////////////////
	struct debug_transform_decompression_settings : public decompression_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// Only support transform tracks
		static constexpr bool is_track_type_supported(track_type8 type) { return type == track_type8::qvvf; }
	};

	//////////////////////////////////////////////////////////////////////////
	// These are the default settings. Only the generally optimal settings
	// are enabled and will offer the overall best performance.
	//////////////////////////////////////////////////////////////////////////
	struct default_scalar_decompression_settings : public decompression_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// Only support scalar tracks
		static constexpr bool is_track_type_supported(track_type8 type) { return type != track_type8::qvvf; }
	};

	//////////////////////////////////////////////////////////////////////////
	// These are the default settings. Only the generally optimal settings
	// are enabled and will offer the overall best performance.
	//////////////////////////////////////////////////////////////////////////
	struct default_transform_decompression_settings : public decompression_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// Only support transform tracks
		static constexpr bool is_track_type_supported(track_type8 type) { return type == track_type8::qvvf; }

		//////////////////////////////////////////////////////////////////////////
		// By default, we only support the variable bit rates as they are generally optimal
		static constexpr bool is_rotation_format_supported(rotation_format8 format) { return format == rotation_format8::quatf_drop_w_variable; }
		static constexpr bool is_translation_format_supported(vector_format8 format) { return format == vector_format8::vector3f_variable; }
		static constexpr bool is_scale_format_supported(vector_format8 format) { return format == vector_format8::vector3f_variable; }
	};

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
		// Constructs a context instance.
		// The default constructor for the `decompression_settings_type` will be used.
		decompression_context();

		//////////////////////////////////////////////////////////////////////////
		// Returns the compressed tracks bound to this context instance.
		const compressed_tracks* get_compressed_tracks() const { return m_context.get_compressed_tracks(); }

		//////////////////////////////////////////////////////////////////////////
		// Initializes the context instance to a particular compressed tracks instance.
		// Returns whether initialization was successful or not.
		bool initialize(const compressed_tracks& tracks);

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
		using algorithm_version_type = acl_impl::decompression_version_selector<settings_type::version_supported()>;

		// Internal context data
		context_type m_context;

		static_assert(std::is_base_of<decompression_settings, settings_type>::value, "decompression_settings_type must derive from decompression_settings!");
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
