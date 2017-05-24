#pragma once

#include "acl/math/math.h"

namespace acl
{
#if defined(ACL_SSE2_INTRINSICS)
	typedef __m128 Quat_32;
	typedef __m128 Vector4_32;

	struct Quat_64
	{
		__m128d xy;
		__m128d zw;
	};

	struct Vector4_64
	{
		__m128d xy;
		__m128d zw;
	};
#else
	struct Quat_32
	{
		float x;
		float y;
		float z;
		float w;
	};

	struct Vector4_32
	{
		float x;
		float y;
		float z;
		float w;
	};

	struct Quat_64
	{
		double x;
		double y;
		double z;
		double w;
	};

	struct Vector4_64
	{
		double x;
		double y;
		double z;
		double w;
	};
#endif
}
