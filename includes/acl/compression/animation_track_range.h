#pragma once

#include "acl/math/vector4_64.h"

namespace acl
{
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

		Vector4_64 get_center() const { return vector_mul(vector_add(m_max, m_min), vector_set(0.5)); }
		Vector4_64 get_extent() const { return vector_mul(vector_sub(m_max, m_min), vector_set(0.5)); }

		bool is_constant(double threshold) const { return vector_all_less_than(vector_abs(vector_sub(m_max, m_min)), vector_set(threshold)); }

	private:
		Vector4_64	m_min;
		Vector4_64	m_max;
	};
}
