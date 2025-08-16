#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2025 Nicholas Frechette & Animation Compression Library contributors
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

#include <cstdint>

#define ACL_IMPL_USE_ANIMATED_PREFETCH

// Try our an alternate method of masking min/extent remap results (segment)
// With Apple Clang, we save a few instructions but overall we are slightly slower on M1
// probably because the masking happens after the fma which depends on the unpacked values.
// If the unpacked values stall (e.g. due to cache miss), then the masking is a dependent
// instruction but in the original code, it is independent and so can happen for free
// if the scheduler window is long enough.
// With Clang 18, enabling this triggers the value unpacking to inline which leads to
// a small performance improvement.
//#define ACL_IMPL_ALTERNATE_MIN_EXTENT_MASKING

// Try our an alternate method of masking min/extent remap results (clip)
// With Apple Clang, we save a few instructions but overall we are slightly slower on M1
// probably because the masking happens after the fma which depends on the unpacked values.
// If the unpacked values stall (e.g. due to cache miss), then the masking is a dependent
// instruction but in the original code, it is independent and so can happen for free
// if the scheduler window is long enough.
// With Clang 18, enabling this reverts the value unpacking back to not inline but overall
// still yields a small perf win vs reference
//#define ACL_IMPL_ALTERNATE_MIN_EXTENT_MASKING2

// Try an alternative method to unpack segment metadata using vtbl
// With Apple Clang, we save a few instructions, mainly when we unpack the second segment
// as we can re-use the constants. Overall, we are slightly slower.
// With Clang 18, we end up with a slight performance improvement
//#define ACL_IMPL_ALTERNATE_SEGMENT_UNPACK

// On x86/x64 platforms the prefetching instruction can have a long latency and it requires
// a few other registers to compute the address which is problematic when registers are scarce.
// As such, we attempt to hide the prefetching behind longer latency instructions like square-roots
// and divisions.
// On other platforms (e.g. ARM), the instruction is cheaper and we have more registers which gives
// the compiler more freedom to hide the address calculation cost between other instructions.
// Because the CPU is generally slower as well, we want to prefetch as soon as possible without
// waiting for the next expensive instruction.
// If your target CPU has a high clock rate, you might benefit from disabling early prefetching
#if !defined(ACL_NO_EARLY_PREFETCHING) && !defined(ACL_IMPL_PREFETCH_EARLY)
	#if !defined(RTM_SSE2_INTRINSICS)
		#define ACL_IMPL_PREFETCH_EARLY
	#endif
#endif

// This defined enables the SIMD 8 wide AVX decompression code path
// Note that currently, it is often slower than the regular SIMD 4 wide AVX code path
// On Intel Haswell and AMD Zen2 CPUs, the 8 wide code is measurably slower
// Perhaps it is faster on newer Intel CPUs but I don't have one to test with
// Enable at your own risk
//#define ACL_IMPL_USE_AVX_8_WIDE_DECOMP

#if defined(ACL_IMPL_USE_AVX_8_WIDE_DECOMP)
	#if !defined(RTM_AVX_INTRINSICS)
		// AVX isn't enabled, disable the 8 wide code path
		#undef ACL_IMPL_USE_AVX_8_WIDE_DECOMP
	#endif
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
#if defined(ACL_IMPL_USE_ANIMATED_PREFETCH)
	#define ACL_IMPL_ANIMATED_PREFETCH(ptr) memory_prefetch(ptr)
#else
	#define ACL_IMPL_ANIMATED_PREFETCH(ptr) (void)(ptr)
#endif
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
