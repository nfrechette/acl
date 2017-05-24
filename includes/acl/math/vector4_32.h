#pragma once

#include "acl/math/math.h"

namespace acl
{
	inline Vector4_32 vector_set(float x, float y, float z, float w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps(x, y, z, w));
#else
		return Vector4_32{ x, y, z, w };
#endif
	}

	inline Vector4_32 vector_set(float xyzw)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps1(xyzw));
#else
		return Vector4_32{ xyzw, xyzw, xyzw, xyzw };
#endif
	}

	inline float vector_get_x(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(input);
#else
		return input.x;
#endif
	}

	inline float vector_get_y(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline float vector_get_z(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2)));
#else
		return input.z;
#endif
	}

	inline float vector_get_w(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3)));
#else
		return input.w;
#endif
	}

	inline Vector4_32 vector_add(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_add_ps(lhs, rhs);
#else
		return vector_set(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
#endif
	}

	inline Vector4_32 vector_sub(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_sub_ps(lhs, rhs);
#else
		return vector_set(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
#endif
	}

	inline Vector4_32 vector_mul(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_mul_ps(lhs, rhs);
#else
		return vector_set(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
#endif
	}

	inline Vector4_32 vector_lerp(const Vector4_32& start, const Vector4_32& end, float alpha)
	{
		return vector_add(start, vector_mul(vector_sub(end, start), vector_set(alpha)));
	}
}
