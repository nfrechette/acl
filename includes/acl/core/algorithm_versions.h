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

#include "acl/core/algorithm_types.h"

#include <cstdint>

namespace acl
{
	// Algorithm version numbers
	static constexpr uint16_t ALGORITHM_VERSION_UNIFORMLY_SAMPLED		= 1;
	//static constexpr uint16_t ALGORITHM_VERSION_LINEAR_KEY_REDUCTION	= 0;
	//static constexpr uint16_t ALGORITHM_VERSION_SPLINE_KEY_REDUCTION	= 0;

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint16_t get_algorithm_version(AlgorithmType8 type)
	{
		switch (type)
		{
			case AlgorithmType8::UniformlySampled:		return ALGORITHM_VERSION_UNIFORMLY_SAMPLED;
			//case AlgorithmType8::LinearKeyReduction:	return ALGORITHM_VERSION_LINEAR_KEY_REDUCTION;
			//case AlgorithmType8::SplineKeyReduction:	return ALGORITHM_VERSION_SPLINE_KEY_REDUCTION;
			default:									return 0xFFFF;
		}
	}
}
