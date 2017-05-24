#pragma once

#include "acl/math/math.h"

namespace acl
{
	inline Quat_64 quat_set(double x, double y, double z, double w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Quat_64{ _mm_set_pd(x, y), _mm_set_pd(z, w) };
#else
		return Quat_64{ x, y, z, w };
#endif
	}

	inline double quat_get_x(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.xy);
#else
		return input.x;
#endif
	}

	inline double quat_get_y(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.xy, input.xy, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline double quat_get_z(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.zw);
#else
		return input.z;
#endif
	}

	inline double quat_get_w(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.zw, input.zw, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.w;
#endif
	}
}
