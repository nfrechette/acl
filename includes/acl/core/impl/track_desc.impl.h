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

// Included only once from track_desc.h

#include "acl/core/error_result.h"
#include "acl/core/track_types.h"

#include <rtm/scalarf.h>

#include <cstdint>

namespace acl
{
	inline error_result track_desc_scalarf::is_valid() const
	{
		if (precision < 0.0F || !rtm::scalar_is_finite(precision))
			return error_result("Invalid precision");

		return error_result();
	}

	inline error_result track_desc_transformf::is_valid() const
	{
		if (precision < 0.0F || !rtm::scalar_is_finite(precision))
			return error_result("Invalid precision");

		if (shell_distance < 0.0F || !rtm::scalar_is_finite(shell_distance))
			return error_result("Invalid shell_distance");

		if (constant_rotation_threshold_angle < 0.0F || !rtm::scalar_is_finite(constant_rotation_threshold_angle))
			return error_result("Invalid constant_rotation_threshold_angle");

		if (constant_translation_threshold < 0.0F || !rtm::scalar_is_finite(constant_translation_threshold))
			return error_result("Invalid constant_translation_threshold");

		if (constant_scale_threshold < 0.0F || !rtm::scalar_is_finite(constant_scale_threshold))
			return error_result("Invalid constant_scale_threshold");

		return error_result();
	}
}
