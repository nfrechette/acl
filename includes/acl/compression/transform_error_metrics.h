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

#include "acl/version.h"
#include "acl/core/additive_utils.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/hash.h"
#include "acl/core/track_types.h"
#include "acl/math/vector4f.h"
#include "acl/math/quatf.h"
#include "acl/math/qvvf.h"

#include <rtm/matrix3x4f.h>
#include <rtm/qvvd.h>
#include <rtm/qvvf.h>
#include <rtm/scalarf.h>

// By default, we use an optimize version of error metrics that leverages SIMD by
// computing the error in Structure Of Array form (xxxx, yyyy, zzzz). This means that we can generally
// compute the error for 2, 3, or 4 points (identical cost) more cheaply than it is for 2 points
// in a classic Array of Structure form (xyz, xyz, xyz, xyz). We leave the define here
// to facilitate debugging as SoA code can be quite opaque.
#define ACL_IMPL_USE_SOA_ERROR_METRIC

// Whether or not to use a third point even when no scale is present
//#define ACL_IMPL_USE_3_POINTS_WITHOUT_SCALE

// Whether or not to use a fourth point to improve coverage
//#define ACL_IMPL_USE_4_POINTS

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// Interface for all skeletal error metrics.
	// An error metric is responsible for a few things:
	//    - converting from rtm::qvvf into whatever transform type the metric uses (optional)
	//    - applying local space transforms on top of base transforms (optional)
	//    - transforming local space transforms into object space
	//    - evaluating the error function
	//
	// Most functions require two implementations: with and without scale support.
	// This is entirely for performance reasons as most clips do not have any scale.
	//////////////////////////////////////////////////////////////////////////
	class itransform_error_metric
	{
	public:
		virtual ~itransform_error_metric() = default;

		//////////////////////////////////////////////////////////////////////////
		// Returns the unique name of the error metric.
		virtual const char* get_name() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns a unique hash to represent the error metric.
		virtual uint32_t get_hash() const { return hash32(get_name()); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the transform size used by the error metric.
		virtual size_t get_transform_size(bool has_scale) const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not the error metric uses a transform that isn't rtm::qvvf.
		// If this is the case, we need to convert from rtm::qvvf into the transform type
		// used by the error metric.
		virtual bool needs_conversion(bool has_scale) const { (void)has_scale; return false; }

		//////////////////////////////////////////////////////////////////////////
		// Whether or not this error metric supports the new bit rate algorithm v2
		// See: [Bit Rate Optimization Algorithm]
		virtual bool supports_bit_rate_algorithm_v2() const { return false; }

		//////////////////////////////////////////////////////////////////////////
		// Input arguments for the 'convert_transforms*' functions.
		//////////////////////////////////////////////////////////////////////////
		struct convert_transforms_args
		{
			//////////////////////////////////////////////////////////////////////////
			// A list of transform indices that are dirty and need conversion.
			const uint32_t* dirty_transform_indices;

			//////////////////////////////////////////////////////////////////////////
			// The number of dirty transforms that need conversion.
			uint32_t num_dirty_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The input transforms in rtm::qvvf format to be converted.
			const rtm::qvvf* transforms;

			//////////////////////////////////////////////////////////////////////////
			// The number of transforms in the input and output buffers.
			uint32_t num_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The sample index these transforms come from.
			uint32_t sample_index;

			//////////////////////////////////////////////////////////////////////////
			// True if these transforms are being converted from lossy data.
			// False if these transforms are being converted from the original uncompressed data.
			bool is_lossy;

			//////////////////////////////////////////////////////////////////////////
			// True if these transforms are being converted from the additive base pose.
			// False if these transforms are being converted from the additive pose.
			// If not using a base, this value is always false.
			bool is_additive_base;
		};

		//////////////////////////////////////////////////////////////////////////
		// Converts from rtm::qvvf into the transform type used by the error metric.
		// Called when 'needs_conversion' returns true.
		virtual void convert_transforms(const convert_transforms_args& args, void* out_transforms) const
		{
			(void)args;
			(void)out_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Converts from rtm::qvvf into the transform type used by the error metric.
		// Called when 'needs_conversion' returns true.
		virtual void convert_transforms_no_scale(const convert_transforms_args& args, void* out_transforms) const
		{
			(void)args;
			(void)out_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Input arguments for the 'local_to_object_space*' functions.
		//////////////////////////////////////////////////////////////////////////
		struct local_to_object_space_args
		{
			//////////////////////////////////////////////////////////////////////////
			// A list of transform indices that are dirty and need transformation.
			const uint32_t* dirty_transform_indices;

			//////////////////////////////////////////////////////////////////////////
			// The number of dirty transforms that need transformation.
			uint32_t num_dirty_transforms;

			//////////////////////////////////////////////////////////////////////////
			// A list of parent transform indices for every transform.
			// An index of 0xFFFF represents a root transform with no parent.
			const uint32_t* parent_transform_indices;

			//////////////////////////////////////////////////////////////////////////
			// The input transforms in the type expected by the error metric to be transformed.
			const void* local_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The number of transforms in the input and output buffers.
			uint32_t num_transforms;
		};

		//////////////////////////////////////////////////////////////////////////
		// Takes local space transforms into object space.
		virtual void local_to_object_space(const local_to_object_space_args& args, void* out_object_transforms) const
		{
			(void)args;
			(void)out_object_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Takes local space transforms into object space.
		virtual void local_to_object_space_no_scale(const local_to_object_space_args& args, void* out_object_transforms) const
		{
			(void)args;
			(void)out_object_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Input arguments for the 'apply_additive_to_base*' functions.
		//////////////////////////////////////////////////////////////////////////
		struct apply_additive_to_base_args
		{
			//////////////////////////////////////////////////////////////////////////
			// A list of transform indices that are dirty and need the base applied.
			const uint32_t* dirty_transform_indices;

			//////////////////////////////////////////////////////////////////////////
			// The number of dirty transforms that need the base applied.
			uint32_t num_dirty_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The input local space transforms in the type expected by the error metric.
			const void* local_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The input base transforms in the type expected by the error metric.
			const void* base_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The number of transforms in the input and output buffers.
			uint32_t num_transforms;
		};

		//////////////////////////////////////////////////////////////////////////
		// Applies local space transforms on top of base transforms.
		// This is called when a clip has an additive base.
		virtual void apply_additive_to_base(const apply_additive_to_base_args& args, void* out_transforms) const
		{
			(void)args;
			(void)out_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Applies local space transforms on top of base transforms.
		// This is called when a clip has an additive base.
		virtual void apply_additive_to_base_no_scale(const apply_additive_to_base_args& args, void* out_transforms) const
		{
			(void)args;
			(void)out_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Input arguments for the 'calculate_error*' functions.
		struct calculate_error_args
		{
			// A point on our rigid shell along the X axis.
			rtm::vector4f shell_point_x;

			// A point on our rigid shell along the Y axis.
			rtm::vector4f shell_point_y;

			// A point on our rigid shell along the Z axis.
			rtm::vector4f shell_point_z;

#if defined(ACL_IMPL_USE_4_POINTS)
			// A point on our rigid shell along the mid line between the XYZ axes.
			rtm::vector4f shell_point_mid;
#endif

#if defined(ACL_IMPL_USE_SOA_ERROR_METRIC)
			// Points on our rigid shell stored transposed.
			rtm::vector4f shell_points_xxxx;

			// Points on our rigid shell stored transposed.
			rtm::vector4f shell_points_yyyy;

			// Points on our rigid shell stored transposed.
			rtm::vector4f shell_points_zzzz;
#endif

			// The first transform used to measure the error.
			// In the type expected by the error metric.
			// Could be in local or object space (same space as lossy).
			const void* transform0;

			// The second transform used to measure the error.
			// In the type expected by the error metric.
			// Could be in local or object space (same space as raw).
			const void* transform1;

			// We measure the error on a rigid shell around each transform.
			// This shell takes the form of a sphere at a certain distance.
			// When no scale is present, measuring any two points is sufficient
			// but when there is scale, measuring all three is necessary.
			// See ./docs/error_metrics.md for details.
			void construct_sphere_shell(float shell_distance)
			{
				shell_point_x = rtm::vector_set(shell_distance, 0.0F, 0.0F, 0.0F);
				shell_point_y = rtm::vector_set(0.0F, shell_distance, 0.0F, 0.0F);
				shell_point_z = rtm::vector_set(0.0F, 0.0F, shell_distance, 0.0F);

#if defined(ACL_IMPL_USE_4_POINTS)
				// We want the 4th point to be between all 3 others
				// Because they are axis aligned, we wish to be at 45 degrees on the XY plane
				// and at 45 degrees along the XZ and YZ planes as well
				// The easiest way to compute this point is to add the first three
				// (1, 0, 0) + (0, 1, 0) + (0, 0, 1) = (1, 1, 1)
				// Now we can normalize this to obtain the direction:
				// ||(1, 1, 1)|| = sqrt(3) = 1.73205081
				// (1, 1, 1) / sqrt(3) = (0.57735027, 0.57735027, 0.57735027)

				shell_point_mid = rtm::vector_mul(rtm::vector_set(0.57735027F, 0.57735027F, 0.57735027F, 0.0F), shell_distance);
#endif

#if defined(ACL_IMPL_USE_SOA_ERROR_METRIC)
	#if defined(ACL_IMPL_USE_4_POINTS)
				RTM_MATRIXF_TRANSPOSE_4X3(
					shell_point_x, shell_point_y, shell_point_z, shell_point_mid,
					shell_points_xxxx, shell_points_yyyy, shell_points_zzzz);
	#else
				RTM_MATRIXF_TRANSPOSE_3X3(
					shell_point_x, shell_point_y, shell_point_z,
					shell_points_xxxx, shell_points_yyyy, shell_points_zzzz);
	#endif
#endif
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

		//////////////////////////////////////////////////////////////////////////
		// Measures the error between a raw and lossy transform.
		rtm::scalarf RTM_SIMD_CALL calculate_error_no_scale(const calculate_error_args& args) const
		{
			return rtm::scalar_sqrt(calculate_error_squared_no_scale(args));
		}

		//////////////////////////////////////////////////////////////////////////
		// Measures the squared error between a raw and lossy transform.
		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared_no_scale(const calculate_error_args& args) const = 0;
	};

	//////////////////////////////////////////////////////////////////////////
	// Uses rtm::qvvf arithmetic for local and object space error.
	// Note that this can cause inaccuracy when dealing with shear/skew.
	//////////////////////////////////////////////////////////////////////////
	class qvvf_transform_error_metric : public itransform_error_metric
	{
	public:
		virtual const char* get_name() const override { return "qvvf_transform_error_metric"; }

		virtual size_t get_transform_size(bool has_scale) const override { (void)has_scale; return sizeof(rtm::qvvf); }
		virtual bool needs_conversion(bool has_scale) const override { (void)has_scale; return false; }
		virtual bool supports_bit_rate_algorithm_v2() const override { return true; }

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void local_to_object_space(const local_to_object_space_args& args, void* out_object_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const uint32_t* parent_transform_indices = args.parent_transform_indices;
			const rtm::qvvf* local_transforms_ = static_cast<const rtm::qvvf*>(args.local_transforms);
			rtm::qvvf* out_object_transforms_ = static_cast<rtm::qvvf*>(out_object_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
				const uint32_t parent_transform_index = parent_transform_indices[transform_index];

				rtm::qvvf obj_transform;
				if (parent_transform_index == k_invalid_track_index)
					obj_transform = local_transforms_[transform_index];	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
				else
					obj_transform = rtm::qvv_normalize(rtm::qvv_mul(local_transforms_[transform_index], out_object_transforms_[parent_transform_index]));

				out_object_transforms_[transform_index] = obj_transform;
			}
		}

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void local_to_object_space_no_scale(const local_to_object_space_args& args, void* out_object_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const uint32_t* parent_transform_indices = args.parent_transform_indices;
			const rtm::qvvf* local_transforms_ = static_cast<const rtm::qvvf*>(args.local_transforms);
			rtm::qvvf* out_object_transforms_ = static_cast<rtm::qvvf*>(out_object_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
				const uint32_t parent_transform_index = parent_transform_indices[transform_index];

				rtm::qvvf obj_transform;
				if (parent_transform_index == k_invalid_track_index)
					obj_transform = local_transforms_[transform_index];	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
				else
					obj_transform = rtm::qvv_normalize(rtm::qvv_mul_no_scale(local_transforms_[transform_index], out_object_transforms_[parent_transform_index]));

				out_object_transforms_[transform_index] = obj_transform;
			}
		}

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const override
		{
			const rtm::qvvf& raw_transform_ = *static_cast<const rtm::qvvf*>(args.transform0);
			const rtm::qvvf& lossy_transform_ = *static_cast<const rtm::qvvf*>(args.transform1);

			// Note that because we have scale, we must measure all three axes

#if defined(ACL_IMPL_USE_SOA_ERROR_METRIC)
			const rtm::vector4f vtx_xxx_ = args.shell_points_xxxx;
			const rtm::vector4f vtx_yyy_ = args.shell_points_yyyy;
			const rtm::vector4f vtx_zzz_ = args.shell_points_zzzz;

			rtm::vector4f raw_vtx_xxx_;
			rtm::vector4f raw_vtx_yyy_;
			rtm::vector4f raw_vtx_zzz_;
			acl_impl::qvv_mul_point3_x4(
				vtx_xxx_, vtx_yyy_, vtx_zzz_,
				raw_transform_,
				raw_vtx_xxx_, raw_vtx_yyy_, raw_vtx_zzz_);

			rtm::vector4f lossy_vtx_xxx_;
			rtm::vector4f lossy_vtx_yyy_;
			rtm::vector4f lossy_vtx_zzz_;
			acl_impl::qvv_mul_point3_x4(
				vtx_xxx_, vtx_yyy_, vtx_zzz_,
				lossy_transform_,
				lossy_vtx_xxx_, lossy_vtx_yyy_, lossy_vtx_zzz_);

			rtm::vector4f vtx_error_sq_xyz_ = acl_impl::vector_distance_squared3_x4(
				raw_vtx_xxx_, raw_vtx_yyy_, raw_vtx_zzz_,
				lossy_vtx_xxx_, lossy_vtx_yyy_, lossy_vtx_zzz_);

	#if defined(ACL_IMPL_USE_4_POINTS)
			return rtm::vector_get_max_component_as_scalar(vtx_error_sq_xyz_);
	#else
			return rtm::vector_get_max_component_as_scalar(rtm::vector_set_w(vtx_error_sq_xyz_, 0.0F));
	#endif
#else
			const rtm::vector4f vtx0 = args.shell_point_x;
			const rtm::vector4f vtx1 = args.shell_point_y;
			const rtm::vector4f vtx2 = args.shell_point_z;

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_transform_);
			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_transform_);
			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_transform_);

			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3(vtx0, lossy_transform_);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3(vtx1, lossy_transform_);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3(vtx2, lossy_transform_);

			const rtm::scalarf vtx0_error = rtm::vector_distance_squared3_as_scalar(raw_vtx0, lossy_vtx0);
			const rtm::scalarf vtx1_error = rtm::vector_distance_squared3_as_scalar(raw_vtx1, lossy_vtx1);
			const rtm::scalarf vtx2_error = rtm::vector_distance_squared3_as_scalar(raw_vtx2, lossy_vtx2);

	#if defined(ACL_IMPL_USE_4_POINTS)
			const rtm::vector4f vtx3 = args.shell_point_mid;
			const rtm::vector4f raw_vtx3 = rtm::qvv_mul_point3(vtx3, raw_transform_);
			const rtm::vector4f lossy_vtx3 = rtm::qvv_mul_point3(vtx3, lossy_transform_);
			const rtm::scalarf vtx3_error = rtm::vector_distance_squared3_as_scalar(raw_vtx3, lossy_vtx3);
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), rtm::scalar_max(vtx2_error, vtx3_error));
	#else
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
	#endif
#endif
		}

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared_no_scale(const calculate_error_args& args) const override
		{
			const rtm::qvvf& raw_transform_ = *static_cast<const rtm::qvvf*>(args.transform0);
			const rtm::qvvf& lossy_transform_ = *static_cast<const rtm::qvvf*>(args.transform1);

#if defined(ACL_IMPL_USE_SOA_ERROR_METRIC)
			const rtm::vector4f vtx_xx__ = args.shell_points_xxxx;
			const rtm::vector4f vtx_yy__ = args.shell_points_yyyy;
			const rtm::vector4f vtx_zz__ = args.shell_points_zzzz;

			rtm::vector4f raw_vtx_xxx_;
			rtm::vector4f raw_vtx_yyy_;
			rtm::vector4f raw_vtx_zzz_;
			acl_impl::qvv_mul_point3_no_scale_x4(
				vtx_xx__, vtx_yy__, vtx_zz__,
				raw_transform_,
				raw_vtx_xxx_, raw_vtx_yyy_, raw_vtx_zzz_);

			rtm::vector4f lossy_vtx_xxx_;
			rtm::vector4f lossy_vtx_yyy_;
			rtm::vector4f lossy_vtx_zzz_;
			acl_impl::qvv_mul_point3_no_scale_x4(
				vtx_xx__, vtx_yy__, vtx_zz__,
				lossy_transform_,
				lossy_vtx_xxx_, lossy_vtx_yyy_, lossy_vtx_zzz_);

			rtm::vector4f vtx_error_sq_xy__ = acl_impl::vector_distance_squared3_x4(
				raw_vtx_xxx_, raw_vtx_yyy_, raw_vtx_zzz_,
				lossy_vtx_xxx_, lossy_vtx_yyy_, lossy_vtx_zzz_);

	#if defined(ACL_IMPL_USE_4_POINTS)
			return rtm::vector_get_max_component_as_scalar(vtx_error_sq_xy__);
	#elif defined(ACL_IMPL_USE_3_POINTS_WITHOUT_SCALE)
			return rtm::vector_get_max_component_as_scalar(rtm::vector_set_w(vtx_error_sq_xy__, 0.0F));
	#else
			return rtm::scalar_max(rtm::vector_get_x_as_scalar(vtx_error_sq_xy__), rtm::vector_get_y_as_scalar(vtx_error_sq_xy__));
	#endif
#else
			const rtm::vector4f vtx0 = args.shell_point_x;
			const rtm::vector4f vtx1 = args.shell_point_y;

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, raw_transform_);
			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, raw_transform_);

			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, lossy_transform_);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, lossy_transform_);

			const rtm::scalarf vtx0_error = rtm::vector_distance_squared3_as_scalar(raw_vtx0, lossy_vtx0);
			const rtm::scalarf vtx1_error = rtm::vector_distance_squared3_as_scalar(raw_vtx1, lossy_vtx1);

	#if defined(ACL_IMPL_USE_4_POINTS)
			const rtm::vector4f vtx2 = args.shell_point_z;
			const rtm::vector4f vtx3 = args.shell_point_mid;
			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3_no_scale(vtx2, raw_transform_);
			const rtm::vector4f raw_vtx3 = rtm::qvv_mul_point3_no_scale(vtx3, raw_transform_);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3_no_scale(vtx2, lossy_transform_);
			const rtm::vector4f lossy_vtx3 = rtm::qvv_mul_point3_no_scale(vtx3, lossy_transform_);
			const rtm::scalarf vtx2_error = rtm::vector_distance_squared3_as_scalar(raw_vtx2, lossy_vtx2);
			const rtm::scalarf vtx3_error = rtm::vector_distance_squared3_as_scalar(raw_vtx3, lossy_vtx3);
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), rtm::scalar_max(vtx2_error, vtx3_error));
	#elif defined(ACL_IMPL_USE_3_POINTS_WITHOUT_SCALE)
			const rtm::vector4f vtx2 = args.shell_point_z;
			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3_no_scale(vtx2, raw_transform_);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3_no_scale(vtx2, lossy_transform_);
			const rtm::scalarf vtx2_error = rtm::vector_distance_squared3_as_scalar(raw_vtx2, lossy_vtx2);
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
	#else
			return rtm::scalar_max(vtx0_error, vtx1_error);
	#endif
#endif
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Uses an analytical computation for the exact error introduced. This is as
	// precise as it gets when no scale is present. We fallback to the normal
	// qvvf approximation when scale is present.
	// This computes the error on the point most impacted by the lossy transform
	// which guarantees that all other points see the same or a lower error.
	//////////////////////////////////////////////////////////////////////////
	class qvvf_precise_transform_error_metric : public qvvf_transform_error_metric
	{
	public:
		virtual const char* get_name() const override { return "qvvf_precise_transform_error_metric"; }

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared_no_scale(const calculate_error_args& args) const override
		{
			const rtm::qvvf& raw_transform_ = *static_cast<const rtm::qvvf*>(args.transform0);
			const rtm::qvvf& lossy_transform_ = *static_cast<const rtm::qvvf*>(args.transform1);

			// We use float64 for the added precision otherwise floating point noise in the rotation
			// can cause instability
			const rtm::qvvd raw_transform_d = rtm::qvv_normalize(rtm::qvv_cast(raw_transform_));
			const rtm::qvvd lossy_transform_d = rtm::qvv_normalize(rtm::qvv_cast(lossy_transform_));

			// Our result
			double max_delta_transform_error_sq = 0.0;

			// We use the quaternion dot product to determine the cosine of the half angle between the raw
			// and lossy rotations. If the angle is very small, the absolute value of the dot product will
			// be very close to 1.0
			// Due to floating point precision, we use a threshold value to determine if we have any delta between the two
			const double raw_lossy_quat_dot = rtm::quat_dot(raw_transform_d.rotation, lossy_transform_d.rotation);
			const bool has_rotation_delta = rtm::scalar_abs(raw_lossy_quat_dot) < 0.99999999999999822;	// 0x3feffffffffffff0
			if (has_rotation_delta)
			{
				// We remove the raw transform from the lossy transform to form a delta transform between the two
				// We remove the raw transform instead of the lossy because this ensures that we always see consistent results
				// It would also allow us to compute the raw inverse ahead of time if we wanted to
				const rtm::qvvd delta_transform = rtm::qvv_normalize(rtm::qvv_mul_no_scale(lossy_transform_d, rtm::qvv_inverse_no_scale(raw_transform_d)));

				// We first calculate the contribution from the rotation delta
				const rtm::vector4d rotation_plane_normal = rtm::vector_normalize3(rtm::quat_to_vector(delta_transform.rotation));
				ACL_ASSERT(rtm::vector_is_finite3(rotation_plane_normal), "Delta rotation plane must exist");

				// Using the unit circle to compute the sine of our half rotation angle
				// 1^2 = cos(a)^2 + sin(a)^2
				const double half_sin_angle = rtm::scalar_sqrt(1.0 - (raw_lossy_quat_dot * raw_lossy_quat_dot));

				// We can then compute the rotation error delta by scaling it with the sphere radius
				const double sphere_radius = (double)rtm::vector_get_x(args.shell_point_x);
				const double max_delta_rotation_error = half_sin_angle * sphere_radius * 2.0;

				// We then calculate the full transform error by incorporating the translation contribution
				const rtm::vector4d delta_translation = delta_transform.translation;
				const double delta_translation_len_sq = rtm::vector_length_squared3(delta_translation);
				const double delta_translation_along_rotation_plane = rtm::vector_dot3(delta_translation, rotation_plane_normal);
				const double delta_translation_along_rotation_plane_sq = delta_translation_along_rotation_plane * delta_translation_along_rotation_plane;
				const double inner_translation_side = rtm::scalar_sqrt(rtm::scalar_max(delta_translation_len_sq - delta_translation_along_rotation_plane_sq, 0.0));

				const double inner_full_side = inner_translation_side + max_delta_rotation_error;
				const double inner_full_side_sq = inner_full_side * inner_full_side;
				max_delta_transform_error_sq = delta_translation_along_rotation_plane_sq + inner_full_side_sq;
			}
			else
			{
				// If we have no rotation delta, then the transform error is just the translation error: the delta length
				max_delta_transform_error_sq = rtm::scalar_cast(rtm::vector_distance_squared3_as_scalar(raw_transform_d.translation, lossy_transform_d.translation));
			}

			return rtm::scalar_set((float)max_delta_transform_error_sq);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Uses a mix of rtm::qvvf and rtm::matrix3x4f arithmetic.
	// The local space error is always calculated with rtm::qvvf arithmetic.
	// The object space error is calculated with rtm::qvvf arithmetic if there is no scale
	// and with rtm::matrix3x4f arithmetic if there is scale.
	// Note that this can cause inaccuracy issues if there are very large or very small
	// scale values.
	//////////////////////////////////////////////////////////////////////////
	class qvvf_matrix3x4f_transform_error_metric : public qvvf_transform_error_metric
	{
	public:
		virtual const char* get_name() const override { return "qvvf_matrix3x4f_transform_error_metric"; }

		virtual size_t get_transform_size(bool has_scale) const override { return has_scale ? sizeof(rtm::matrix3x4f) : sizeof(rtm::qvvf); }
		virtual bool needs_conversion(bool has_scale) const override { return has_scale; }
		virtual bool supports_bit_rate_algorithm_v2() const override { return false; }

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void convert_transforms(const convert_transforms_args& args, void* out_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const rtm::qvvf* transforms_ = args.transforms;
			rtm::matrix3x4f* out_transforms_ = static_cast<rtm::matrix3x4f*>(out_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];

				const rtm::qvvf& transform_qvv = transforms_[transform_index];
				rtm::matrix3x4f transform_mtx = rtm::matrix_from_qvv(transform_qvv);

				out_transforms_[transform_index] = transform_mtx;
			}
		}

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void local_to_object_space(const local_to_object_space_args& args, void* out_object_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const uint32_t* parent_transform_indices = args.parent_transform_indices;
			const rtm::matrix3x4f* local_transforms_ = static_cast<const rtm::matrix3x4f*>(args.local_transforms);
			rtm::matrix3x4f* out_object_transforms_ = static_cast<rtm::matrix3x4f*>(out_object_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
				const uint32_t parent_transform_index = parent_transform_indices[transform_index];

				rtm::matrix3x4f obj_transform;
				if (parent_transform_index == k_invalid_track_index)
					obj_transform = local_transforms_[transform_index];	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
				else
					obj_transform = rtm::matrix_mul(local_transforms_[transform_index], out_object_transforms_[parent_transform_index]);

				out_object_transforms_[transform_index] = obj_transform;
			}
		}

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const override
		{
			const rtm::matrix3x4f& raw_transform_ = *static_cast<const rtm::matrix3x4f*>(args.transform0);
			const rtm::matrix3x4f& lossy_transform_ = *static_cast<const rtm::matrix3x4f*>(args.transform1);

			// Note that because we have scale, we must measure all three axes
			const rtm::vector4f vtx0 = args.shell_point_x;
			const rtm::vector4f vtx1 = args.shell_point_y;
			const rtm::vector4f vtx2 = args.shell_point_z;

			const rtm::vector4f raw_vtx0 = rtm::matrix_mul_point3(vtx0, raw_transform_);
			const rtm::vector4f raw_vtx1 = rtm::matrix_mul_point3(vtx1, raw_transform_);
			const rtm::vector4f raw_vtx2 = rtm::matrix_mul_point3(vtx2, raw_transform_);

			const rtm::vector4f lossy_vtx0 = rtm::matrix_mul_point3(vtx0, lossy_transform_);
			const rtm::vector4f lossy_vtx1 = rtm::matrix_mul_point3(vtx1, lossy_transform_);
			const rtm::vector4f lossy_vtx2 = rtm::matrix_mul_point3(vtx2, lossy_transform_);

			const rtm::scalarf vtx0_error = rtm::vector_distance_squared3_as_scalar(raw_vtx0, lossy_vtx0);
			const rtm::scalarf vtx1_error = rtm::vector_distance_squared3_as_scalar(raw_vtx1, lossy_vtx1);
			const rtm::scalarf vtx2_error = rtm::vector_distance_squared3_as_scalar(raw_vtx2, lossy_vtx2);

#if defined(ACL_IMPL_USE_4_POINTS)
			const rtm::vector4f vtx3 = args.shell_point_mid;
			const rtm::vector4f raw_vtx3 = rtm::matrix_mul_point3(vtx3, raw_transform_);
			const rtm::vector4f lossy_vtx3 = rtm::matrix_mul_point3(vtx3, lossy_transform_);
			const rtm::scalarf vtx3_error = rtm::vector_distance_squared3_as_scalar(raw_vtx3, lossy_vtx3);
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), rtm::scalar_max(vtx2_error, vtx3_error));
#else
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
#endif
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Uses rtm::qvvf arithmetic for local and object space error.
	// This error metric should be used whenever a clip is additive or relative.
	// Note that this can cause inaccuracy when dealing with shear/skew.
	//////////////////////////////////////////////////////////////////////////
	template<additive_clip_format8 additive_format>
	class additive_qvvf_transform_error_metric : public qvvf_transform_error_metric
	{
	public:
		virtual const char* get_name() const override
		{
			switch (additive_format)
			{
			default:
			case additive_clip_format8::none:			return "additive_qvvf_transform_error_metric<none>";
			case additive_clip_format8::relative:		return "additive_qvvf_transform_error_metric<relative>";
			case additive_clip_format8::additive0:		return "additive_qvvf_transform_error_metric<additive0>";
			case additive_clip_format8::additive1:		return "additive_qvvf_transform_error_metric<additive1>";
			}
		}

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void apply_additive_to_base(const apply_additive_to_base_args& args, void* out_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const rtm::qvvf* local_transforms_ = static_cast<const rtm::qvvf*>(args.local_transforms);
			const rtm::qvvf* base_transforms_ = static_cast<const rtm::qvvf*>(args.base_transforms);
			rtm::qvvf* out_transforms_ = static_cast<rtm::qvvf*>(out_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];

				const rtm::qvvf& local_transform = local_transforms_[transform_index];
				const rtm::qvvf& base_transform = base_transforms_[transform_index];
				const rtm::qvvf transform = acl::apply_additive_to_base(additive_format, base_transform, local_transform);

				out_transforms_[transform_index] = transform;
			}
		}

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void apply_additive_to_base_no_scale(const apply_additive_to_base_args& args, void* out_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const rtm::qvvf* local_transforms_ = static_cast<const rtm::qvvf*>(args.local_transforms);
			const rtm::qvvf* base_transforms_ = static_cast<const rtm::qvvf*>(args.base_transforms);
			rtm::qvvf* out_transforms_ = static_cast<rtm::qvvf*>(out_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];

				const rtm::qvvf& local_transform = local_transforms_[transform_index];
				const rtm::qvvf& base_transform = base_transforms_[transform_index];
				const rtm::qvvf transform = acl::apply_additive_to_base_no_scale(additive_format, base_transform, local_transform);

				out_transforms_[transform_index] = transform;
			}
		}
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
