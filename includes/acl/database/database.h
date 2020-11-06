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

#include "acl/core/compressed_database.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/error.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/database/idatabase_streamer.h"
#include "acl/database/impl/database_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	struct database_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// Which version we should optimize for.
		// If 'any' is specified, the database context will support every single version
		// with full backwards compatibility.
		// Using a specific version allows the compiler to statically strip code for all other
		// versions. This allows the creation of context objects specialized for specific
		// versions which yields optimal performance.
		// Must be static constexpr!
		static constexpr compressed_tracks_version16 version_supported() { return compressed_tracks_version16::any; }
	};

	//////////////////////////////////////////////////////////////////////////
	// These are debug settings, everything is enabled and nothing is stripped.
	// It will have the worst performance but allows every feature.
	//////////////////////////////////////////////////////////////////////////
	struct debug_database_settings : public database_settings
	{
	};

	//////////////////////////////////////////////////////////////////////////
	// These are the default settings. Only the generally optimal settings
	// are enabled and will offer the overall best performance.
	// Supports every version.
	//////////////////////////////////////////////////////////////////////////
	struct default_database_settings : public database_settings
	{
	};

	//////////////////////////////////////////////////////////////////////////
	// Encapsulates the possible streaming request results.
	//////////////////////////////////////////////////////////////////////////
	enum class database_stream_request_result
	{
		//////////////////////////////////////////////////////////////////////////
		// Streaming is done for the requested tier
		done,

		//////////////////////////////////////////////////////////////////////////
		// The streaming request has been dispatched
		dispatched,

		//////////////////////////////////////////////////////////////////////////
		// The streaming request has been ignored because streaming is already in progress
		streaming,

		//////////////////////////////////////////////////////////////////////////
		// The database context isn't initialized
		not_initialized,
	};

	template<class database_settings_type>
	class database_context
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// An alias to the database settings type.
		using settings_type = database_settings_type;

		database_context();
		~database_context();

		const compressed_database* get_compressed_database() const;

		bool initialize(iallocator& allocator, const compressed_database& database);

		bool initialize(iallocator& allocator, const compressed_database& database, idatabase_streamer& streamer);

		bool is_initialized() const;

		void reset();

		bool contains(const compressed_tracks& tracks) const;

		bool is_streamed_in() const;

		bool is_streaming() const;

		database_stream_request_result stream_in(uint32_t num_chunks_to_stream = ~0U);

		database_stream_request_result stream_out(uint32_t num_chunks_to_stream = ~0U);

	private:
		database_context(const database_context& other) = delete;
		database_context& operator=(const database_context& other) = delete;

		// Internal context data
		acl_impl::database_context_v0 m_context;

		static_assert(std::is_base_of<database_settings, settings_type>::value, "database_settings_type must derive from database_settings!");
	};
}

#include "acl/database/impl/database.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
