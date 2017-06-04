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

#include <stdint.h>

namespace acl
{
	// Misc globals
	static constexpr uint32_t COMPRESSED_CLIP_TAG					= 0xac10ac10;

	// Algorithm version numbers
	static constexpr uint16_t FULL_PRECISION_ALGORITHM_VERSION		= 0;

	enum class AlgorithmType : uint16_t
	{
		FullPrecision			= 0,
	};

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint16_t get_algorithm_version(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::FullPrecision:	return FULL_PRECISION_ALGORITHM_VERSION;
			default:							return 0xFFFF;
		}
	}

	// TODO: constexpr
	inline bool is_valid_algorithm_type(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::FullPrecision:	return true;
			default:							return false;
		}
	}

	// TODO: constexpr
	inline const char* get_algorithm_name(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::FullPrecision:	return "Full Precision";
			default:							return "<Unknown>";
		}
	}
}
