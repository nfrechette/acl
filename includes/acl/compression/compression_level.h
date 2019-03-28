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

#include <cstdint>
#include <cstring>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// CompressionLevel8 represents how aggressively we attempt to reduce the memory
	// footprint. Higher levels will try more permutations and bit rates. The higher
	// the level, the slower the compression but the smaller the memory footprint.
	enum class CompressionLevel8 : uint8_t
	{
		Lowest		= 0,	// Same as Medium for now
		Low			= 1,	// Same as Medium for now
		Medium		= 2,
		High		= 3,
		Highest		= 4,
	};

	//////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////
	// Returns a string representing the compression level.
	// TODO: constexpr
	inline const char* get_compression_level_name(CompressionLevel8 level)
	{
		switch (level)
		{
		case CompressionLevel8::Lowest:		return "Lowest";
		case CompressionLevel8::Low:		return "Low";
		case CompressionLevel8::Medium:		return "Medium";
		case CompressionLevel8::High:		return "High";
		case CompressionLevel8::Highest:	return "Highest";
		default:							return "<Invalid>";
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns the compression level from its string representation.
	inline bool get_compression_level(const char* level_name, CompressionLevel8& out_level)
	{
		const char* level_lowest = "Lowest";
		if (std::strncmp(level_name, level_lowest, std::strlen(level_lowest)) == 0)
		{
			out_level = CompressionLevel8::Lowest;
			return true;
		}

		const char* level_low = "Low";
		if (std::strncmp(level_name, level_low, std::strlen(level_low)) == 0)
		{
			out_level = CompressionLevel8::Low;
			return true;
		}

		const char* level_medium = "Medium";
		if (std::strncmp(level_name, level_medium, std::strlen(level_medium)) == 0)
		{
			out_level = CompressionLevel8::Medium;
			return true;
		}

		const char* level_highest = "Highest";
		if (std::strncmp(level_name, level_highest, std::strlen(level_highest)) == 0)
		{
			out_level = CompressionLevel8::Highest;
			return true;
		}

		const char* level_high = "High";
		if (std::strncmp(level_name, level_high, std::strlen(level_high)) == 0)
		{
			out_level = CompressionLevel8::High;
			return true;
		}

		return false;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
