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
#include "acl/math/scalar_64.h"

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Setters, getters, and casts

	inline Vector4_64 vector_set(double x, double y, double z, double w)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_set_pd(y, x), _mm_set_pd(w, z) };
#else
		return Vector4_64{ x, y, z, w };
#endif
	}

	inline Vector4_64 vector_set(double x, double y, double z)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_set_pd(y, x), _mm_set_pd(0.0, z) };
#else
		return Vector4_64{ x, y, z, 0.0 };
#endif
	}

	inline Vector4_64 vector_set(double xyzw)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xyzw_pd = _mm_set1_pd(xyzw);
		return Vector4_64{ xyzw_pd, xyzw_pd };
#else
		return Vector4_64{ xyzw, xyzw, xyzw, xyzw };
#endif
	}

	inline Vector4_64 vector_unaligned_load(const double* input)
	{
		ACL_ENSURE(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], input[3]);
	}

	inline Vector4_64 vector_unaligned_load3(const double* input)
	{
		ACL_ENSURE(is_aligned(input), "Invalid alignment");
		return vector_set(input[0], input[1], input[2], 0.0);
	}

	inline Vector4_64 vector_unaligned_load_64(const uint8_t* input)
	{
		Vector4_64 result;
		memcpy(&result, input, sizeof(Vector4_64));
		return result;
	}

	inline Vector4_64 vector_unaligned_load3_64(const uint8_t* input)
	{
		double input_f[3];
		memcpy(&input_f[0], input, sizeof(double) * 3);
		return vector_set(input_f[0], input_f[1], input_f[2], 0.0);
	}

	inline Vector4_64 vector_zero_64()
	{
		return vector_set(0.0, 0.0, 0.0, 0.0);
	}

	inline Vector4_64 quat_to_vector(const Quat_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ input.xy, input.zw };
#else
		return Vector4_64{ input.x, input.y, input.z, input.w };
#endif
	}

	inline Vector4_64 vector_cast(const Vector4_32& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_cvtps_pd(input), _mm_cvtps_pd(_mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 2, 3, 2))) };
#else
		return Vector4_64{ double(input.x), double(input.y), double(input.z), double(input.w) };
#endif
	}

	inline double vector_get_x(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.xy);
#else
		return input.x;
#endif
	}

	inline double vector_get_y(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.xy, input.xy, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.y;
#endif
	}

	inline double vector_get_z(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(input.zw);
#else
		return input.z;
#endif
	}

	inline double vector_get_w(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return _mm_cvtsd_f64(_mm_shuffle_pd(input.zw, input.zw, _MM_SHUFFLE(1, 1, 1, 1)));
#else
		return input.w;
#endif
	}

	template<VectorMix component_index>
	inline double vector_get_component(const Vector4_64& input)
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
			return 0.0;
		}
	}

	inline double vector_get_component(const Vector4_64& input, VectorMix component_index)
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
			return 0.0;
		}
	}

	inline const double* vector_as_double_ptr(const Vector4_64& input)
	{
		return reinterpret_cast<const double*>(&input);
	}

	inline void vector_unaligned_write(const Vector4_64& input, double* output)
	{
		ACL_ENSURE(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
		output[3] = vector_get_w(input);
	}

	inline void vector_unaligned_write3(const Vector4_64& input, double* output)
	{
		ACL_ENSURE(is_aligned(output), "Invalid alignment");
		output[0] = vector_get_x(input);
		output[1] = vector_get_y(input);
		output[2] = vector_get_z(input);
	}

	inline void vector_unaligned_write(const Vector4_64& input, uint8_t* output)
	{
		memcpy(output, &input, sizeof(Vector4_64));
	}

	inline void vector_unaligned_write3(const Vector4_64& input, uint8_t* output)
	{
		memcpy(output, &input, sizeof(double) * 3);
	}

	//////////////////////////////////////////////////////////////////////////
	// Arithmetic

	inline Vector4_64 vector_add(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_add_pd(lhs.xy, rhs.xy), _mm_add_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
#endif
	}

	inline Vector4_64 vector_sub(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_sub_pd(lhs.xy, rhs.xy), _mm_sub_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
#endif
	}

	inline Vector4_64 vector_mul(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_mul_pd(lhs.xy, rhs.xy), _mm_mul_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
#endif
	}

	inline Vector4_64 vector_mul(const Vector4_64& lhs, double rhs)
	{
		return vector_mul(lhs, vector_set(rhs));
	}

	inline Vector4_64 vector_div(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_div_pd(lhs.xy, rhs.xy), _mm_div_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w);
#endif
	}

	inline Vector4_64 vector_max(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_max_pd(lhs.xy, rhs.xy), _mm_max_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w));
#endif
	}

	inline Vector4_64 vector_min(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		return Vector4_64{ _mm_min_pd(lhs.xy, rhs.xy), _mm_min_pd(lhs.zw, rhs.zw) };
#else
		return vector_set(min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w));
#endif
	}

	inline Vector4_64 vector_abs(const Vector4_64& input)
	{
#if defined(ACL_SSE2_INTRINSICS)
		Vector4_64 zero{ _mm_setzero_pd(), _mm_setzero_pd() };
		return vector_max(vector_sub(zero, input), input);
#else
		return vector_set(abs(input.x), abs(input.y), abs(input.z), abs(input.w));
#endif
	}

	inline Vector4_64 vector_neg(const Vector4_64& input)
	{
		return vector_mul(input, -1.0);
	}

	inline Vector4_64 vector_reciprocal(const Vector4_64& input)
	{
		return vector_div(vector_set(1.0), input);
	}

	inline Vector4_64 vector_cross3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
		return vector_set(vector_get_y(lhs) * vector_get_z(rhs) - vector_get_z(lhs) * vector_get_y(rhs),
						  vector_get_z(lhs) * vector_get_x(rhs) - vector_get_x(lhs) * vector_get_z(rhs),
						  vector_get_x(lhs) * vector_get_y(rhs) - vector_get_y(lhs) * vector_get_x(rhs));
	}

	inline double vector_dot(const Vector4_64& lhs, const Vector4_64& rhs)
	{
		// TODO: Use dot instruction
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs)) + (vector_get_w(lhs) * vector_get_w(rhs));
	}

	inline double vector_dot3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
		// TODO: Use dot instruction
		return (vector_get_x(lhs) * vector_get_x(rhs)) + (vector_get_y(lhs) * vector_get_y(rhs)) + (vector_get_z(lhs) * vector_get_z(rhs));
	}

	inline double vector_length_squared(const Vector4_64& input)
	{
		return vector_dot(input, input);
	}

	inline double vector_length_squared3(const Vector4_64& input)
	{
		return vector_dot3(input, input);
	}

	inline double vector_length(const Vector4_64& input)
	{
		return sqrt(vector_length_squared(input));
	}

	inline double vector_length3(const Vector4_64& input)
	{
		return sqrt(vector_length_squared3(input));
	}

	inline double vector_length_reciprocal(const Vector4_64& input)
	{
		return 1.0 / vector_length(input);
	}

	inline double vector_length_reciprocal3(const Vector4_64& input)
	{
		return 1.0 / vector_length3(input);
	}

	inline double vector_distance3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
		return vector_length3(vector_sub(rhs, lhs));
	}

	inline Vector4_64 vector_normalize3(const Vector4_64& input, double threshold = 0.00000001)
	{
		// Reciprocal is more accurate to normalize with
		double inv_len = vector_length_reciprocal3(input);
		if (inv_len >= threshold)
			return vector_mul(input, vector_set(inv_len));
		else
			return input;
	}

	inline Vector4_64 vector_lerp(const Vector4_64& start, const Vector4_64& end, double alpha)
	{
		return vector_add(start, vector_mul(vector_sub(end, start), vector_set(alpha)));
	}

	inline Vector4_64 vector_fraction(const Vector4_64& input)
	{
		return vector_set(fraction(vector_get_x(input)), fraction(vector_get_y(input)), fraction(vector_get_z(input)), fraction(vector_get_w(input)));
	}

	// output = (input * scale) + offset
	inline Vector4_64 vector_mul_add(const Vector4_64& input, const Vector4_64& scale, const Vector4_64& offset)
	{
		return vector_add(vector_mul(input, scale), offset);
	}

	// output = offset - (input * scale)
	inline Vector4_64 vector_neg_mul_sub(const Vector4_64& input, const Vector4_64& scale, const Vector4_64& offset)
	{
		return vector_sub(offset, vector_mul(input, scale));
	}

	//////////////////////////////////////////////////////////////////////////
	// Comparisons and masking

	inline Vector4_64 vector_less_than(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_lt_pd = _mm_cmplt_pd(lhs.xy, rhs.xy);
		__m128d zw_lt_pd = _mm_cmplt_pd(lhs.zw, rhs.zw);
		return Vector4_64{xy_lt_pd, zw_lt_pd};
#else
		return Vector4_64{math_impl::get_mask_value(lhs.x < rhs.x), math_impl::get_mask_value(lhs.y < rhs.y), math_impl::get_mask_value(lhs.z < rhs.z), math_impl::get_mask_value(lhs.w < rhs.w)};
#endif
	}

	inline Vector4_64 vector_greater_equal(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_ge_pd = _mm_cmpge_pd(lhs.xy, rhs.xy);
		__m128d zw_ge_pd = _mm_cmpge_pd(lhs.zw, rhs.zw);
		return Vector4_64{ xy_ge_pd, zw_ge_pd };
#else
		return Vector4_32{ math_impl::get_mask_value(lhs.x >= rhs.x), math_impl::get_mask_value(lhs.y >= rhs.y), math_impl::get_mask_value(lhs.z >= rhs.z), math_impl::get_mask_value(lhs.w >= rhs.w) };
#endif
	}

	inline bool vector_all_less_than(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_lt_pd = _mm_cmplt_pd(lhs.xy, rhs.xy);
		__m128d zw_lt_pd = _mm_cmplt_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_lt_pd) & _mm_movemask_pd(zw_lt_pd)) == 3;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z && lhs.w < rhs.w;
#endif
	}

	inline bool vector_all_less_than3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_lt_pd = _mm_cmplt_pd(lhs.xy, rhs.xy);
		__m128d zw_lt_pd = _mm_cmplt_pd(lhs.zw, rhs.zw);
		return _mm_movemask_pd(xy_lt_pd) == 3 && (_mm_movemask_pd(zw_lt_pd) & 1) == 1;
#else
		return lhs.x < rhs.x && lhs.y < rhs.y && lhs.z < rhs.z;
#endif
	}

	inline bool vector_any_less_than(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_lt_pd = _mm_cmplt_pd(lhs.xy, rhs.xy);
		__m128d zw_lt_pd = _mm_cmplt_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_lt_pd) | _mm_movemask_pd(zw_lt_pd)) != 0;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z || lhs.w < rhs.w;
#endif
	}

	inline bool vector_any_less_than3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_lt_pd = _mm_cmplt_pd(lhs.xy, rhs.xy);
		__m128d zw_lt_pd = _mm_cmplt_pd(lhs.zw, rhs.zw);
		return _mm_movemask_pd(xy_lt_pd) != 0 || (_mm_movemask_pd(zw_lt_pd) & 0x1) != 0;
#else
		return lhs.x < rhs.x || lhs.y < rhs.y || lhs.z < rhs.z;
#endif
	}

	inline bool vector_all_less_equal(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_le_pd = _mm_cmple_pd(lhs.xy, rhs.xy);
		__m128d zw_le_pd = _mm_cmple_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_le_pd) & _mm_movemask_pd(zw_le_pd)) == 3;
#else
		return lhs.x <= rhs.x && lhs.y <= rhs.y && lhs.z <= rhs.z && lhs.w <= rhs.w;
#endif
	}

	inline bool vector_all_less_equal3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_le_pd = _mm_cmple_pd(lhs.xy, rhs.xy);
		__m128d zw_le_pd = _mm_cmple_pd(lhs.zw, rhs.zw);
		return _mm_movemask_pd(xy_le_pd) == 3 && (_mm_movemask_pd(zw_le_pd) & 1) != 0;
#else
		return lhs.x <= rhs.x && lhs.y <= rhs.y && lhs.z <= rhs.z;
#endif
	}

	inline bool vector_any_less_equal(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_le_pd = _mm_cmple_pd(lhs.xy, rhs.xy);
		__m128d zw_le_pd = _mm_cmple_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_le_pd) | _mm_movemask_pd(zw_le_pd)) != 0;
#else
		return lhs.x <= rhs.x || lhs.y <= rhs.y || lhs.z <= rhs.z || lhs.w <= rhs.w;
#endif
	}

	inline bool vector_any_less_equal3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_le_pd = _mm_cmple_pd(lhs.xy, rhs.xy);
		__m128d zw_le_pd = _mm_cmple_pd(lhs.zw, rhs.zw);
		return _mm_movemask_pd(xy_le_pd) != 0 || (_mm_movemask_pd(zw_le_pd) & 1) != 0;
#else
		return lhs.x <= rhs.x || lhs.y <= rhs.y || lhs.z <= rhs.z;
#endif
	}

	inline bool vector_all_greater_equal(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_ge_pd = _mm_cmpge_pd(lhs.xy, rhs.xy);
		__m128d zw_ge_pd = _mm_cmpge_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_ge_pd) & _mm_movemask_pd(zw_ge_pd)) == 3;
#else
		return lhs.x >= rhs.x && lhs.y >= rhs.y && lhs.z >= rhs.z && lhs.w >= rhs.w;
#endif
	}

	inline bool vector_all_greater_equal3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_ge_pd = _mm_cmpge_pd(lhs.xy, rhs.xy);
		__m128d zw_ge_pd = _mm_cmpge_pd(lhs.zw, rhs.zw);
		return _mm_movemask_pd(xy_ge_pd) == 3 && (_mm_movemask_pd(zw_ge_pd) & 1) != 0;
#else
		return lhs.x >= rhs.x && lhs.y >= rhs.y && lhs.z >= rhs.z;
#endif
	}

	inline bool vector_any_greater_equal(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_ge_pd = _mm_cmpge_pd(lhs.xy, rhs.xy);
		__m128d zw_ge_pd = _mm_cmpge_pd(lhs.zw, rhs.zw);
		return (_mm_movemask_pd(xy_ge_pd) | _mm_movemask_pd(zw_ge_pd)) != 0;
#else
		return lhs.x >= rhs.x || lhs.y >= rhs.y || lhs.z >= rhs.z || lhs.w >= rhs.w;
#endif
	}

	inline bool vector_any_greater_equal3(const Vector4_64& lhs, const Vector4_64& rhs)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy_ge_pd = _mm_cmpge_pd(lhs.xy, rhs.xy);
		__m128d zw_ge_pd = _mm_cmpge_pd(lhs.zw, rhs.zw);
		return _mm_movemask_pd(xy_ge_pd) != 0 || (_mm_movemask_pd(zw_ge_pd) & 1) != 0;
#else
		return lhs.x >= rhs.x || lhs.y >= rhs.y || lhs.z >= rhs.z;
#endif
	}

	inline bool vector_all_near_equal(const Vector4_64& lhs, const Vector4_64& rhs, double threshold = 0.00001)
	{
		return vector_all_less_equal(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_all_near_equal3(const Vector4_64& lhs, const Vector4_64& rhs, double threshold = 0.00001)
	{
		return vector_all_less_equal3(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_any_near_equal(const Vector4_64& lhs, const Vector4_64& rhs, double threshold = 0.00001)
	{
		return vector_any_less_equal(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_any_near_equal3(const Vector4_64& lhs, const Vector4_64& rhs, double threshold = 0.00001)
	{
		return vector_any_less_equal3(vector_abs(vector_sub(lhs, rhs)), vector_set(threshold));
	}

	inline bool vector_is_finite(const Vector4_64& input)
	{
		return is_finite(vector_get_x(input)) && is_finite(vector_get_y(input)) && is_finite(vector_get_z(input)) && is_finite(vector_get_w(input));
	}

	inline bool vector_is_finite3(const Vector4_64& input)
	{
		return is_finite(vector_get_x(input)) && is_finite(vector_get_y(input)) && is_finite(vector_get_z(input));
	}

	//////////////////////////////////////////////////////////////////////////
	// Swizzling, permutations, and mixing

	inline Vector4_64 vector_blend(const Vector4_64& mask, const Vector4_64& if_true, const Vector4_64& if_false)
	{
#if defined(ACL_SSE2_INTRINSICS)
		__m128d xy = _mm_or_pd(_mm_andnot_pd(mask.xy, if_false.xy), _mm_and_pd(if_true.xy, mask.xy));
		__m128d zw = _mm_or_pd(_mm_andnot_pd(mask.zw, if_false.zw), _mm_and_pd(if_true.zw, mask.zw));
		return Vector4_64{ xy, zw };
#else
		return Vector4_64{ math_impl::select(mask.x, if_true.x, if_false.x), math_impl::select(mask.y, if_true.y, if_false.y), math_impl::select(mask.z, if_true.z, if_false.z), math_impl::select(mask.w, if_true.w, if_false.w) };
#endif
	}

	template<VectorMix comp0, VectorMix comp1, VectorMix comp2, VectorMix comp3>
	inline Vector4_64 vector_mix(const Vector4_64& input0, const Vector4_64& input1)
	{
		if (math_impl::is_vector_mix_arg_xyzw(comp0) && math_impl::is_vector_mix_arg_xyzw(comp1) && math_impl::is_vector_mix_arg_xyzw(comp2) && math_impl::is_vector_mix_arg_xyzw(comp3))
		{
			// All four components come from input 0
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input0, comp1), vector_get_component(input0, comp2), vector_get_component(input0, comp3));
		}

		if (math_impl::is_vector_mix_arg_abcd(comp0) && math_impl::is_vector_mix_arg_abcd(comp1) && math_impl::is_vector_mix_arg_abcd(comp2) && math_impl::is_vector_mix_arg_abcd(comp3))
		{
			// All four components come from input 1
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input1, comp1), vector_get_component(input1, comp2), vector_get_component(input1, comp3));
		}

		if ((comp0 == VectorMix::X || comp0 == VectorMix::Y) && (comp1 == VectorMix::X || comp1 == VectorMix::Y) && (comp2 == VectorMix::A || comp2 == VectorMix::B) && (comp3 == VectorMix::A && comp3 == VectorMix::B))
		{
			// First two components come from input 0, second two come from input 1
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input0, comp1), vector_get_component(input1, comp2), vector_get_component(input1, comp3));
		}

		if ((comp0 == VectorMix::A || comp0 == VectorMix::B) && (comp1 == VectorMix::A && comp1 == VectorMix::B) && (comp2 == VectorMix::X || comp2 == VectorMix::Y) && (comp3 == VectorMix::X || comp3 == VectorMix::Y))
		{
			// First two components come from input 1, second two come from input 0
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input1, comp1), vector_get_component(input0, comp2), vector_get_component(input0, comp3));
		}

		if (comp0 == VectorMix::X && comp1 == VectorMix::A && comp2 == VectorMix::Y && comp3 == VectorMix::B)
		{
			// Low words from both inputs are interleaved
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input1, comp1), vector_get_component(input0, comp2), vector_get_component(input1, comp3));
		}

		if (comp0 == VectorMix::A && comp1 == VectorMix::X && comp2 == VectorMix::B && comp3 == VectorMix::Y)
		{
			// Low words from both inputs are interleaved
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input0, comp1), vector_get_component(input1, comp2), vector_get_component(input0, comp3));
		}

		if (comp0 == VectorMix::Z && comp1 == VectorMix::C && comp2 == VectorMix::W && comp3 == VectorMix::D)
		{
			// High words from both inputs are interleaved
			return vector_set(vector_get_component(input0, comp0), vector_get_component(input1, comp1), vector_get_component(input0, comp2), vector_get_component(input1, comp3));
		}

		if (comp0 == VectorMix::C && comp1 == VectorMix::Z && comp2 == VectorMix::D && comp3 == VectorMix::W)
		{
			// High words from both inputs are interleaved
			return vector_set(vector_get_component(input1, comp0), vector_get_component(input0, comp1), vector_get_component(input1, comp2), vector_get_component(input0, comp3));
		}

		// Slow code path, not yet optimized
		//ACL_ENSURE(false, "vector_mix permutation not handled");
		const double x = math_impl::is_vector_mix_arg_xyzw(comp0) ? vector_get_component<comp0>(input0) : vector_get_component<comp0>(input1);
		const double y = math_impl::is_vector_mix_arg_xyzw(comp1) ? vector_get_component<comp1>(input0) : vector_get_component<comp1>(input1);
		const double z = math_impl::is_vector_mix_arg_xyzw(comp2) ? vector_get_component<comp2>(input0) : vector_get_component<comp2>(input1);
		const double w = math_impl::is_vector_mix_arg_xyzw(comp3) ? vector_get_component<comp3>(input0) : vector_get_component<comp3>(input1);
		return vector_set(x, y, z, w);
	}

	inline Vector4_64 vector_mix_xxxx(const Vector4_64& input) { return vector_mix<VectorMix::X, VectorMix::X, VectorMix::X, VectorMix::X>(input, input); }
	inline Vector4_64 vector_mix_yyyy(const Vector4_64& input) { return vector_mix<VectorMix::Y, VectorMix::Y, VectorMix::Y, VectorMix::Y>(input, input); }
	inline Vector4_64 vector_mix_zzzz(const Vector4_64& input) { return vector_mix<VectorMix::Z, VectorMix::Z, VectorMix::Z, VectorMix::Z>(input, input); }
	inline Vector4_64 vector_mix_wwww(const Vector4_64& input) { return vector_mix<VectorMix::W, VectorMix::W, VectorMix::W, VectorMix::W>(input, input); }

	inline Vector4_64 vector_mix_xxyy(const Vector4_64& input) { return vector_mix<VectorMix::X, VectorMix::X, VectorMix::Y, VectorMix::Y>(input, input); }
	inline Vector4_64 vector_mix_xzyw(const Vector4_64& input) { return vector_mix<VectorMix::X, VectorMix::Z, VectorMix::Y, VectorMix::W>(input, input); }
	inline Vector4_64 vector_mix_yzxy(const Vector4_64& input) { return vector_mix<VectorMix::Y, VectorMix::Z, VectorMix::X, VectorMix::Y>(input, input); }
	inline Vector4_64 vector_mix_ywxz(const Vector4_64& input) { return vector_mix<VectorMix::Y, VectorMix::W, VectorMix::X, VectorMix::Z>(input, input); }
	inline Vector4_64 vector_mix_zxyx(const Vector4_64& input) { return vector_mix<VectorMix::Z, VectorMix::X, VectorMix::Y, VectorMix::X>(input, input); }
	inline Vector4_64 vector_mix_zwyz(const Vector4_64& input) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::Y, VectorMix::Z>(input, input); }
	inline Vector4_64 vector_mix_zwzw(const Vector4_64& input) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::Z, VectorMix::W>(input, input); }
	inline Vector4_64 vector_mix_wxwx(const Vector4_64& input) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::W, VectorMix::X>(input, input); }
	inline Vector4_64 vector_mix_wzwy(const Vector4_64& input) { return vector_mix<VectorMix::W, VectorMix::Z, VectorMix::W, VectorMix::Y>(input, input); }

	inline Vector4_64 vector_mix_xyab(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::B>(input0, input1); }
	inline Vector4_64 vector_mix_xzac(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::X, VectorMix::Z, VectorMix::A, VectorMix::C>(input0, input1); }
	inline Vector4_64 vector_mix_xbxb(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::X, VectorMix::B, VectorMix::X, VectorMix::B>(input0, input1); }
	inline Vector4_64 vector_mix_xbzd(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::X, VectorMix::B, VectorMix::Z, VectorMix::D>(input0, input1); }
	inline Vector4_64 vector_mix_ywbd(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::Y, VectorMix::W, VectorMix::B, VectorMix::D>(input0, input1); }
	inline Vector4_64 vector_mix_zyax(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::Z, VectorMix::Y, VectorMix::A, VectorMix::X>(input0, input1); }
	inline Vector4_64 vector_mix_zycx(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::Z, VectorMix::Y, VectorMix::C, VectorMix::X>(input0, input1); }
	inline Vector4_64 vector_mix_zwcd(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::Z, VectorMix::W, VectorMix::C, VectorMix::D>(input0, input1); }
	inline Vector4_64 vector_mix_zbaz(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::Z, VectorMix::B, VectorMix::A, VectorMix::Z>(input0, input1); }
	inline Vector4_64 vector_mix_zdcz(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::Z, VectorMix::D, VectorMix::C, VectorMix::Z>(input0, input1); }
	inline Vector4_64 vector_mix_wxya(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::Y, VectorMix::A>(input0, input1); }
	inline Vector4_64 vector_mix_wxyc(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::W, VectorMix::X, VectorMix::Y, VectorMix::C>(input0, input1); }
	inline Vector4_64 vector_mix_wbyz(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::W, VectorMix::B, VectorMix::Y, VectorMix::Z>(input0, input1); }
	inline Vector4_64 vector_mix_wdyz(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::W, VectorMix::D, VectorMix::Y, VectorMix::Z>(input0, input1); }
	inline Vector4_64 vector_mix_bxwa(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::B, VectorMix::X, VectorMix::W, VectorMix::A>(input0, input1); }
	inline Vector4_64 vector_mix_bywx(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::B, VectorMix::Y, VectorMix::W, VectorMix::X>(input0, input1); }
	inline Vector4_64 vector_mix_dxwc(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::D, VectorMix::X, VectorMix::W, VectorMix::C>(input0, input1); }
	inline Vector4_64 vector_mix_dywx(const Vector4_64& input0, const Vector4_64& input1) { return vector_mix<VectorMix::D, VectorMix::Y, VectorMix::W, VectorMix::X>(input0, input1); }

	//////////////////////////////////////////////////////////////////////////
	// Misc

	inline Vector4_64 vector_sign(const Vector4_64& input)
	{
		Vector4_64 mask = vector_greater_equal(input, vector_zero_64());
		return vector_blend(mask, vector_set(1.0), vector_set(-1.0));
	}
}
