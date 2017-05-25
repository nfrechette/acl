#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/math/vector4_64.h"

namespace acl
{
	// For a rotation track, the extent only tells us if the track is constant or not
	// since the min/max we maintain aren't valid rotations.
	// Similarly, the center isn't a valid rotation and is meaningless.
	class AnimationTrackRange
	{
	public:
		AnimationTrackRange()
			: m_min(vector_set(0.0))
			, m_max(vector_set(0.0))
		{}

		AnimationTrackRange(const Vector4_64& min, const Vector4_64& max)
			: m_min(min)
			, m_max(max)
		{}

		Vector4_64 get_min() const { return m_min; }
		Vector4_64 get_max() const { return m_max; }

		Vector4_64 get_center() const { return vector_mul(vector_add(m_max, m_min), 0.5); }
		Vector4_64 get_extent() const { return vector_mul(vector_sub(m_max, m_min), 0.5); }

		bool is_constant(double threshold) const { return vector_all_less_than(vector_abs(vector_sub(m_max, m_min)), vector_set(threshold)); }

	private:
		Vector4_64	m_min;
		Vector4_64	m_max;
	};
}
