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

#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/math/math.h"
#include "acl/math/scalar_32.h"

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Setters, getters, and casts

	inline Vector4_32 vector_set(float x, float y, float z, float w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps(w, z, y, x));
#else
		return Vector4_32{ x, y, z, w };
#endif
	}

	inline Vector4_32 vector_set(float x, float y, float z)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps(0.0f, z, y, x));
#else
		return Vector4_32{ x, y, z, 0.0f };
#endif
	}

	inline Vector4_32 vector_set(float xyzw)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_32(_mm_set_ps1(xyzw));
#else
		return Vector4_32{ xyzw, xyzw, xyzw, xyzw };
#endif
	}

	inline Vector4_32 vector_unaligned_load(const float* input)
	{
		ACL_ENSURE(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], input[3]);
	}

	inline Vector4_32 vector_unaligned_load3(const float* input)
	{
		ACL_ENSURE(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], 0.0f);
	}

	inline Vector4_32 vector_unaligned_load_32(const uint8_t* input)
	{
		Vector4_32 result;
		memcpy(&result, input, sizeof(Vector4_32));
		return result;
	}

	inline Vector4_32 vector_unaligned_load3_32(const uint8_t* input)
	{
		float input_f[3];
		memcpy(&input_f[0], input, sizeof(float) * 3);
		return vector_set(input_f[0], input_f[1], input_f[2], 0.0f);
	}

	inline Vector4_32 vector_zero_32()
	{
		return vector_set(0.0f, 0.0f, 0.0f, 0.0f);
	}

	inline Vector4_32 quat_to_vector(const Quat_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return input;
#else
		return Vector4_32{ input.x, input.y, input.z, input.w };
#endif
	}

	inline Vector4_32 vector_cast(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_shuffle_ps(_mm_cvtpd_ps(input.xy), _mm_cvtpd_ps(input.zw), _MM_SHUFFLE(1, 0, 1, 0));
#else
		return Vector4_32{ float(input.x), float(input.y), float(input.z), float(input.w) };
#endif
	}

	inline float vector_get_x(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(input);
#else
		return input.x;
#endif
	}

	inline float vector_get_y(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline float vector_get_z(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2)));
#else
		return input.z;
#endif
	}

	inline float vector_get_w(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtss_f32(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3)));
#else
		return input.w;
#endif
	}

	template<VectorMix component_index>
	inline float vector_get_component(const Vector4_32& input)
	{
		switch (component_index)
		{
		case VectorMix::A:
		case VectorMix::X: return vector_get_x(input);
		case VectorMix::B:
		case VectorMix::Y: return vector_get_y(input);
		case VectorMix::C:
		case VectorMix::Z: return vector_get_z(input);
		case VectorMix::D:
		case VectorMix::W: return vector_get_w(input);
		default:
			ACL_ENSURE(false, "Invalid component index");
			return 0.0f;
		}
	}

	inline float vector_get_component(const Vector4_32& input, VectorMix component_index)
	{
		switch (component_index)
		{
		case VectorMix::A:
		case VectorMix::X: return vector_get_x(input);
		case VectorMix::B:
		case VectorMix::Y: return vector_get_y(input);
		case VectorMix::C:
		case VectorMix::Z: return vector_get_z(input);
		case VectorMix::D:
		case VectorMix::W: return vector_get_w(input);
		default:
			ACL_ENSURE(false, "Invalid component index");
			return 0.0f;
		}
	}

	inline const float* vector_as_float_ptr(const Vector4_32& input)
	{
		return reinterpret_cast<const float*>(&input);
	}

	inline void vector_unaligned_write(const Vector4_32& input, float* output)
	{
		ACL_ENSURE(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
		output[3] = vector_get_w(input);
	}

	inline void vector_unaligned_write3(const Vector4_32& input, float* output)
	{
		ACL_ENSURE(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
	}

	inline void vector_unaligned_write(const Vector4_32& input, uint8_t* output)
	{
		memcpy(output, &input, sizeof(Vector4_32));
	}

	inline void vector_unaligned_write3(const Vector4_32& input, uint8_t* output)
	{
		memcpy(output, &input, sizeof(float) * 3);
	}

	//////////////////////////////////////////////////////////////////////////
	// Arithmetic

	inline Vector4_32 vector_add(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_add_ps(lhs, rhs);
#else
		return vector_set(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
#endif
	}

	inline Vector4_32 vector_sub(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_sub_ps(lhs, rhs);
#else
		return vector_set(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
#endif
	}

	inline Vector4_32 vector_mul(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_mul_ps(lhs, rhs);
#else
		return vector_set(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
#endif
	}

	inline Vector4_32 vector_mul(const Vector4_32& lhs, float rhs)
	{
		return vector_mul(lhs, vector_set(rhs));
	}

	inline Vector4_32 vector_div(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_div_ps(lhs, rhs);
#else
		return vector_set(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w);
#endif
	}

	inline Vector4_32 vector_max(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_max_ps(lhs, rhs);
#else
		return vector_set(max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w));
#endif
	}

	inline Vector4_32 vector_min(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_min_ps(lhs, rhs);
#else
		return vector_set(min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w));
#endif
	}

	inline Vector4_32 vector_abs(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return vector_max(vector_sub(_mm_setzero_ps(), input), input);
#else
		return vector_set(abs(input.x), abs(input.y), abs(input.z), abs(input.w));
#endif
	}

	inline Vector4_32 vector_neg(const Vector4_32& input)
	{
		return vector_mul(input, -1.0f);
	}

	inline Vector4_32 vector_reciprocal(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		// Perform two passes of Newton-Raphson iteration on the hardware estimate
		__m128 x0 = _mm_rcp_ps(input);

		// First iteration
		__m128 x1 = _mm_sub_ps(_mm_add_ps(x0, x0), _mm_mul_ps(input, _mm_mul_ps(x0, x0)));

		// Second iteration
		__m128 x2 = _mm_sub_ps(_mm_add_ps(x1, x1), _mm_mul_ps(input, _mm_mul_ps(x1, x1)));

		return x2;
#else
		return vector_div(vector_set(1.0f), input);
#endif
	}

	inline Vector4_32 vector_cross3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
		return vector_set(vector_get_y(lhs) * vector_get_z(rhs) - vector_get_z(lhs) * vector_get_y(rhs),
						  vector_get_z(lhs) * vector_get_x(rhs) - vector_get_x(lhs) * vector_get_z(rhs),
						  vector_get_x(lhs) * vector_get_y(rhs) - vector_get_y(lhs) * vector_get_x(rhs));
	}

	inline float vector_dot(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE4_INTRINSICS) && 0
		// SSE4 dot product instruction isn't precise enough
		return _mm_cvtss_f32(_mm_dp_ps(lhs, rhs, 0xFF));
#elif defined(ACL_SSE2_INTRINSICS)
		__m128 x2_y2_z2_w2 = _mm_mul_ps(lhs, rhs);
		__m128 z2_w2_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 3, 2));
		__m128 x2z2_y2w2_0_0 = _mm_add_ps(x2_y2_z2_w2, z2_w2_0_0);
		__m128 y2w2_0_0_0 = _mm_shuffle_ps(x2z2_y2w2_0_0, x2z2_y2w2_0_0, _MM_SHUFFLE(0, 0, 0, 1));
		__m128 x2y2z2w2_0_0_0 = _mm_add_ps(x2z2_y2w2_0_0, y2w2_0_0_0);
		return _mm_cvtss_f32(x2y2z2w2_0_0_0);
#else
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs)) + (vector_get_w(lhs) * vector_get_w(rhs));
#endif
	}

	inline float vector_dot3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE4_INTRINSICS) && 0
		// SSE4 dot product instruction isn't precise enough
		return _mm_cvtss_f32(_mm_dp_ps(lhs, rhs, 0x7F));
#elif defined(ACL_SSE2_INTRINSICS)
		__m128 x2_y2_z2_w2 = _mm_mul_ps(lhs, rhs);
		__m128 y2_0_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 0, 1));
		__m128 x2y2_0_0_0 = _mm_add_ss(x2_y2_z2_w2, y2_0_0_0);
		__m128 z2_0_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 0, 2));
		__m128 x2y2z2_0_0_0 = _mm_add_ss(x2y2_0_0_0, z2_0_0_0);
		return _mm_cvtss_f32(x2y2z2_0_0_0);
#else
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs));
#endif
	}

	inline float vector_length_squared(const Vector4_32& input)
	{
		return vector_dot(input, input);
	}

	inline float vector_length_squared3(const Vector4_32& input)
	{
		return vector_dot3(input, input);
	}

	inline float vector_length(const Vector4_32& input)
	{
		return sqrt(vector_length_squared(input));
	}

	inline float vector_length3(const Vector4_32& input)
	{
		return sqrt(vector_length_squared3(input));
	}

	inline float vector_length_reciprocal(const Vector4_32& input)
	{
		return sqrt_reciprocal(vector_length_squared(input));
	}

	inline float vector_length_reciprocal3(const Vector4_32& input)
	{
		return sqrt_reciprocal(vector_length_squared3(input));
	}

	inline float vector_distance3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
		return vector_length3(vector_sub(rhs, lhs));
	}

	inline Vector4_32 vector_normalize3(const Vector4_32& input, float threshold = 0.00000001f)
	{
		// Reciprocal is more accurate to normalize with
		float inv_len = vector_length_reciprocal3(input);
		if (inv_len >= threshold)
			return vector_mul(input, vector_set(inv_len));
		else
			return input;
	}

	inline Vector4_32 vector_lerp(const Vector4_32& start, const Vector4_32& end, float alpha)
	{
		return vector_add(start, vector_mul(vector_sub(end, start), vector_set(alpha)));
	}

	inline Vector4_32 vector_fraction(const Vector4_32& input)
	{
		return vector_set(fraction(vector_get_x(input)), fraction(vector_get_y(input)), fraction(vector_get_z(input)), fraction(vector_get_w(input)));
	}

	// output = (input * scale) + offset
	inline Vector4_32 vector_mul_add(const Vector4_32& input, const Vector4_32& scale, const Vector4_32& offset)
	{
		return vector_add(vector_mul(input, scale), offset);
	}

	// output = offset - (input * scale)
	inline Vector4_32 vector_neg_mul_sub(const Vector4_32& input, const Vector4_32& scale, const Vector4_32& offset)
	{
		return vector_sub(offset, vector_mul(input, scale));
	}

	//////////////////////////////////////////////////////////////////////////
	// Comparisons and masking

	inline Vector4_32 vector_less_than(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cmplt_ps(lhs, rhs);
#else
		return Vector4_32{ math_impl::get_mask_value(lhs.x < rhs.x), math_impl::get_mask_value(lhs.y < rhs.y), math_impl::get_mask_value(lhs.z < rhs.z), math_impl::get_mask_value(lhs.w < rhs.w) };
#endif
	}

	inline Vector4_32 vector_greater_equal(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cmpge_ps(lhs, rhs);
#else
		return Vector4_32{ math_impl::get_mask_value(lhs.x >= rhs.x), math_impl::get_mask_value(lhs.y >= rhs.y), math_impl::get_mask_value(lhs.z >= rhs.z), math_impl::get_mask_value(lhs.w >= rhs.w) };
#endif
	}

	inline bool vector_all_less_than(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) == 0xF;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z && lhs.w < rhs.w;
#endif
	}

	inline bool vector_all_less_than3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) & 0x7) == 0x7;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z;
#endif
	}

	inline bool vector_any_less_than(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) != 0;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z || lhs.w < rhs.w;
#endif
	}

	inline bool vector_any_less_than3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) & 0x7) != 0;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z;
#endif
	}

	inline bool vector_all_less_equal(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) == 0xF;
#else
		return lhs.x <= rhs.x && lhs.y <= rhs.y && lhs.z <= rhs.z && lhs.w <= rhs.w;
#endif
	}

	inline bool vector_all_less_equal3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) & 0x7) == 0x7;
#else
		return lhs.x <= rhs.x && lhs.y <= rhs.y && lhs.z <= rhs.z;
#endif
	}

	inline bool vector_any_less_equal(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) != 0;
#else
		return lhs.x <= rhs.x || lhs.y <= rhs.y || lhs.z <= rhs.z || lhs.w <= rhs.w;
#endif
	}

	inline bool vector_any_less_equal3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmple_ps(lhs, rhs)) & 0x7) != 0;
#else
		return lhs.x <= rhs.x || lhs.y <= rhs.y || lhs.z <= rhs.z;
#endif
	}

	inline bool vector_all_greater_equal(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) == 0xF;
#else
		return lhs.x >= rhs.x && lhs.y >= rhs.y && lhs.z >= rhs.z && lhs.w >= rhs.w;
#endif
	}

	inline bool vector_all_greater_equal3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) & 0x7) == 0x7;
#else
		return lhs.x >= rhs.x && lhs.y >= rhs.y && lhs.z >= rhs.z;
#endif
	}

	inline bool vector_any_greater_equal(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) != 0;
#else
		return lhs.x >= rhs.x || lhs.y >= rhs.y || lhs.z >= rhs.z || lhs.w >= rhs.w;
#endif
	}

	inline bool vector_any_greater_equal3(const Vector4_32& lhs, const Vector4_32& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return (_mm_movemask_ps(_mm_cmpge_ps(lhs, rhs)) & 0x7) != 0;
#else
		return lhs.x >= rhs.x || lhs.y >= rhs.y || lhs.z >= rhs.z;
#endif
	}

	inline bool vector_all_near_equal(const Vector4_32& lhs, const Vector4_32& rhs, float threshold = 0.00001f)
	{
		return vector_all_less_equal(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_all_near_equal3(const Vector4_32& lhs, const Vector4_32& rhs, float threshold = 0.00001f)
	{
		return vector_all_less_equal3(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_any_near_equal(const Vector4_32& lhs, const Vector4_32& rhs, float threshold = 0.00001f)
	{
		return vector_any_less_equal(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_any_near_equal3(const Vector4_32& lhs, const Vector4_32& rhs, float threshold = 0.00001f)
	{
		return vector_any_less_equal3(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_is_finite(const Vector4_32& input)
	{
		return is_finite(vector_get_x(input)) && is_finite(vector_get_y(input)) && is_finite(vector_get_z(input)) && is_finite(vector_get_w(input));
	}

	inline bool vector_is_finite3(const Vector4_32& input)
	{
		return is_finite(vector_get_x(input)) && is_finite(vector_get_y(input)) && is_finite(vector_get_z(input));
	}

	//////////////////////////////////////////////////////////////////////////
	// Swizzling, permutations, and mixing

	inline Vector4_32 vector_blend(const Vector4_32& mask, const Vector4_32& if_true, const Vector4_32& if_false)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_or_ps(_mm_andnot_ps(mask, if_false), _mm_and_ps(if_true, mask));
#else
		return Vector4_32{ math_impl::select(mask.x, if_true.x, if_false.x), math_impl::select(mask.y, if_true.y, if_false.y), math_impl::select(mask.z, if_true.z, if_false.z), math_impl::select(mask.w, if_true.w, if_false.w) };
#endif
	}

	template<VectorMix comp0, VectorMix comp1, VectorMix comp2, VectorMix comp3>
	inline Vector4_32 vector_mix(const Vector4_32& input0, const Vector4_32& input1)
	{
		if (math_impl::is_vector_mix_arg_xyzw(comp0) && math_impl::is_vector_mix_arg_xyzw(comp1) && math_impl::is_vector_mix_arg_xyzw(comp2) && math_impl::is_vector_mix_arg_xyzw(comp3))
		{
			// All four components come from input 0
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_shuffle_ps(input0, input0, _MM_SHUFFLE(GET_VECTOR_MIX_COMPONENT_INDEX(comp3), GET_VECTOR_MIX_COMPONENT_INDEX(comp2), GET_VECTOR_MIX_COMPONENT_INDEX(comp1), GET_VECTOR_MIX_COMPONENT_INDEX(comp0)));
#else
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input0, comp1), vector_get_component(input0, comp2), vector_get_component(input0, comp3));
#endif
		}

		if (math_impl::is_vector_mix_arg_abcd(comp0) && math_impl::is_vector_mix_arg_abcd(comp1) && math_impl::is_vector_mix_arg_abcd(comp2) && math_impl::is_vector_mix_arg_abcd(comp3))
		{
			// All four components come from input 1
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_shuffle_ps(input1, input1, _MM_SHUFFLE(GET_VECTOR_MIX_COMPONENT_INDEX(comp3), GET_VECTOR_MIX_COMPONENT_INDEX(comp2), GET_VECTOR_MIX_COMPONENT_INDEX(comp1), GET_VECTOR_MIX_COMPONENT_INDEX(comp0)));
#else
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input1, comp1), vector_get_component(input1, comp2), vector_get_component(input1, comp3));
#endif
		}

		if ((comp0 == VectorMix::X || comp0 == VectorMix::Y) && (comp1 == VectorMix::X || comp1 == VectorMix::Y) && (comp2 == VectorMix::A || comp2 == VectorMix::B) && (comp3 == VectorMix::A && comp3 == VectorMix::B))
		{
			// First two components come from input 0, second two come from input 1
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_shuffle_ps(input0, input1, _MM_SHUFFLE(GET_VECTOR_MIX_COMPONENT_INDEX(comp3), GET_VECTOR_MIX_COMPONENT_INDEX(comp2), GET_VECTOR_MIX_COMPONENT_INDEX(comp1), GET_VECTOR_MIX_COMPONENT_INDEX(comp0)));
#else
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input0, comp1), vector_get_component(input1, comp2), vector_get_component(input1, comp3));
#endif
		}

		if ((comp0 == VectorMix::A || comp0 == VectorMix::B) && (comp1 == VectorMix::A && comp1 == VectorMix::B) && (comp2 == VectorMix::X || comp2 == VectorMix::Y) && (comp3 == VectorMix::X || comp3 == VectorMix::Y))
		{
			// First two components come from input 1, second two come from input 0
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_shuffle_ps(input1, input0, _MM_SHUFFLE(GET_VECTOR_MIX_COMPONENT_INDEX(comp3), GET_VECTOR_MIX_COMPONENT_INDEX(comp2), GET_VECTOR_MIX_COMPONENT_INDEX(comp1), GET_VECTOR_MIX_COMPONENT_INDEX(comp0)));
#else
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input1, comp1), vector_get_component(input0, comp2), vector_get_component(input0, comp3));
#endif
		}

		if (comp0 == VectorMix::X && comp1 == VectorMix::A && comp2 == VectorMix::Y && comp3 == VectorMix::B)
		{
			// Low words from both inputs are interleaved
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_unpacklo_ps(input0, input1);
#else
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input1, comp1), vector_get_component(input0, comp2), vector_get_component(input1, comp3));
#endif
		}

		if (comp0 == VectorMix::A && comp1 == VectorMix::X && comp2 == VectorMix::B && comp3 == VectorMix::Y)
		{
			// Low words from both inputs are interleaved
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_unpacklo_ps(input1, input0);
#else
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input0, comp1), vector_get_component(input1, comp2), vector_get_component(input0, comp3));
#endif
		}

		if (comp0 == VectorMix::Z && comp1 == VectorMix::C && comp2 == VectorMix::W && comp3 == VectorMix::D)
		{
			// High words from both inputs are interleaved
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_unpackhi_ps(input0, input1);
#else
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input1, comp1), vector_get_component(input0, comp2), vector_get_component(input1, comp3));
#endif
		}

		if (comp0 == VectorMix::C && comp1 == VectorMix::Z && comp2 == VectorMix::D && comp3 == VectorMix::W)
		{
			// High words from both inputs are interleaved
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_unpackhi_ps(input1, input0);
#else
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input0, comp1), vector_get_component(input1, comp2), vector_get_component(input0, comp3));
#endif
		}

		// Slow code path, not yet optimized
		//ACL_ENSURE(false, "vector_mix permutation not handled");
		const float x = math_impl::is_vector_mix_arg_xyzw(comp0) ? vector_get_component<comp0>(input0) : vector_get_component<comp0>(input1);
		const float y = math_impl::is_vector_mix_arg_xyzw(comp1) ? vector_get_component<comp1>(input0) : vector_get_component<comp1>(input1);
		const float z = math_impl::is_vector_mix_arg_xyzw(comp2) ? vector_get_component<comp2>(input0) : vector_get_component<comp2>(input1);
		const float w = math_impl::is_vector_mix_arg_xyzw(comp3) ? vector_get_component<comp3>(input0) : vector_get_component<comp3>(input1);
		return vector_set(x, y, z, w);
	}

	inline Vector4_32 vector_mix_xxxx(const Vector4_32& input) { return vector_mix<VectorMix::X, VectorMix::X, VectorMix::X, VectorMix::X>(input, input); }
	inline Vector4_32 vector_mix_yyyy(const Vector4_32& input) { return vector_mix<VectorMix::Y, VectorMix::Y, VectorMix::Y, VectorMix::Y>(input, input); }
	inline Vector4_32 vector_mix_zzzz(const Vector4_32& input) { return vector_mix<VectorMix::Z, VectorMix::Z, VectorMix::Z, VectorMix::Z>(input, input); }
	inline Vector4_32 vector_mix_wwww(const Vector4_32& input) { return vector_mix<VectorMix::W, VectorMix::W, VectorMix::W, VectorMix::W>(input, input); }

	inline Vector4_32 vector_mix_xxyy(const Vector4_32& input) { return vector_mix<VectorMix::X, VectorMix::X, VectorMix::Y, VectorMix::Y>(input, input); }
	inline Vector4_32 vector_mix_xzyw(const Vector4_32& input) { return vector_mix<VectorMix::X, VectorMix::Z, VectorMix::Y, VectorMix::W>(input, input); }
	inline Vector4_32 vector_mix_yzxy(const Vector4_32& input) { return vector_mix<VectorMix::Y, VectorMix::Z, VectorMix::X, VectorMix::Y>(input, input); }
	inline Vector4_32 vector_mix_ywxz(const Vector4_32& input) { return vector_mix<VectorMix::Y, VectorMix::W, VectorMix::X, VectorMix::Z>(input, input); }
	inline Vector4_32 vector_mix_zxyx(const Vector4_32& input) { return vector_mix<VectorMix::Z, VectorMix::X, VectorMix::Y, VectorMix::X>(input, input); }
	inline Vector4_32 vector_mix_zwyz(const Vector4_32& input) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::Y, VectorMix::Z>(input, input); }
	inline Vector4_32 vector_mix_zwzw(const Vector4_32& input) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::Z, VectorMix::W>(input, input); }
	inline Vector4_32 vector_mix_wxwx(const Vector4_32& input) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::W, VectorMix::X>(input, input); }
	inline Vector4_32 vector_mix_wzwy(const Vector4_32& input) { return vector_mix<VectorMix::W, VectorMix::Z, VectorMix::W, VectorMix::Y>(input, input); }

	inline Vector4_32 vector_mix_xyab(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::B>(input0, input1); }
	inline Vector4_32 vector_mix_xzac(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::X, VectorMix::Z, VectorMix::A, VectorMix::C>(input0, input1); }
	inline Vector4_32 vector_mix_xbxb(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::X, VectorMix::B, VectorMix::X, VectorMix::B>(input0, input1); }
	inline Vector4_32 vector_mix_xbzd(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::X, VectorMix::B, VectorMix::Z, VectorMix::D>(input0, input1); }
	inline Vector4_32 vector_mix_ywbd(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::Y, VectorMix::W, VectorMix::B, VectorMix::D>(input0, input1); }
	inline Vector4_32 vector_mix_zyax(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::Z, VectorMix::Y, VectorMix::A, VectorMix::X>(input0, input1); }
	inline Vector4_32 vector_mix_zycx(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::Z, VectorMix::Y, VectorMix::C, VectorMix::X>(input0, input1); }
	inline Vector4_32 vector_mix_zwcd(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::C, VectorMix::D>(input0, input1); }
	inline Vector4_32 vector_mix_zbaz(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::Z, VectorMix::B, VectorMix::A, VectorMix::Z>(input0, input1); }
	inline Vector4_32 vector_mix_zdcz(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::Z, VectorMix::D, VectorMix::C, VectorMix::Z>(input0, input1); }
	inline Vector4_32 vector_mix_wxya(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::Y, VectorMix::A>(input0, input1); }
	inline Vector4_32 vector_mix_wxyc(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::Y, VectorMix::C>(input0, input1); }
	inline Vector4_32 vector_mix_wbyz(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::W, VectorMix::B, VectorMix::Y, VectorMix::Z>(input0, input1); }
	inline Vector4_32 vector_mix_wdyz(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::W, VectorMix::D, VectorMix::Y, VectorMix::Z>(input0, input1); }
	inline Vector4_32 vector_mix_bxwa(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::B, VectorMix::X, VectorMix::W, VectorMix::A>(input0, input1); }
	inline Vector4_32 vector_mix_bywx(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::B, VectorMix::Y, VectorMix::W, VectorMix::X>(input0, input1); }
	inline Vector4_32 vector_mix_dxwc(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::D, VectorMix::X, VectorMix::W, VectorMix::C>(input0, input1); }
	inline Vector4_32 vector_mix_dywx(const Vector4_32& input0, const Vector4_32& input1) { return vector_mix<VectorMix::D, VectorMix::Y, VectorMix::W, VectorMix::X>(input0, input1); }

	//////////////////////////////////////////////////////////////////////////
	// Misc

	inline Vector4_32 vector_sign(const Vector4_32& input)
	{
		Vector4_32 mask = vector_greater_equal(input, vector_zero_32());
		return vector_blend(mask, vector_set(1.0f), vector_set(-1.0f));
	}
}
