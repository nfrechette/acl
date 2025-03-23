#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"

#include <rtm/math.h>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// Stores the floating point environment information.
	struct fp_environment
	{
#if defined(RTM_SSE2_INTRINSICS)
		// TODO: It would be nice to clean this up and just cache the MXCSR register, see _mm_getcsr
		unsigned int exception_mask = 0;
		unsigned int flush_zero_mode = 0;
		unsigned int denormals_zero_mode = 0;
#elif defined(RTM_NEON_INTRINSICS)
		// TODO: Implement on ARM. API to do this is not consistent across Android, Windows ARM, and iOS
		// and on top of it, most ARM CPUs out there do not raise the SIGFPE trap so they are silent
#endif
	};

	// Enables floating point exceptions for invalid operations, division by zero, and overflow.
	inline void enable_fp_exceptions(fp_environment& out_old_env)
	{
#if defined(RTM_SSE2_INTRINSICS)
		// We only care about SSE and not x87
		// Clear any exceptions that might have been raised already
		_MM_SET_EXCEPTION_STATE(0);

		// Cache the exception mask we had so we can restore it later
		out_old_env.exception_mask = _MM_GET_EXCEPTION_MASK();

		// Enable our exceptions
		const unsigned int exception_flags = _MM_MASK_INVALID | _MM_MASK_DIV_ZERO | _MM_MASK_OVERFLOW;
		_MM_SET_EXCEPTION_MASK(~exception_flags & _MM_MASK_MASK);
#else
		(void)out_old_env;
#endif
	}

	// Disables all floating point exceptions.
	inline void disable_fp_exceptions(fp_environment& out_old_env)
	{
#if defined(RTM_SSE2_INTRINSICS)
		// We only care about SSE and not x87
		// Cache the exception mask we had so we can restore it later
		out_old_env.exception_mask = _MM_GET_EXCEPTION_MASK();

		// Disable all exceptions
		_MM_SET_EXCEPTION_MASK(_MM_MASK_MASK);
#else
		(void)out_old_env;
#endif
	}

	// Restores a previously set floating point exception state.
	inline void restore_fp_exceptions(const fp_environment& env)
	{
#if defined(RTM_SSE2_INTRINSICS)
		// We only care about SSE and not x87
		// Clear any exceptions that might have been raised already
		_MM_SET_EXCEPTION_STATE(0);

		// Restore our old mask value
		_MM_SET_EXCEPTION_MASK(env.exception_mask);
#else
		(void)env;
#endif
	}

	// Enables flushing floating point denormals to zero.
	inline void enable_flush_to_zero_fp_denormals(fp_environment& out_old_env)
	{
#if defined(RTM_SSE3_INTRINSICS)
		// We only care about SSE and not x87
		// Cache the flush to zero mask we had so we can restore it later
		out_old_env.flush_zero_mode = _MM_GET_FLUSH_ZERO_MODE();
		out_old_env.denormals_zero_mode = _MM_GET_DENORMALS_ZERO_MODE();

		// Enable flushing
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#else
		(void)out_old_env;
#endif
	}

	// Disables flushing floating point denormals to zero.
	inline void disable_flush_to_zero_fp_denormals(fp_environment& out_old_env)
	{
#if defined(RTM_SSE3_INTRINSICS)
		// We only care about SSE and not x87
		// Cache the flush to zero mask we had so we can restore it later
		out_old_env.flush_zero_mode = _MM_GET_FLUSH_ZERO_MODE();
		out_old_env.denormals_zero_mode = _MM_GET_DENORMALS_ZERO_MODE();

		// Disable flushing
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_OFF);
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_OFF);
#else
		(void)out_old_env;
#endif
	}

	// Restores a previously set floating point flush-to-zero state.
	inline void restore_flush_to_zero_fp_denormals(const fp_environment& env)
	{
#if defined(RTM_SSE3_INTRINSICS)
		// We only care about SSE and not x87
		// Restore our old value
		_MM_SET_FLUSH_ZERO_MODE(env.flush_zero_mode);
		_MM_SET_DENORMALS_ZERO_MODE(env.denormals_zero_mode);
#else
		(void)env;
#endif
	}

	// Enables floating point exceptions in the parent scope for invalid operations, division by zero, and overflow.
	class scope_enable_fp_exceptions
	{
	public:
		scope_enable_fp_exceptions()
		{
			enable_fp_exceptions(env);
		}

		~scope_enable_fp_exceptions()
		{
			restore_fp_exceptions(env);
		}

	private:
		// Prevent copy or move
		scope_enable_fp_exceptions(const scope_enable_fp_exceptions&) = delete;
		scope_enable_fp_exceptions& operator=(const scope_enable_fp_exceptions&) = delete;

		fp_environment env;
	};

	// Disables all floating point exceptions in the parent scope.
	class scope_disable_fp_exceptions
	{
	public:
		scope_disable_fp_exceptions()
		{
			disable_fp_exceptions(env);
		}

		~scope_disable_fp_exceptions()
		{
			restore_fp_exceptions(env);
		}

	private:
		// Prevent copy or move
		scope_disable_fp_exceptions(const scope_disable_fp_exceptions&) = delete;
		scope_disable_fp_exceptions& operator=(const scope_disable_fp_exceptions&) = delete;

		fp_environment env;
	};

	// Enables flushing floating point denormals to zero.
	class scope_enable_flush_to_zero_fp_denormals
	{
	public:
		scope_enable_flush_to_zero_fp_denormals()
		{
			enable_flush_to_zero_fp_denormals(env);
		}

		~scope_enable_flush_to_zero_fp_denormals()
		{
			restore_flush_to_zero_fp_denormals(env);
		}

	private:
		// Prevent copy or move
		scope_enable_flush_to_zero_fp_denormals(const scope_enable_flush_to_zero_fp_denormals&) = delete;
		scope_enable_flush_to_zero_fp_denormals& operator=(const scope_enable_flush_to_zero_fp_denormals&) = delete;

		fp_environment env;
	};

	// Disables flushing floating point denormals to zero.
	class scope_disable_flush_to_zero_fp_denormals
	{
	public:
		scope_disable_flush_to_zero_fp_denormals()
		{
			disable_flush_to_zero_fp_denormals(env);
		}

		~scope_disable_flush_to_zero_fp_denormals()
		{
			restore_flush_to_zero_fp_denormals(env);
		}

	private:
		// Prevent copy or move
		scope_disable_flush_to_zero_fp_denormals(const scope_disable_flush_to_zero_fp_denormals&) = delete;
		scope_disable_flush_to_zero_fp_denormals& operator=(const scope_disable_flush_to_zero_fp_denormals&) = delete;

		fp_environment env;
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
