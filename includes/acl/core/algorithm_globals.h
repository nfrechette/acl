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
	// Algorithm version numbers
	static constexpr uint16_t UNIFORMLY_SAMPLED_FULL_PRECISION_ALGORITHM_VERSION		= 0;
	static constexpr uint16_t UNIFORMLY_SAMPLED_FIXED_QUANTIZATION_ALGORITHM_VERSION	= 0;

	enum class AlgorithmType : uint16_t
	{
		UniformlySampledFullPrecision			= 0,
		UniformlySampledFixedQuantization		= 1,
	};

	enum class RotationFormat : uint8_t
	{
		Quat,					// Full quaternion with x,y,z,w
		QuatXYZ,				// Quaternion with dropped 'w'
		//QuatLog,
	};

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint16_t get_algorithm_version(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::UniformlySampledFullPrecision:		return UNIFORMLY_SAMPLED_FULL_PRECISION_ALGORITHM_VERSION;
			case AlgorithmType::UniformlySampledFixedQuantization:	return UNIFORMLY_SAMPLED_FIXED_QUANTIZATION_ALGORITHM_VERSION;
			default:												return 0xFFFF;
		}
	}

	// TODO: constexpr
	inline bool is_valid_algorithm_type(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::UniformlySampledFullPrecision:
			case AlgorithmType::UniformlySampledFixedQuantization:
				return true;
			default:
				return false;
		}
	}

	// TODO: constexpr
	inline const char* get_algorithm_name(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::UniformlySampledFullPrecision:		return "Uniformly Sampled Full Precision";
			case AlgorithmType::UniformlySampledFixedQuantization:	return "Uniformly Sampled Fixed Quantization";
			default:												return "<Unknown>";
		}
	}

	// TODO: constexpr
	inline const char* get_rotation_format_name(RotationFormat format)
	{
		switch (format)
		{
			case RotationFormat::Quat:		return "Quat";
			case RotationFormat::QuatXYZ:	return "Quat Dropped W";
			default:						return "<Unknown>";
		}
	}
}
