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

// Included only once from decompress.h

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// decompression_context implementation

	template<class decompression_settings_type>
	inline decompression_context<decompression_settings_type>::decompression_context()
		: m_context()
	{
		m_context.reset();
	}

	template<class decompression_settings_type>
	inline bool decompression_context<decompression_settings_type>::initialize(const compressed_tracks& tracks)
	{
		if (!algorithm_version_type::is_version_supported(tracks.get_version()))
			return false;

		return algorithm_version_type::template initialize<decompression_settings_type>(m_context, tracks);
	}

	template<class decompression_settings_type>
	inline bool decompression_context<decompression_settings_type>::is_dirty(const compressed_tracks& tracks) const
	{
		return algorithm_version_type::template is_dirty(m_context, tracks);
	}

	template<class decompression_settings_type>
	inline void decompression_context<decompression_settings_type>::seek(float sample_time, sample_rounding_policy rounding_policy)
	{
		algorithm_version_type::template seek<decompression_settings_type>(m_context, sample_time, rounding_policy);
	}

	template<class decompression_settings_type>
	template<class track_writer_type>
	inline void decompression_context<decompression_settings_type>::decompress_tracks(track_writer_type& writer)
	{
		algorithm_version_type::template decompress_tracks<decompression_settings_type>(m_context, writer);
	}

	template<class decompression_settings_type>
	template<class track_writer_type>
	inline void decompression_context<decompression_settings_type>::decompress_track(uint32_t track_index, track_writer_type& writer)
	{
		algorithm_version_type::template decompress_track<decompression_settings_type>(m_context, track_index, writer);
	}
}
