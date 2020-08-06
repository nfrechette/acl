#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/interpolation_utils.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/decompression/impl/scalar_track_decompression.h"
#include "acl/decompression/impl/transform_track_decompression.h"
#include "acl/decompression/impl/universal_track_decompression.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Selector template for decompression implementation details.
		// Specialized for each algorithm version.
		//////////////////////////////////////////////////////////////////////////
		template<compressed_tracks_version16 version>
		struct decompression_version_selector {};

		//////////////////////////////////////////////////////////////////////////
		// Optimized for ACL 2.0.0
		//////////////////////////////////////////////////////////////////////////
		template<>
		struct decompression_version_selector<compressed_tracks_version16::v02_00_00>
		{
			static constexpr bool is_version_supported(compressed_tracks_version16 version) { return version == compressed_tracks_version16::v02_00_00; }

			template<class decompression_settings_type, class context_type>
			ACL_FORCE_INLINE static bool initialize(context_type& context, const compressed_tracks& tracks) { return acl_impl::initialize_v0<decompression_settings_type>(context, tracks); }

			template<class context_type>
			ACL_FORCE_INLINE static bool is_dirty(const context_type& context, const compressed_tracks& tracks) { return acl_impl::is_dirty_v0(context, tracks); }

			template<class decompression_settings_type, class context_type>
			ACL_FORCE_INLINE static void seek(context_type& context, float sample_time, sample_rounding_policy rounding_policy) { acl_impl::seek_v0<decompression_settings_type>(context, sample_time, rounding_policy); }

			template<class decompression_settings_type, class track_writer_type, class context_type>
			ACL_FORCE_INLINE static void decompress_tracks(context_type& context, track_writer_type& writer) { acl_impl::decompress_tracks_v0<decompression_settings_type>(context, writer); }

			template<class decompression_settings_type, class track_writer_type, class context_type>
			ACL_FORCE_INLINE static void decompress_track(context_type& context, uint32_t track_index, track_writer_type& writer) { acl_impl::decompress_track_v0<decompression_settings_type>(context, track_index, writer); }
		};

		//////////////////////////////////////////////////////////////////////////
		// Not optimized for any particular version.
		//////////////////////////////////////////////////////////////////////////
		template<>
		struct decompression_version_selector<compressed_tracks_version16::any>
		{
			static constexpr bool is_version_supported(compressed_tracks_version16 version)
			{
				return version >= compressed_tracks_version16::first && version <= compressed_tracks_version16::latest;
			}

			template<class decompression_settings_type, class context_type>
			static bool initialize(context_type& context, const compressed_tracks& tracks)
			{
				// TODO: Use an array of lambdas and use the version to lookup? This could be a simple indirect function call.
				const compressed_tracks_version16 version = tracks.get_version();
				switch (version)
				{
				case compressed_tracks_version16::v02_00_00:
					return decompression_version_selector<compressed_tracks_version16::v02_00_00>::initialize<decompression_settings_type>(context, tracks);
				default:
					ACL_ASSERT(false, "Unsupported version");
					return false;
				}
			}

			template<class context_type>
			static bool is_dirty(const context_type& context, const compressed_tracks& tracks)
			{
				const compressed_tracks_version16 version = tracks.get_version();
				switch (version)
				{
				case compressed_tracks_version16::v02_00_00:
					return decompression_version_selector<compressed_tracks_version16::v02_00_00>::is_dirty(context, tracks);
				default:
					ACL_ASSERT(false, "Unsupported version");
					return false;
				}
			}

			template<class decompression_settings_type, class context_type>
			static void seek(context_type& context, float sample_time, sample_rounding_policy rounding_policy)
			{
				const compressed_tracks_version16 version = context.get_version();
				switch (version)
				{
				case compressed_tracks_version16::v02_00_00:
					decompression_version_selector<compressed_tracks_version16::v02_00_00>::seek<decompression_settings_type>(context, sample_time, rounding_policy);
					break;
				default:
					ACL_ASSERT(false, "Unsupported version");
					break;
				}
			}

			template<class decompression_settings_type, class track_writer_type, class context_type>
			static void decompress_tracks(context_type& context, track_writer_type& writer)
			{
				const compressed_tracks_version16 version = context.get_version();
				switch (version)
				{
				case compressed_tracks_version16::v02_00_00:
					decompression_version_selector<compressed_tracks_version16::v02_00_00>::decompress_tracks<decompression_settings_type>(context, writer);
					break;
				default:
					ACL_ASSERT(false, "Unsupported version");
					break;
				}
			}

			template<class decompression_settings_type, class track_writer_type, class context_type>
			static void decompress_track(context_type& context, uint32_t track_index, track_writer_type& writer)
			{
				const compressed_tracks_version16 version = context.get_version();
				switch (version)
				{
				case compressed_tracks_version16::v02_00_00:
					decompression_version_selector<compressed_tracks_version16::v02_00_00>::decompress_track<decompression_settings_type>(context, track_index, writer);
					break;
				default:
					ACL_ASSERT(false, "Unsupported version");
					break;
				}
			}
		};
	}
}

ACL_IMPL_FILE_PRAGMA_POP
