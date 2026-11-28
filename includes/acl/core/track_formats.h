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

/// \file
/// The formats of the rotation, translation, scale, and scalar samples.

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C26495: Variable '...' is uninitialized. Always initialize a member variable (type.6).
	// We explicitly control initialization
	#pragma warning(disable : 26495)
#endif

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The rotation format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	/// The format of the rotation samples of transform tracks.
	enum class rotation_format8 : uint8_t
	{
		quatf_full					= 0,	///< Full precision quaternion, [x,y,z,w] stored with float32
		//quatf_variable			= 1,	// TODO Quantized quaternion, [x,y,z,w] stored with [N,N,N,N] bits (same number of bits per component)
		quatf_drop_w_full			= 2,	///< Full precision quaternion, [x,y,z] stored with float32 (w is dropped)
		quatf_drop_w_variable		= 3,	///< Quantized quaternion, [x,y,z] stored with [N,N,N] bits (w is dropped, same number of bits per component)
		quatf_drop_largest_full		= 4,	///< Full precision quaternion, [a,b,c] stored with float32 (the largest component of each sample is dropped)
		quatf_drop_largest_variable	= 5,	///< Quantized quaternion, [a,b,c] stored with [N,N,N] bits (each segment drops the component whose smallest absolute value is the largest, same number of bits per component)

		//quatf_optimal				= 15,	// Mix of quatf_variable and quatf_drop_w_variable

		// TODO: Implement these?
		//quatf_log_full,					// Full precision quaternion logarithm, [x,y,z] stored with float32
		//quatf_log_variable,				// Quantized quaternion logarithm, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	// BE CAREFUL WHEN CHANGING VALUES IN THIS ENUM
	// The vector format is serialized in the compressed data, if you change a value
	// the compressed clips will be invalid. If you do, bump the appropriate algorithm versions.
	/// The format of the translation and scale samples of transform tracks.
	enum class vector_format8 : uint8_t
	{
		vector3f_full				= 0,	///< Full precision vector3f, [x,y,z] stored with float32
		vector3f_variable			= 1,	///< Quantized vector3f, [x,y,z] stored with [N,N,N] bits (same number of bits per component)
	};

	/// The format of the samples of scalar tracks.
	/// The compressed data does not store this value, the bit rate of each track tells how its samples are stored.
	enum class scalar_format8 : uint8_t
	{
		scalarf_full				= 0,	///< Full precision scalar track, each component stored with float32
		scalarf_variable			= 1,	///< Quantized scalar track, each component stored with N bits (same number of bits per component), N depends on the track precision
	};

	/// The format of a rotation, translation, or scale sub-track.
	union track_format8
	{
		rotation_format8 rotation;		///< The format of a rotation sub-track
		vector_format8 vector;			///< The format of a translation or scale sub-track

		/// Constructs a format without a value.
		track_format8() noexcept {}

		/// Constructs the format of a rotation sub-track.
		explicit track_format8(rotation_format8 format) noexcept : rotation(format) {}

		/// Constructs the format of a translation or scale sub-track.
		explicit track_format8(vector_format8 format) noexcept : vector(format) {}
	};

	/// The type of a sub-track of a transform track.
	enum class animation_track_type8 : uint8_t
	{
		rotation,		///< The rotation sub-track
		translation,	///< The translation sub-track
		scale,			///< The scale sub-track
	};

	/// The representation of a rotation, without its precision.
	enum class rotation_variant8 : uint8_t
	{
		quat,			///< A quaternion with the four components [x,y,z,w]
		quat_drop_w,	///< A quaternion with the three components [x,y,z], [w] is dropped
		quat_drop_largest,	///< A quaternion with three components, one component is dropped
		//quat_log,
	};

	/// Returns the name of a rotation format.
	const char* get_rotation_format_name(rotation_format8 format);

	/// Finds the rotation format that has the given name.
	/// Returns true and writes the format to `out_format` when the name is valid, otherwise returns false.
	bool get_rotation_format(const char* format, rotation_format8& out_format);

	/// Returns the name of a vector format.
	constexpr const char* get_vector_format_name(vector_format8 format);

	/// Finds the vector format that has the given name.
	/// Returns true and writes the format to `out_format` when the name is valid, otherwise returns false.
	bool get_vector_format(const char* format, vector_format8& out_format);

	/// Returns the name of a scalar format.
	constexpr const char* get_scalar_format_name(scalar_format8 format);

	/// Finds the scalar format that has the given name.
	/// Returns true and writes the format to `out_format` when the name is valid, otherwise returns false.
	bool get_scalar_format(const char* format, scalar_format8& out_format);

	/// Returns the rotation variant of a rotation format.
	constexpr rotation_variant8 get_rotation_variant(rotation_format8 rotation_format);

	/// Returns the full precision rotation format of a rotation variant.
	constexpr rotation_format8 get_highest_variant_precision(rotation_variant8 variant);

	/// Returns true if the rotation format uses a variable bit rate.
	constexpr bool is_rotation_format_variable(rotation_format8 format);

	/// Returns true if the rotation format stores each component with float32.
	constexpr bool is_rotation_format_full_precision(rotation_format8 format);

	/// Returns true if the vector format uses a variable bit rate.
	constexpr bool is_vector_format_variable(vector_format8 format);

	/// Returns true if the vector format stores each component with float32.
	constexpr bool is_vector_format_full_precision(vector_format8 format);

	ACL_IMPL_VERSION_NAMESPACE_END
}

#include "acl/core/impl/track_formats.impl.h"

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif

ACL_IMPL_FILE_PRAGMA_POP
