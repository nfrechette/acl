#pragma once

#include <assert.h>

namespace acl
{
	// TODO: Make this a macro, that way the game code can trivially override it
	inline void ensure(bool expression)
	{
		assert(expression);
	}
}
