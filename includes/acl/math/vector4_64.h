#pragma once

#include "acl/math/math.h"

#if defined(ACL_NO_INTRINSICS)
#include <algorithm>
#endif

namespace acl
{
	inline Vector4_64 vector_set(double x, double y, double z, double w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_set_pd(x, y), _mm_set_pd(z, w) };
#else
		return Vector4_64{ x, y, z, w };
#endif
	}

	inline Vector4_64 vector_set(double xyzw)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xyzw_pd = _mm_set1_pd(xyzw);
		return Vector4_64{ xyzw_pd, xyzw_pd };
#else
		return Vector4_64{ xyzw, xyzw, xyzw, xyzw };
#endif
	}

	inline double vector_get_x(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.xy);
#else
		return input.x;
#endif
	}

	inline double vector_get_y(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.xy, input.xy, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline double vector_get_z(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.zw);
#else
		return input.z;
#endif
	}

	inline double vector_get_w(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.zw, input.zw, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.w;
#endif
	}

	inline Vector4_64 vector_add(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_add_pd(lhs.xy, rhs.xy), _mm_add_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
#endif
	}

	inline Vector4_64 vector_sub(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_sub_pd(lhs.xy, rhs.xy), _mm_sub_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
#endif
	}

	inline Vector4_64 vector_mul(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_mul_pd(lhs.xy, rhs.xy), _mm_mul_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
#endif
	}

	inline Vector4_64 vector_max(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_max_pd(lhs.xy, rhs.xy), _mm_max_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y), std::max(lhs.z, rhs.z), std::max(lhs.w, rhs.w));
#endif
	}

	inline Vector4_64 vector_min(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_min_pd(lhs.xy, rhs.xy), _mm_min_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y), std::min(lhs.z, rhs.z), std::min(lhs.w, rhs.w));
#endif
	}

	inline Vector4_64 vector_abs(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		Vector4_64 zero{ _mm_setzero_pd(), _mm_setzero_pd() };
		return vector_max(vector_sub(zero, input), input);
#else
		return vector_set(std::abs(input.x), std::abs(input.y), std::abs(input.z), std::abs(input.w));
#endif
	}

	inline bool vector_all_less_than(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_lt_pd = _mm_cmplt_pd(lhs.xy, rhs.xy);
		__m128d zw_lt_pd = _mm_cmplt_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_lt_pd) & _mm_movemask_pd(zw_lt_pd)) == 3;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z && lhs.w < rhs.w;
#endif
	}

	inline bool vector_any_less_than(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_lt_pd = _mm_cmplt_pd(lhs.xy, rhs.xy);
		__m128d zw_lt_pd = _mm_cmplt_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_lt_pd) + _mm_movemask_pd(zw_lt_pd)) < 6;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z || lhs.w < rhs.w;
#endif
	}
}
