#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2022 Nicholas Frechette & Animation Compression Library contributors
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

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// This enum dictates how looping is handled.
	enum class sample_looping_policy
	{
		//////////////////////////////////////////////////////////////////////////
		// The sample time is clamped between [0, clip duration], inclusive.
		// This is equivalent to the clamp policy and is provided for readability.
		// Both values are interchangeable.
		// This should always be used for non-looping playback whether or not the
		// looping clips should wrap.
		non_looping = 0,

		//////////////////////////////////////////////////////////////////////////
		// The sample time is clamped between [0, clip duration], inclusive.
		// This is the proper way to handle looping clips. This means that clips
		// that need to loop seamlessly require their last sample to match the first
		// sample because we never interpolate between the last and first samples
		// as we loop. This is necessary because not all tracks can interpolate
		// the last and first samples safely. For example, root motion is often
		// stored as a translation delta from the start of the clip meaning we would
		// interpolate values that can be very different.
		// e.g. a walking character would have its root start at 0,0,0 and end
		// at 100,0,0 (walking 100cm in the X axis)
		// This makes it possible to extract the total root motion by sampling at
		// the full duration of the clip and at 0 seconds while subtracting the two.
		// This is the prior (and recommended) ACL behavior for v2.0 and earlier.
		clamp = 0,

		//////////////////////////////////////////////////////////////////////////
		// The sample time wraps around the end of the clip meaning that the
		// last and first samples can interpolate. This saves a tiny bit of memory
		// by not requiring the last sample to match the first but it adds a lot of
		// complexity and can often lead to incorrect or unwanted behavior.
		// e.g. when root motion is present, see above.
		// This means that the duration of a clip depends on whether or not its
		// playback is looping because looping clips have a missing sample: the first
		// sample repeats. This is problematic because it means that sampling a
		// clip at 0 seconds and at its full duration is equivalent and both will
		// return the exact same sample values.
		// This should only be used in runtimes that require wrapping and only
		// when sampling a looping clip.
		// DO NOT USE this for non-looping playback.
		wrap = 1,
	};
}

ACL_IMPL_FILE_PRAGMA_POP
