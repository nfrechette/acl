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

#include "acl/core/memory.h"
#include "acl/core/algorithm_versions.h"

#include <stdint.h>

namespace acl
{
	class alignas(16) CompressedClip
	{
	public:
		AlgorithmType8 get_algorithm_type() const { return m_type; }
		uint32_t get_size() const { return m_size; }

		bool is_valid(bool check_crc) const
		{
			if (!is_aligned_to(this, alignof(CompressedClip)))
				return false;

			if (m_tag != COMPRESSED_CLIP_TAG)
				return false;

			if (!is_valid_algorithm_type(m_type))
				return false;

			if (m_version != get_algorithm_version(m_type))
				return false;

			if (check_crc)
				return true;	// TODO: Implement
			return true;
		}

	private:
		static constexpr uint32_t COMPRESSED_CLIP_TAG = 0xac10ac10;

		CompressedClip(uint32_t size, AlgorithmType8 type)
			: m_size(size)
			, m_crc32(0)		// TODO: Implement
			, m_tag(COMPRESSED_CLIP_TAG)
			, m_version(get_algorithm_version(type))
			, m_type(type)
			, m_padding(0)
		{}

		// 16 byte header, the rest of the data follows in memory
		uint32_t		m_size;
		uint32_t		m_crc32;

		// Everything starting here is included in the CRC32
		uint32_t		m_tag;
		uint16_t		m_version;
		AlgorithmType8	m_type;
		uint8_t			m_padding;

		friend CompressedClip* make_compressed_clip(void* buffer, uint32_t size, AlgorithmType8 type);
		friend void finalize_compressed_clip(CompressedClip& compressed_clip);
	};

	static_assert(alignof(CompressedClip) == 16, "Invalid alignment for CompressedClip");
	static_assert(sizeof(CompressedClip) == 16, "Invalid size for CompressedClip");
}
