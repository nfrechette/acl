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
	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The algorithm type is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class AlgorithmType8 : uint8_t
	{
		UniformlySampled			= 0,
		//LinearKeyReduction			= 1,
		//SplineKeyReduction			= 2,
	};

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The rotation format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class RotationFormat8 : uint8_t
	{
		Quat_128				= 0,	// Full precision quaternion, [x,y,z,w] stored with float32
		Quat_96					= 1,	// Full precision quaternion, [x,y,z] stored with float32 (w is dropped)
		Quat_48					= 2,	// Quantized quaternion, [x,y,z] stored with [16,16,16] bits (w is dropped)
		Quat_32					= 3,	// Quantized quaternion, [x,y,z] stored with [11,11,10] bits (w is dropped)
		// TODO: Implement these
		//Quat_32_Largest,		// Quantized quaternion, [?,?,?] stored with [10,10,10] bits (largest component is dropped, component index stored on 2 bits)
		//Quat_Variable,		// Quantized quaternion, [x,y,z] stored with [N,N,N] bits (w is dropped, same number of bits per component)
		//QuatLog_96,			// Full precision quaternion logarithm, [x,y,z] stored with float 32
		//QuatLog_48,			// Quantized quaternion logarithm, [x,y,z] stored with [16,16,16] bits
		//QuatLog_32,			// Quantized quaternion logarithm, [x,y,z] stored with [11,11,10] bits
		//QuatLog_Variable,		// Quantized quaternion logarithm, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The vector format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	enum class VectorFormat8 : uint8_t
	{
		Vector3_96				= 0,	// Full precision vector3, [x,y,z] stored with float32
		Vector3_48				= 1,	// Quantized vector3, [x,y,z] stored with [16,16,16] bits
		// TODO: Implement these
		
		//Vector3_32,			// Quantized vector3, [x,y,z] stored with [11,11,10] bits
		//Vector3_Variable,		// Quantized vector3, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
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
			default:									return "<Unknown>";
		}
	}

	// TODO: constexpr
	inline const char* get_rotation_format_name(RotationFormat8 format)
	{
		switch (format)
		{
			case RotationFormat8::Quat_128:		return "Quat 128";
			case RotationFormat8::Quat_96:		return "Quat 96";
			case RotationFormat8::Quat_48:		return "Quat 48";
			case RotationFormat8::Quat_32:		return "Quat 32";
			default:							return "<Unknown>";
		}
	}

	// TODO: constexpr
	inline const char* get_vector_format_name(VectorFormat8 format)
	{
		switch (format)
		{
		case VectorFormat8::Vector3_96:		return "Vector3 96";
		case VectorFormat8::Vector3_48:		return "Vector3 48";
		default:							return "<Unknown>";
		}
	}
}
