#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/error.h"
#include "acl/core/hash.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/fwd.h"
#include "acl/compression/transform_adapters.h"
#include "acl/math/vector4f.h"

#include <rtm/quatd.h>
#include <rtm/scalard.h>
#include <rtm/scalarf.h>
#include <rtm/vector4d.h>
#include <rtm/vector4f.h>
#include <rtm/vqsf.h>
#include <rtm/vqvd.h>
#include <rtm/vqvf.h>

#include <cstdint>

// Whether or not to use a third point when the transforms have no scale (VQ)
//#define ACL_IMPL_METRIC_USE_3_POINTS_WITHOUT_SCALE

// Whether or not to use a fourth point to improve coverage
//#define ACL_IMPL_METRIC_USE_4_POINTS

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// Interface for all transform metrics.
	// A transform metric measures the error between a raw and a lossy transform.
	// It uses the transforms only through a transform adapter. Thus one metric
	// can measure the error of each runtime transform type that it supports.
	//
	// We measure the error on a rigid shell around each transform: a sphere at a
	// certain distance from the transform. See ./docs/error_metrics.md for details.
	//////////////////////////////////////////////////////////////////////////
	class itransform_metric
	{
	public:
		virtual ~itransform_metric() = default;

		//////////////////////////////////////////////////////////////////////////
		// Returns the unique name of the transform metric.
		virtual const char* get_name() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns a unique hash to represent the transform metric.
		virtual uint32_t get_hash() const { return hash32(get_name()); }

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not the metric can measure the error of transforms
		// that use the provided transform adapter.
		virtual bool is_supported(const itransform_adapter& adapter) const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Input arguments for the 'calculate_error*' functions.
		//////////////////////////////////////////////////////////////////////////
		struct calculate_error_args
		{
			//////////////////////////////////////////////////////////////////////////
			// The points on the rigid shell, stored transposed.
			// The X, Y, and Z lanes hold the points along the X, Y, and Z axes.
			// The W lane holds a point between the three axes.
			rtm::vector4f shell_points_xxxx;
			rtm::vector4f shell_points_yyyy;
			rtm::vector4f shell_points_zzzz;

			//////////////////////////////////////////////////////////////////////////
			// The distance from the transform to the rigid shell: the radius of the sphere.
			float shell_distance;

			//////////////////////////////////////////////////////////////////////////
			// The transform adapter of the two transforms.
			const itransform_adapter* adapter;

			//////////////////////////////////////////////////////////////////////////
			// The raw transform, in the runtime type of the adapter.
			// Could be in local or object space (same space as the lossy transform).
			const void* transform0;

			//////////////////////////////////////////////////////////////////////////
			// The lossy transform, in the runtime type of the adapter.
			// Could be in local or object space (same space as the raw transform).
			const void* transform1;

			//////////////////////////////////////////////////////////////////////////
			// Sets the shell points for a sphere with the provided radius.
			void construct_sphere_shell(float shell_distance_)
			{
				shell_distance = shell_distance_;

				// The fourth point is between the three others
				// Because they are axis aligned, it is at 45 degrees on the XY, XZ, and YZ planes
				// (1, 1, 1) / sqrt(3) = (0.57735027, 0.57735027, 0.57735027)
				const float mid_distance = 0.57735027F * shell_distance_;

				shell_points_xxxx = rtm::vector_set(shell_distance_, 0.0F, 0.0F, mid_distance);
				shell_points_yyyy = rtm::vector_set(0.0F, shell_distance_, 0.0F, mid_distance);
				shell_points_zzzz = rtm::vector_set(0.0F, 0.0F, shell_distance_, mid_distance);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// Measures the error between a raw and lossy transform.
		rtm::scalarf RTM_SIMD_CALL calculate_error(const calculate_error_args& args) const
		{
			return rtm::scalar_sqrt(calculate_error_squared(args));
		}

		//////////////////////////////////////////////////////////////////////////
		// Measures the squared error between a raw and lossy transform.
		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const = 0;
	};

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Returns the largest squared distance between two rigid transforms on a sphere.
		// The transforms have no scale. Their rotations must be normalized.
		// The result is exact: we find the point on the sphere with the largest error.
		inline double calculate_rigid_error_squared_precise(const rtm::vqvd& raw_transform, const rtm::vqvd& lossy_transform, double sphere_radius)
		{
			// We use the quaternion dot product to determine the cosine of the half angle between the raw
			// and lossy rotations. If the angle is very small, the absolute value of the dot product will
			// be very close to 1.0
			// Due to floating point precision, we use a threshold value to determine if we have any delta between the two
			const double raw_lossy_quat_dot = rtm::quat_dot(raw_transform.rotation, lossy_transform.rotation);
			const bool has_rotation_delta = rtm::scalar_abs(raw_lossy_quat_dot) < 0.99999999999999822;	// 0x3feffffffffffff0
			if (!has_rotation_delta)
			{
				// If we have no rotation delta, then the transform error is just the translation error: the delta length
				return rtm::scalar_cast(rtm::vector_distance_squared3_as_scalar(raw_transform.translation, lossy_transform.translation));
			}

			// We remove the raw transform from the lossy transform to form a delta transform between the two
			// We remove the raw transform instead of the lossy because this ensures that we always see consistent results
			// It would also allow us to compute the raw inverse ahead of time if we wanted to
			const rtm::vqvd delta_transform = rtm::vqv_normalize(rtm::vqv_mul_no_scale(lossy_transform, rtm::vqv_inverse_no_scale(raw_transform)));

			// We first calculate the contribution from the rotation delta
			const rtm::vector4d rotation_plane_normal = rtm::vector_normalize3(rtm::quat_to_vector(delta_transform.rotation));
			ACL_ASSERT(rtm::vector_is_finite3(rotation_plane_normal), "Delta rotation plane must exist");

			// Using the unit circle to compute the sine of our half rotation angle
			// 1^2 = cos(a)^2 + sin(a)^2
			const double half_sin_angle = rtm::scalar_sqrt(1.0 - (raw_lossy_quat_dot * raw_lossy_quat_dot));

			// We can then compute the rotation error delta by scaling it with the sphere radius
			const double max_delta_rotation_error = half_sin_angle * sphere_radius * 2.0;

			// We then calculate the full transform error by incorporating the translation contribution
			const rtm::vector4d delta_translation = delta_transform.translation;
			const double delta_translation_len_sq = rtm::vector_length_squared3(delta_translation);
			const double delta_translation_along_rotation_plane = rtm::vector_dot3(delta_translation, rotation_plane_normal);
			const double delta_translation_along_rotation_plane_sq = delta_translation_along_rotation_plane * delta_translation_along_rotation_plane;
			const double inner_translation_side = rtm::scalar_sqrt(rtm::scalar_max(delta_translation_len_sq - delta_translation_along_rotation_plane_sq, 0.0));

			const double inner_full_side = inner_translation_side + max_delta_rotation_error;
			const double inner_full_side_sq = inner_full_side * inner_full_side;
			return delta_translation_along_rotation_plane_sq + inner_full_side_sq;
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the largest squared distance between two VQS transforms on a sphere, or a value above it.
		// With 'p' a point on the sphere and 's0', 's1' the raw and lossy scales:
		//    error(p) = |s1 * R1 * p - s0 * R0 * p + (t1 - t0)|
		//             = |s0 * (R1 - R0) * p + (t1 - t0) + (s1 - s0) * R1 * p|
		//            <= rigid error with a radius of |s0| * r, plus |s1 - s0| * r
		// The same is true when we swap 's0' and 's1'. We return the smaller of the two bounds.
		// When the two scales are equal, the result is exact.
		inline double calculate_vqs_error_squared_precise(const rtm::vqsf& raw_transform, const rtm::vqsf& lossy_transform, double sphere_radius)
		{
			const float raw_scale_f = rtm::vector_get_w(raw_transform.translation_scale);
			const float lossy_scale_f = rtm::vector_get_w(lossy_transform.translation_scale);
			const double raw_scale = raw_scale_f;
			const double lossy_scale = lossy_scale_f;

			// We use float64 for the added precision otherwise floating point noise in the rotation
			// can cause instability
			const rtm::vector4f no_scale = rtm::vector_set(1.0F);
			const rtm::vqvd raw_transform_d = rtm::vqv_normalize(rtm::vqv_cast(rtm::vqv_set(raw_transform.rotation, raw_transform.translation_scale, no_scale)));
			const rtm::vqvd lossy_transform_d = rtm::vqv_normalize(rtm::vqv_cast(rtm::vqv_set(lossy_transform.rotation, lossy_transform.translation_scale, no_scale)));

			const double scale_error = rtm::scalar_abs(lossy_scale - raw_scale) * sphere_radius;

			const double raw_rigid_error = rtm::scalar_sqrt(calculate_rigid_error_squared_precise(raw_transform_d, lossy_transform_d, rtm::scalar_abs(raw_scale) * sphere_radius));
			if (scale_error == 0.0)
				return raw_rigid_error * raw_rigid_error;	// The scales are equal, the result is exact

			const double lossy_rigid_error = rtm::scalar_sqrt(calculate_rigid_error_squared_precise(raw_transform_d, lossy_transform_d, rtm::scalar_abs(lossy_scale) * sphere_radius));

			const double error = rtm::scalar_min(raw_rigid_error, lossy_rigid_error) + scale_error;
			return error * error;
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the largest squared distance between two VQ transforms on a sphere.
		// The scale of the rtm::vqvf transforms is not used. The result is exact.
		inline double calculate_vq_error_squared_precise(const rtm::vqvf& raw_transform, const rtm::vqvf& lossy_transform, double sphere_radius)
		{
			// We use float64 for the added precision otherwise floating point noise in the rotation
			// can cause instability
			const rtm::vqvd raw_transform_d = rtm::vqv_normalize(rtm::vqv_cast(raw_transform));
			const rtm::vqvd lossy_transform_d = rtm::vqv_normalize(rtm::vqv_cast(lossy_transform));
			return calculate_rigid_error_squared_precise(raw_transform_d, lossy_transform_d, sphere_radius);
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the largest squared distance between the raw and lossy points of the rigid shell.
		// The points are stored transposed, see 'itransform_metric::calculate_error_args'.
		// Without a scale ('is_vq' is true), we use the first two points only.
		inline rtm::scalarf RTM_SIMD_CALL calculate_shell_error_squared(
			rtm::vector4f_arg0 raw_points_xxxx, rtm::vector4f_arg1 raw_points_yyyy, rtm::vector4f_arg2 raw_points_zzzz,
			rtm::vector4f_arg3 lossy_points_xxxx, rtm::vector4f_arg4 lossy_points_yyyy, rtm::vector4f_arg5 lossy_points_zzzz,
			bool is_vq)
		{
			const rtm::vector4f error_sq_xyzw = vector_distance_squared3_x4(
				raw_points_xxxx, raw_points_yyyy, raw_points_zzzz,
				lossy_points_xxxx, lossy_points_yyyy, lossy_points_zzzz);

#if defined(ACL_IMPL_METRIC_USE_4_POINTS)
			(void)is_vq;
			return rtm::vector_get_max_component_as_scalar(error_sq_xyzw);
#else
	#if !defined(ACL_IMPL_METRIC_USE_3_POINTS_WITHOUT_SCALE)
			// Without a scale, two points are enough
			if (is_vq)
				return rtm::scalar_max(rtm::vector_get_x_as_scalar(error_sq_xyzw), rtm::vector_get_y_as_scalar(error_sq_xyzw));
	#else
			(void)is_vq;
	#endif

			// Because we have a scale, we must measure all three axes
			return rtm::vector_get_max_component_as_scalar(rtm::vector_set_w(error_sq_xyzw, 0.0F));
#endif
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Measures the error on points of the rigid shell.
	// It transforms the points with the raw and lossy transforms and returns the largest distance.
	// It supports every transform adapter. With a scale, it measures three points; without a scale (VQ), two points.
	// The points give an approximation of the error. Use it with transforms that have a 3D scale or a shear
	// (VQV, VQM, or a matrix). For VQ and VQS transforms, 'precise_transform_metric' gives the exact error.
	//////////////////////////////////////////////////////////////////////////
	class shell_transform_metric : public itransform_metric
	{
	public:
		virtual const char* get_name() const override { return "shell_transform_metric"; }

		virtual bool is_supported(const itransform_adapter& adapter) const override { (void)adapter; return true; }

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const override
		{
			const itransform_adapter& adapter = *args.adapter;

			rtm::vector4f raw_points_xxxx;
			rtm::vector4f raw_points_yyyy;
			rtm::vector4f raw_points_zzzz;
			adapter.mul_point3_x4(
				args.shell_points_xxxx, args.shell_points_yyyy, args.shell_points_zzzz,
				args.transform0,
				raw_points_xxxx, raw_points_yyyy, raw_points_zzzz);

			rtm::vector4f lossy_points_xxxx;
			rtm::vector4f lossy_points_yyyy;
			rtm::vector4f lossy_points_zzzz;
			adapter.mul_point3_x4(
				args.shell_points_xxxx, args.shell_points_yyyy, args.shell_points_zzzz,
				args.transform1,
				lossy_points_xxxx, lossy_points_yyyy, lossy_points_zzzz);

			return acl_impl::calculate_shell_error_squared(
				raw_points_xxxx, raw_points_yyyy, raw_points_zzzz,
				lossy_points_xxxx, lossy_points_yyyy, lossy_points_zzzz,
				adapter.get_type() == transform_runtime_type_t::vqf);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Measures the exact largest error on the rigid shell, for VQ and VQS transforms.
	// It uses the rotation, the translation, and the uniform scale to find the point on
	// the sphere with the largest error. It computes with float64.
	// With a VQS transform, when the raw and lossy scales are not equal, the result can be
	// above the exact error by no more than |s1 - s0| * shell_distance. See 'calculate_vqs_error_squared_precise'.
	// It supports only the 'vqf' and 'vqsf' transform adapters.
	//////////////////////////////////////////////////////////////////////////
	class precise_transform_metric : public itransform_metric
	{
	public:
		virtual const char* get_name() const override { return "precise_transform_metric"; }

		virtual bool is_supported(const itransform_adapter& adapter) const override
		{
			const transform_runtime_type_t type = adapter.get_type();
			return type == transform_runtime_type_t::vqf || type == transform_runtime_type_t::vqsf;
		}

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const override
		{
			ACL_ASSERT(is_supported(*args.adapter), "The precise transform metric does not support this transform adapter");

			const double sphere_radius = double(args.shell_distance);

			double error_sq;
			if (args.adapter->get_type() == transform_runtime_type_t::vqsf)
			{
				const rtm::vqsf& raw_transform = *static_cast<const rtm::vqsf*>(args.transform0);
				const rtm::vqsf& lossy_transform = *static_cast<const rtm::vqsf*>(args.transform1);
				error_sq = acl_impl::calculate_vqs_error_squared_precise(raw_transform, lossy_transform, sphere_radius);
			}
			else
			{
				// The VQ adapter uses rtm::vqvf and we do not use the scale
				const rtm::vqvf& raw_transform = *static_cast<const rtm::vqvf*>(args.transform0);
				const rtm::vqvf& lossy_transform = *static_cast<const rtm::vqvf*>(args.transform1);
				error_sq = acl_impl::calculate_vq_error_squared_precise(raw_transform, lossy_transform, sphere_radius);
			}

			return rtm::scalar_set(float(error_sq));
		}
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
