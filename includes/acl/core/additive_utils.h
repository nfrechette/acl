#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
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

#include <rtm/qvvd.h>
#include <rtm/qvvf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Describes the format used by the additive clip.
	enum class AdditiveClipFormat8 : uint8_t
	{
		//////////////////////////////////////////////////////////////////////////
		// Clip is not additive
		None				= 0,

		//////////////////////////////////////////////////////////////////////////
		// Clip is in relative space, transform_mul or equivalent is used to combine them.
		// transform = transform_mul(additive_transform, base_transform)
		Relative			= 1,

		//////////////////////////////////////////////////////////////////////////
		// Clip is in additive space where scale is combined with: base_scale * additive_scale
		// transform = transform_add0(additive_transform, base_transform)
		Additive0			= 2,

		//////////////////////////////////////////////////////////////////////////
		// Clip is in additive space where scale is combined with: base_scale * (1.0 + additive_scale)
		// transform = transform_add1(additive_transform, base_transform)
		Additive1			= 3,
	};

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline const char* get_additive_clip_format_name(AdditiveClipFormat8 format)
	{
		switch (format)
		{
		case AdditiveClipFormat8::None:				return "None";
		case AdditiveClipFormat8::Relative:			return "Relative";
		case AdditiveClipFormat8::Additive0:		return "Additive0";
		case AdditiveClipFormat8::Additive1:		return "Additive1";
		default:									return "<Invalid>";
		}
	}

	inline bool get_additive_clip_format(const char* format, AdditiveClipFormat8& out_format)
	{
		const char* none_format = "None";
		if (std::strncmp(format, none_format, std::strlen(none_format)) == 0)
		{
			out_format = AdditiveClipFormat8::None;
			return true;
		}

		const char* relative_format = "Relative";
		if (std::strncmp(format, relative_format, std::strlen(relative_format)) == 0)
		{
			out_format = AdditiveClipFormat8::Relative;
			return true;
		}

		const char* additive0_format = "Additive0";
		if (std::strncmp(format, additive0_format, std::strlen(additive0_format)) == 0)
		{
			out_format = AdditiveClipFormat8::Additive0;
			return true;
		}

		const char* additive1_format = "Additive1";
		if (std::strncmp(format, additive1_format, std::strlen(additive1_format)) == 0)
		{
			out_format = AdditiveClipFormat8::Additive1;
			return true;
		}

		return false;
	}

	inline rtm::vector4f RTM_SIMD_CALL get_default_scale(AdditiveClipFormat8 additive_format)
	{
		return additive_format == AdditiveClipFormat8::Additive1 ? rtm::vector_zero() : rtm::vector_set(1.0F);
	}

	inline rtm::qvvf RTM_SIMD_CALL transform_add0(rtm::qvvf_arg0 base, rtm::qvvf_arg1 additive)
	{
		const rtm::quatf rotation = rtm::quat_mul(additive.rotation, base.rotation);
		const rtm::vector4f translation = rtm::vector_add(additive.translation, base.translation);
		const rtm::vector4f scale = rtm::vector_mul(additive.scale, base.scale);
		return rtm::qvv_set(rotation, translation, scale);
	}

	inline rtm::qvvf RTM_SIMD_CALL transform_add1(rtm::qvvf_arg0 base, rtm::qvvf_arg1 additive)
	{
		const rtm::quatf rotation = rtm::quat_mul(additive.rotation, base.rotation);
		const rtm::vector4f translation = rtm::vector_add(additive.translation, base.translation);
		const rtm::vector4f scale = rtm::vector_mul(rtm::vector_add(rtm::vector_set(1.0F), additive.scale), base.scale);
		return rtm::qvv_set(rotation, translation, scale);
	}

	inline rtm::qvvf RTM_SIMD_CALL transform_add_no_scale(rtm::qvvf_arg0 base, rtm::qvvf_arg1 additive)
	{
		const rtm::quatf rotation = rtm::quat_mul(additive.rotation, base.rotation);
		const rtm::vector4f translation = rtm::vector_add(additive.translation, base.translation);
		return rtm::qvv_set(rotation, translation, rtm::vector_set(1.0F));
	}

	inline rtm::qvvf RTM_SIMD_CALL apply_additive_to_base(AdditiveClipFormat8 additive_format, rtm::qvvf_arg0 base, rtm::qvvf_arg1 additive)
	{
		switch (additive_format)
		{
		default:
		case AdditiveClipFormat8::None:			return additive;
		case AdditiveClipFormat8::Relative:		return rtm::qvv_mul(additive, base);
		case AdditiveClipFormat8::Additive0:	return transform_add0(base, additive);
		case AdditiveClipFormat8::Additive1:	return transform_add1(base, additive);
		}
	}

	inline rtm::qvvf RTM_SIMD_CALL apply_additive_to_base_no_scale(AdditiveClipFormat8 additive_format, rtm::qvvf_arg0 base, rtm::qvvf_arg1 additive)
	{
		switch (additive_format)
		{
		default:
		case AdditiveClipFormat8::None:			return additive;
		case AdditiveClipFormat8::Relative:		return rtm::qvv_mul_no_scale(additive, base);
		case AdditiveClipFormat8::Additive0:	return transform_add_no_scale(base, additive);
		case AdditiveClipFormat8::Additive1:	return transform_add_no_scale(base, additive);
		}
	}

	inline rtm::qvvd convert_to_relative(const rtm::qvvd& base, const rtm::qvvd& transform)
	{
		return rtm::qvv_mul(transform, rtm::qvv_inverse(base));
	}

	inline rtm::qvvd convert_to_additive0(const rtm::qvvd& base, const rtm::qvvd& transform)
	{
		const rtm::quatd rotation = rtm::quat_mul(transform.rotation, rtm::quat_conjugate(base.rotation));
		const rtm::vector4d translation = rtm::vector_sub(transform.translation, base.translation);
		const rtm::vector4d scale = rtm::vector_div(transform.scale, base.scale);
		return rtm::qvv_set(rotation, translation, scale);
	}

	inline rtm::qvvd convert_to_additive1(const rtm::qvvd& base, const rtm::qvvd& transform)
	{
		const rtm::quatd rotation = rtm::quat_mul(transform.rotation, rtm::quat_conjugate(base.rotation));
		const rtm::vector4d translation = rtm::vector_sub(transform.translation, base.translation);
		const rtm::vector4d scale = rtm::vector_sub(rtm::vector_mul(transform.scale, rtm::vector_reciprocal(base.scale)), rtm::vector_set(1.0));
		return rtm::qvv_set(rotation, translation, scale);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
