#pragma once

#include "acl/math/math.h"

#include <algorithm>

namespace acl
{
	inline float floor(float input)
	{
		return float(uint32_t(input));
	}

	inline float clamp(float input, float min, float max)
	{
		return std::min(std::max(input, min), max);
	}

	inline float sqrt(float input)
	{
		return std::sqrt(input);
	}
}
