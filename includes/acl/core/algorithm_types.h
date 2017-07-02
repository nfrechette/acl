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

#include "acl/core/enum_utils.h"

#include <stdint.h>

namespace acl
{
	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The algorithm type is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class AlgorithmType8 : uint8_t
	{
		UniformlySampled			= 0,
		//LinearKeyReduction			= 1,
		//SplineKeyReduction			= 2,
	};

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline bool is_valid_algorithm_type(AlgorithmType8 type)
	{
		switch (type)
		{
			case AlgorithmType8::UniformlySampled:
			//case AlgorithmType8::LinearKeyReduction:
			//case AlgorithmType8::SplineKeyReduction:
				return true;
			default:
				return false;
		}
	}

	// TODO: constexpr
	inline const char* get_algorithm_name(AlgorithmType8 type)
	{
		switch (type)
		{
			case AlgorithmType8::UniformlySampled:		return "Uniformly Sampled";
			//case AlgorithmType8::LinearKeyReduction:	return "Linear Key Reduction";
			//case AlgorithmType8::SplineKeyReduction:	return "Spline Key Reduction";
			default:									return "<Invalid>";
		}
	}
}
