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
#include "acl/core/iallocator.h"
#include "acl/core/track_types.h"
#include "acl/compression/fwd.h"
#include "acl/compression/transform_adapters.h"
#include "acl/compression/transform_metrics.h"
#include "acl/math/vector4f.h"
#include "acl/math/quatf.h"
#include "acl/math/vqvf.h"

#include <rtm/matrix3x4f.h>
#include <rtm/vqvd.h>
#include <rtm/vqvf.h>
#include <rtm/scalarf.h>

#include <cstdint>
#include <cstring>

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

// This file names the deprecated legacy error metrics
ACL_IMPL_DEPRECATION_WARNING_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// Interface for all skeletal error metrics.
	// An error metric is responsible for a few things:
	//    - applying local space rtm::vqvf transforms on top of rtm::vqvf base transforms (optional)
	//    - converting from rtm::vqvf into whatever transform type the metric uses (optional)
	//    - transforming local space transforms into object space
	//    - evaluating the error function
	//
	// Most functions require two implementations: with and without scale support.
	// This is entirely for performance reasons as most clips do not have any scale.
	//////////////////////////////////////////////////////////////////////////
	class ACL_DEPRECATED("Use the transform adapters and the transform metrics instead, to be removed in v2.3") itransform_error_metric
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
		// Returns whether or not the error metric uses a transform that isn't rtm::vqvf.
		// If this is the case, we need to convert from rtm::vqvf into the transform type
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
			// The input transforms in rtm::vqvf format to be converted.
			const rtm::vqvf* transforms;

			//////////////////////////////////////////////////////////////////////////
			// The number of transforms in the input and output buffers.
			uint32_t num_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The index in the clip of the sample these transforms come from.
			uint32_t sample_index;

			//////////////////////////////////////////////////////////////////////////
			// True if these transforms are being converted from lossy data.
			// False if these transforms are being converted from the original uncompressed data.
			bool is_lossy;

			//////////////////////////////////////////////////////////////////////////
			// ACL applies the additive base pose before the conversion and does not convert the base pose.
			// Thus this value is always false.
			bool is_additive_base;
		};

		//////////////////////////////////////////////////////////////////////////
		// Converts from rtm::vqvf into the transform type used by the error metric.
		// Called when 'needs_conversion' returns true.
		virtual void convert_transforms(const convert_transforms_args& args, void* out_transforms) const
		{
			(void)args;
			(void)out_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Converts from rtm::vqvf into the transform type used by the error metric.
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
			// The input local space transforms in rtm::vqvf format.
			const void* local_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The input base transforms in rtm::vqvf format.
			const void* base_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The number of transforms in the input and output buffers.
			uint32_t num_transforms;
		};

		//////////////////////////////////////////////////////////////////////////
		// Applies local space transforms on top of base transforms.
		// This is called when a clip has an additive base.
		// The input and output transforms are in rtm::vqvf format. ACL calls this function
		// before 'convert_transforms', thus the additive applies to the source transforms.
		virtual void apply_additive_to_base(const apply_additive_to_base_args& args, void* out_transforms) const
		{
			(void)args;
			(void)out_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Applies local space transforms on top of base transforms.
		// This is called when a clip has an additive base.
		// The input and output transforms are in rtm::vqvf format. ACL calls this function
		// before 'convert_transforms_no_scale', thus the additive applies to the source transforms.
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
	// Uses rtm::vqvf arithmetic for local and object space error.
	// Note that this can cause inaccuracy when dealing with shear/skew.
	//////////////////////////////////////////////////////////////////////////
	class ACL_DEPRECATED("Use the transform adapters and the transform metrics instead, to be removed in v2.3") vqvf_transform_error_metric : public itransform_error_metric
	{
	public:
		virtual const char* get_name() const override { return "vqvf_transform_error_metric"; }

		virtual size_t get_transform_size(bool has_scale) const override { (void)has_scale; return sizeof(rtm::vqvf); }
		virtual bool needs_conversion(bool has_scale) const override { (void)has_scale; return false; }
		virtual bool supports_bit_rate_algorithm_v2() const override { return true; }

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void local_to_object_space(const local_to_object_space_args& args, void* out_object_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const uint32_t* parent_transform_indices = args.parent_transform_indices;
			const rtm::vqvf* local_transforms_ = static_cast<const rtm::vqvf*>(args.local_transforms);
			rtm::vqvf* out_object_transforms_ = static_cast<rtm::vqvf*>(out_object_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
				const uint32_t parent_transform_index = parent_transform_indices[transform_index];

				rtm::vqvf obj_transform;
				if (parent_transform_index == k_invalid_track_index)
					obj_transform = local_transforms_[transform_index];	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
				else
					obj_transform = rtm::vqv_normalize(rtm::vqv_mul(local_transforms_[transform_index], out_object_transforms_[parent_transform_index]));

				out_object_transforms_[transform_index] = obj_transform;
			}
		}

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void local_to_object_space_no_scale(const local_to_object_space_args& args, void* out_object_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const uint32_t* parent_transform_indices = args.parent_transform_indices;
			const rtm::vqvf* local_transforms_ = static_cast<const rtm::vqvf*>(args.local_transforms);
			rtm::vqvf* out_object_transforms_ = static_cast<rtm::vqvf*>(out_object_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
				const uint32_t parent_transform_index = parent_transform_indices[transform_index];

				rtm::vqvf obj_transform;
				if (parent_transform_index == k_invalid_track_index)
					obj_transform = local_transforms_[transform_index];	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
				else
					obj_transform = rtm::vqv_normalize(rtm::vqv_mul_no_scale(local_transforms_[transform_index], out_object_transforms_[parent_transform_index]));

				out_object_transforms_[transform_index] = obj_transform;
			}
		}

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const override
		{
			const rtm::vqvf& raw_transform_ = *static_cast<const rtm::vqvf*>(args.transform0);
			const rtm::vqvf& lossy_transform_ = *static_cast<const rtm::vqvf*>(args.transform1);

			// Note that because we have scale, we must measure all three axes

#if defined(ACL_IMPL_USE_SOA_ERROR_METRIC)
			const rtm::vector4f vtx_xxx_ = args.shell_points_xxxx;
			const rtm::vector4f vtx_yyy_ = args.shell_points_yyyy;
			const rtm::vector4f vtx_zzz_ = args.shell_points_zzzz;

			rtm::vector4f raw_vtx_xxx_;
			rtm::vector4f raw_vtx_yyy_;
			rtm::vector4f raw_vtx_zzz_;
			acl_impl::vqv_mul_point3_x4(
				vtx_xxx_, vtx_yyy_, vtx_zzz_,
				raw_transform_,
				raw_vtx_xxx_, raw_vtx_yyy_, raw_vtx_zzz_);

			rtm::vector4f lossy_vtx_xxx_;
			rtm::vector4f lossy_vtx_yyy_;
			rtm::vector4f lossy_vtx_zzz_;
			acl_impl::vqv_mul_point3_x4(
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

			const rtm::vector4f raw_vtx0 = rtm::vqv_mul_point3(vtx0, raw_transform_);
			const rtm::vector4f raw_vtx1 = rtm::vqv_mul_point3(vtx1, raw_transform_);
			const rtm::vector4f raw_vtx2 = rtm::vqv_mul_point3(vtx2, raw_transform_);

			const rtm::vector4f lossy_vtx0 = rtm::vqv_mul_point3(vtx0, lossy_transform_);
			const rtm::vector4f lossy_vtx1 = rtm::vqv_mul_point3(vtx1, lossy_transform_);
			const rtm::vector4f lossy_vtx2 = rtm::vqv_mul_point3(vtx2, lossy_transform_);

			const rtm::scalarf vtx0_error = rtm::vector_distance_squared3_as_scalar(raw_vtx0, lossy_vtx0);
			const rtm::scalarf vtx1_error = rtm::vector_distance_squared3_as_scalar(raw_vtx1, lossy_vtx1);
			const rtm::scalarf vtx2_error = rtm::vector_distance_squared3_as_scalar(raw_vtx2, lossy_vtx2);

	#if defined(ACL_IMPL_USE_4_POINTS)
			const rtm::vector4f vtx3 = args.shell_point_mid;
			const rtm::vector4f raw_vtx3 = rtm::vqv_mul_point3(vtx3, raw_transform_);
			const rtm::vector4f lossy_vtx3 = rtm::vqv_mul_point3(vtx3, lossy_transform_);
			const rtm::scalarf vtx3_error = rtm::vector_distance_squared3_as_scalar(raw_vtx3, lossy_vtx3);
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), rtm::scalar_max(vtx2_error, vtx3_error));
	#else
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
	#endif
#endif
		}

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared_no_scale(const calculate_error_args& args) const override
		{
			const rtm::vqvf& raw_transform_ = *static_cast<const rtm::vqvf*>(args.transform0);
			const rtm::vqvf& lossy_transform_ = *static_cast<const rtm::vqvf*>(args.transform1);

#if defined(ACL_IMPL_USE_SOA_ERROR_METRIC)
			const rtm::vector4f vtx_xx__ = args.shell_points_xxxx;
			const rtm::vector4f vtx_yy__ = args.shell_points_yyyy;
			const rtm::vector4f vtx_zz__ = args.shell_points_zzzz;

			rtm::vector4f raw_vtx_xxx_;
			rtm::vector4f raw_vtx_yyy_;
			rtm::vector4f raw_vtx_zzz_;
			acl_impl::vqv_mul_point3_no_scale_x4(
				vtx_xx__, vtx_yy__, vtx_zz__,
				raw_transform_,
				raw_vtx_xxx_, raw_vtx_yyy_, raw_vtx_zzz_);

			rtm::vector4f lossy_vtx_xxx_;
			rtm::vector4f lossy_vtx_yyy_;
			rtm::vector4f lossy_vtx_zzz_;
			acl_impl::vqv_mul_point3_no_scale_x4(
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

			const rtm::vector4f raw_vtx0 = rtm::vqv_mul_point3_no_scale(vtx0, raw_transform_);
			const rtm::vector4f raw_vtx1 = rtm::vqv_mul_point3_no_scale(vtx1, raw_transform_);

			const rtm::vector4f lossy_vtx0 = rtm::vqv_mul_point3_no_scale(vtx0, lossy_transform_);
			const rtm::vector4f lossy_vtx1 = rtm::vqv_mul_point3_no_scale(vtx1, lossy_transform_);

			const rtm::scalarf vtx0_error = rtm::vector_distance_squared3_as_scalar(raw_vtx0, lossy_vtx0);
			const rtm::scalarf vtx1_error = rtm::vector_distance_squared3_as_scalar(raw_vtx1, lossy_vtx1);

	#if defined(ACL_IMPL_USE_4_POINTS)
			const rtm::vector4f vtx2 = args.shell_point_z;
			const rtm::vector4f vtx3 = args.shell_point_mid;
			const rtm::vector4f raw_vtx2 = rtm::vqv_mul_point3_no_scale(vtx2, raw_transform_);
			const rtm::vector4f raw_vtx3 = rtm::vqv_mul_point3_no_scale(vtx3, raw_transform_);
			const rtm::vector4f lossy_vtx2 = rtm::vqv_mul_point3_no_scale(vtx2, lossy_transform_);
			const rtm::vector4f lossy_vtx3 = rtm::vqv_mul_point3_no_scale(vtx3, lossy_transform_);
			const rtm::scalarf vtx2_error = rtm::vector_distance_squared3_as_scalar(raw_vtx2, lossy_vtx2);
			const rtm::scalarf vtx3_error = rtm::vector_distance_squared3_as_scalar(raw_vtx3, lossy_vtx3);
			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), rtm::scalar_max(vtx2_error, vtx3_error));
	#elif defined(ACL_IMPL_USE_3_POINTS_WITHOUT_SCALE)
			const rtm::vector4f vtx2 = args.shell_point_z;
			const rtm::vector4f raw_vtx2 = rtm::vqv_mul_point3_no_scale(vtx2, raw_transform_);
			const rtm::vector4f lossy_vtx2 = rtm::vqv_mul_point3_no_scale(vtx2, lossy_transform_);
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
	// vqvf approximation when scale is present.
	// This computes the error on the point most impacted by the lossy transform
	// which guarantees that all other points see the same or a lower error.
	//////////////////////////////////////////////////////////////////////////
	class ACL_DEPRECATED("Use the transform adapters and the transform metrics instead, to be removed in v2.3") vqvf_precise_transform_error_metric : public vqvf_transform_error_metric
	{
	public:
		virtual const char* get_name() const override { return "vqvf_precise_transform_error_metric"; }

		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared_no_scale(const calculate_error_args& args) const override
		{
			const rtm::vqvf& raw_transform_ = *static_cast<const rtm::vqvf*>(args.transform0);
			const rtm::vqvf& lossy_transform_ = *static_cast<const rtm::vqvf*>(args.transform1);

			// We use float64 for the added precision otherwise floating point noise in the rotation
			// can cause instability
			const rtm::vqvd raw_transform_d = rtm::vqv_normalize(rtm::vqv_cast(raw_transform_));
			const rtm::vqvd lossy_transform_d = rtm::vqv_normalize(rtm::vqv_cast(lossy_transform_));

			const double sphere_radius = (double)rtm::vector_get_x(args.shell_point_x);
			const double max_delta_transform_error_sq = acl_impl::calculate_rigid_error_squared_precise(raw_transform_d, lossy_transform_d, sphere_radius);

			return rtm::scalar_set((float)max_delta_transform_error_sq);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Uses a mix of rtm::vqvf and rtm::matrix3x4f arithmetic.
	// The local space error is always calculated with rtm::vqvf arithmetic.
	// The object space error is calculated with rtm::vqvf arithmetic if there is no scale
	// and with rtm::matrix3x4f arithmetic if there is scale.
	// Note that this can cause inaccuracy issues if there are very large or very small
	// scale values.
	//////////////////////////////////////////////////////////////////////////
	class ACL_DEPRECATED("Use the transform adapters and the transform metrics instead, to be removed in v2.3") vqvf_matrix3x4f_transform_error_metric : public vqvf_transform_error_metric
	{
	public:
		virtual const char* get_name() const override { return "vqvf_matrix3x4f_transform_error_metric"; }

		virtual size_t get_transform_size(bool has_scale) const override { return has_scale ? sizeof(rtm::matrix3x4f) : sizeof(rtm::vqvf); }
		virtual bool needs_conversion(bool has_scale) const override { return has_scale; }
		virtual bool supports_bit_rate_algorithm_v2() const override { return false; }

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void convert_transforms(const convert_transforms_args& args, void* out_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const rtm::vqvf* transforms_ = args.transforms;
			rtm::matrix3x4f* out_transforms_ = static_cast<rtm::matrix3x4f*>(out_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];

				const rtm::vqvf& transform_vqv = transforms_[transform_index];
				rtm::matrix3x4f transform_mtx = rtm::matrix_from_vqv(transform_vqv);

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
	// Uses rtm::vqvf arithmetic for local and object space error.
	// This error metric should be used whenever a clip is additive or relative.
	// Note that this can cause inaccuracy when dealing with shear/skew.
	//////////////////////////////////////////////////////////////////////////
	template<additive_clip_format8 additive_format>
	class ACL_DEPRECATED("Use the transform adapters and the transform metrics instead, to be removed in v2.3") additive_vqvf_transform_error_metric : public vqvf_transform_error_metric
	{
	public:
		virtual const char* get_name() const override
		{
			switch (additive_format)
			{
			default:
			case additive_clip_format8::none:			return "additive_vqvf_transform_error_metric<none>";
			case additive_clip_format8::relative:		return "additive_vqvf_transform_error_metric<relative>";
			case additive_clip_format8::additive0:		return "additive_vqvf_transform_error_metric<additive0>";
			case additive_clip_format8::additive1:		return "additive_vqvf_transform_error_metric<additive1>";
			}
		}

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void apply_additive_to_base(const apply_additive_to_base_args& args, void* out_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const rtm::vqvf* local_transforms_ = static_cast<const rtm::vqvf*>(args.local_transforms);
			const rtm::vqvf* base_transforms_ = static_cast<const rtm::vqvf*>(args.base_transforms);
			rtm::vqvf* out_transforms_ = static_cast<rtm::vqvf*>(out_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];

				const rtm::vqvf& local_transform = local_transforms_[transform_index];
				const rtm::vqvf& base_transform = base_transforms_[transform_index];
				const rtm::vqvf transform = acl::apply_additive_to_base(additive_format, base_transform, local_transform);

				out_transforms_[transform_index] = transform;
			}
		}

		virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void apply_additive_to_base_no_scale(const apply_additive_to_base_args& args, void* out_transforms) const override
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const rtm::vqvf* local_transforms_ = static_cast<const rtm::vqvf*>(args.local_transforms);
			const rtm::vqvf* base_transforms_ = static_cast<const rtm::vqvf*>(args.base_transforms);
			rtm::vqvf* out_transforms_ = static_cast<rtm::vqvf*>(out_transforms);

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];

				const rtm::vqvf& local_transform = local_transforms_[transform_index];
				const rtm::vqvf& base_transform = base_transforms_[transform_index];
				const rtm::vqvf transform = acl::apply_additive_to_base_no_scale(additive_format, base_transform, local_transform);

				out_transforms_[transform_index] = transform;
			}
		}
	};

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Returns true when the transform layout of the error metric depends on the scale.
		// For example, the matrix error metric uses a vqvf without scale and a matrix3x4f with scale.
		// When this is true, the caller must use the functions with scale, even when the clip has no scale.
		inline bool is_error_metric_layout_scale_dependent(const itransform_error_metric& error_metric)
		{
			return error_metric.get_transform_size(false) != error_metric.get_transform_size(true) ||
				error_metric.needs_conversion(false) != error_metric.needs_conversion(true);
		}

		//////////////////////////////////////////////////////////////////////////
		// Converts one transform with the functions with or without scale of an error metric.
		// The error metric reads and writes the transform at its index in a full pose: it can use
		// the transform index to read its own data. Thus we keep a full pose for the input and the output.
		class single_transform_converter
		{
		public:
			single_transform_converter(iallocator& allocator, const itransform_error_metric& error_metric, bool has_scale, uint32_t num_transforms)
				: m_allocator(allocator)
				, m_error_metric(error_metric)
				, m_transform_size(error_metric.get_transform_size(has_scale))
				, m_num_transforms(num_transforms)
				, m_has_scale(has_scale)
			{
				if (error_metric.needs_conversion(has_scale))
				{
					m_pose = allocate_type_array<rtm::vqvf>(allocator, num_transforms);
					m_pose_converted = allocate_type_array_aligned<uint8_t>(allocator, m_transform_size * num_transforms, 64);
				}
			}

			~single_transform_converter()
			{
				deallocate_type_array(m_allocator, m_pose, m_num_transforms);
				deallocate_type_array(m_allocator, m_pose_converted, m_transform_size * m_num_transforms);
			}

			single_transform_converter(const single_transform_converter&) = delete;
			single_transform_converter& operator=(const single_transform_converter&) = delete;

			// Writes the converted transform in 'out_transform'
			// 'sample_index' is the index of the sample in the clip
			void convert(const rtm::vqvf& transform, uint32_t transform_index, uint32_t sample_index, bool is_lossy, void* out_transform) const
			{
				ACL_ASSERT(m_pose != nullptr, "The error metric does not need a conversion");
				ACL_ASSERT(transform_index < m_num_transforms, "Invalid transform index");

				m_pose[transform_index] = transform;

				itransform_error_metric::convert_transforms_args args;
				args.dirty_transform_indices = &transform_index;
				args.num_dirty_transforms = 1;
				args.transforms = m_pose;
				args.num_transforms = m_num_transforms;
				args.sample_index = sample_index;
				args.is_lossy = is_lossy;
				args.is_additive_base = false;

				if (m_has_scale)
					m_error_metric.convert_transforms(args, m_pose_converted);
				else
					m_error_metric.convert_transforms_no_scale(args, m_pose_converted);

				std::memcpy(out_transform, m_pose_converted + (transform_index * m_transform_size), m_transform_size);
			}

		private:
			iallocator& m_allocator;
			const itransform_error_metric& m_error_metric;
			size_t m_transform_size;
			uint32_t m_num_transforms;
			bool m_has_scale;

			// We write them in the const function
			rtm::vqvf* m_pose = nullptr;
			uint8_t* m_pose_converted = nullptr;
		};
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_DEPRECATION_WARNING_POP

ACL_IMPL_FILE_PRAGMA_POP
