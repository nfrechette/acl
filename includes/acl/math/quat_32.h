#pragma once

#include "acl/math/math.h"
#include "acl/math/scalar_32.h"

namespace acl
{
	inline Quat_32 quat_set(float x, float y, float z, float w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Quat_32(_mm_set_ps(x, y, z, w));
#else
		return Quat_32{ x, y, z, w };
#endif
	}

	inline float quat_get_x(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(input);
#else
		return input.x;
#endif
	}

	inline float quat_get_y(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline float quat_get_z(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2)));
#else
		return input.z;
#endif
	}

	inline float quat_get_w(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3)));
#else
		return input.w;
#endif
	}

	inline float quat_length_squared(const Quat_32& input)
	{
		// TODO: Use dot instruction
		return (quat_get_x(input) * quat_get_x(input)) + (quat_get_y(input) + quat_get_y(input)) + (quat_get_z(input) + quat_get_z(input)) + (quat_get_w(input) + quat_get_w(input));
	}

	inline float quat_length(const Quat_32& input)
	{
		// TODO: Use intrinsics to avoid scalar coercion
		return sqrt(quat_length_squared(input));
	}

	inline float quat_length_reciprocal(const Quat_32& input)
	{
		// TODO: Use recip instruction
		return 1.0f / sqrt(quat_length_squared(input));
	}

	inline Quat_32 quat_normalize(const Quat_32& input)
	{
		// TODO: Use vector mul instruction
		float length_recip = quat_length_reciprocal(input);
		return quat_set(quat_get_x(input) * length_recip, quat_get_y(input) * length_recip, quat_get_z(input) * length_recip, quat_get_w(input) * length_recip);
	}
}
