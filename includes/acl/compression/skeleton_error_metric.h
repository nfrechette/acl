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

#include "acl/core/additive_utils.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/hash.h"
#include "acl/compression/skeleton.h"

#include <rtm/matrix3x4f.h>
#include <rtm/qvvf.h>
#include <rtm/scalarf.h>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	// Base class for all skeletal error metrics
	class ISkeletalErrorMetric
	{
	public:
		virtual ~ISkeletalErrorMetric() {}

		virtual const char* get_name() const = 0;
		virtual uint32_t get_hash() const = 0;

		virtual float calculate_local_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const = 0;
		virtual float calculate_local_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const = 0;

		virtual float calculate_object_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const = 0;
		virtual float calculate_object_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const = 0;
	};

	// Uses a mix of Transform_32 and AffineMatrix_32 arithmetic.
	// The local space error is always calculated with Transform_32 arithmetic.
	// The object space error is calculated with Transform_32 arithmetic if there is no scale
	// and with AffineMatrix_32 arithmetic if there is scale.
	// Note that this can cause inaccuracy issues if there are very large or very small
	// scale values.
	class TransformMatrixErrorMetric final : public ISkeletalErrorMetric
	{
	public:
		virtual const char* get_name() const override { return "TransformMatrixErrorMetric"; }
		virtual uint32_t get_hash() const override { return hash32("TransformMatrixErrorMetric"); }

		virtual float calculate_local_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			const RigidBone& bone = skeleton.get_bone(bone_index);

			// Note that because we have scale, we must measure all three axes
			const rtm::vector4f vtx0 = rtm::vector_set(bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, bone.vertex_distance, 0.0F);
			const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, bone.vertex_distance);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3(vtx0, lossy_local_pose[bone_index]);
			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);

			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3(vtx1, lossy_local_pose[bone_index]);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3(vtx2, lossy_local_pose[bone_index]);
			const float vtx2_error = rtm::vector_distance3(raw_vtx2, lossy_vtx2);

			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
		}

		virtual float calculate_local_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			const RigidBone& bone = skeleton.get_bone(bone_index);

			const rtm::vector4f vtx0 = rtm::vector_set(bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, bone.vertex_distance, 0.0F);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, lossy_local_pose[bone_index]);
			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);

			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, lossy_local_pose[bone_index]);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			return rtm::scalar_max(vtx0_error, vtx1_error);
		}

		virtual float calculate_object_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			rtm::matrix3x4f raw_obj_mtx = rtm::matrix_from_qvv(raw_local_pose[0]);
			rtm::matrix3x4f lossy_obj_mtx = rtm::matrix_from_qvv(lossy_local_pose[0]);

			const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
			auto chain_bone_it = ++bone_chain.begin();	// Skip root bone
			const auto chain_bone_end = bone_chain.end();
			for (; chain_bone_it != chain_bone_end; ++chain_bone_it)
			{
				const uint16_t chain_bone_index = *chain_bone_it;
				raw_obj_mtx = rtm::matrix_mul(rtm::matrix_from_qvv(raw_local_pose[chain_bone_index]), raw_obj_mtx);
				lossy_obj_mtx = rtm::matrix_mul(rtm::matrix_from_qvv(lossy_local_pose[chain_bone_index]), lossy_obj_mtx);
			}

			const RigidBone& target_bone = skeleton.get_bone(bone_index);

			// Note that because we have scale, we must measure all three axes
			const rtm::vector4f vtx0 = rtm::vector_set(target_bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, target_bone.vertex_distance, 0.0F);
			const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, target_bone.vertex_distance);

			const rtm::vector4f raw_vtx0 = rtm::matrix_mul_point3(vtx0, raw_obj_mtx);
			const rtm::vector4f raw_vtx1 = rtm::matrix_mul_point3(vtx1, raw_obj_mtx);
			const rtm::vector4f raw_vtx2 = rtm::matrix_mul_point3(vtx2, raw_obj_mtx);
			const rtm::vector4f lossy_vtx0 = rtm::matrix_mul_point3(vtx0, lossy_obj_mtx);
			const rtm::vector4f lossy_vtx1 = rtm::matrix_mul_point3(vtx1, lossy_obj_mtx);
			const rtm::vector4f lossy_vtx2 = rtm::matrix_mul_point3(vtx2, lossy_obj_mtx);

			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);
			const float vtx2_error = rtm::vector_distance3(raw_vtx2, lossy_vtx2);

			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
		}

		virtual float calculate_object_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			rtm::qvvf raw_obj_transform = raw_local_pose[0];
			rtm::qvvf lossy_obj_transform = lossy_local_pose[0];

			const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
			auto chain_bone_it = ++bone_chain.begin();	// Skip root bone
			const auto chain_bone_end = bone_chain.end();
			for (; chain_bone_it != chain_bone_end; ++chain_bone_it)
			{
				const uint16_t chain_bone_index = *chain_bone_it;
				raw_obj_transform = rtm::qvv_mul_no_scale(raw_local_pose[chain_bone_index], raw_obj_transform);
				lossy_obj_transform = rtm::qvv_mul_no_scale(lossy_local_pose[chain_bone_index], lossy_obj_transform);
			}

			const RigidBone& target_bone = skeleton.get_bone(bone_index);

			const rtm::vector4f vtx0 = rtm::vector_set(target_bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, target_bone.vertex_distance, 0.0F);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, raw_obj_transform);
			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, raw_obj_transform);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, lossy_obj_transform);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, lossy_obj_transform);

			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			return rtm::scalar_max(vtx0_error, vtx1_error);
		}
	};

	// Uses Transform_32 arithmetic for local and object space error.
	// Note that this can cause inaccuracy when dealing with shear/skew.
	class TransformErrorMetric final : public ISkeletalErrorMetric
	{
	public:
		virtual const char* get_name() const override { return "TransformErrorMetric"; }
		virtual uint32_t get_hash() const override { return hash32("TransformErrorMetric"); }

		virtual float calculate_local_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			const RigidBone& bone = skeleton.get_bone(bone_index);

			// Note that because we have scale, we must measure all three axes
			const rtm::vector4f vtx0 = rtm::vector_set(bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, bone.vertex_distance, 0.0F);
			const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, bone.vertex_distance);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3(vtx0, lossy_local_pose[bone_index]);
			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);

			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3(vtx1, lossy_local_pose[bone_index]);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3(vtx2, lossy_local_pose[bone_index]);
			const float vtx2_error = rtm::vector_distance3(raw_vtx2, lossy_vtx2);

			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
		}

		virtual float calculate_local_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			const RigidBone& bone = skeleton.get_bone(bone_index);

			const rtm::vector4f vtx0 = rtm::vector_set(bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, bone.vertex_distance, 0.0F);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, lossy_local_pose[bone_index]);
			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);

			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, raw_local_pose[bone_index]);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, lossy_local_pose[bone_index]);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			return rtm::scalar_max(vtx0_error, vtx1_error);
		}

		virtual float calculate_object_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			rtm::qvvf raw_obj_transform = raw_local_pose[0];
			rtm::qvvf lossy_obj_transform = lossy_local_pose[0];

			const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
			auto chain_bone_it = ++bone_chain.begin();	// Skip root bone
			const auto chain_bone_end = bone_chain.end();
			for (; chain_bone_it != chain_bone_end; ++chain_bone_it)
			{
				const uint16_t chain_bone_index = *chain_bone_it;
				raw_obj_transform = rtm::qvv_mul(raw_local_pose[chain_bone_index], raw_obj_transform);
				lossy_obj_transform = rtm::qvv_mul(lossy_local_pose[chain_bone_index], lossy_obj_transform);
			}

			const RigidBone& target_bone = skeleton.get_bone(bone_index);

			// Note that because we have scale, we must measure all three axes
			const rtm::vector4f vtx0 = rtm::vector_set(target_bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, target_bone.vertex_distance, 0.0F);
			const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, target_bone.vertex_distance);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_obj_transform);
			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_obj_transform);
			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_obj_transform);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3(vtx0, lossy_obj_transform);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3(vtx1, lossy_obj_transform);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3(vtx2, lossy_obj_transform);

			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);
			const float vtx2_error = rtm::vector_distance3(raw_vtx2, lossy_vtx2);

			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
		}

		virtual float calculate_object_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);
			(void)base_local_pose;

			rtm::qvvf raw_obj_transform = raw_local_pose[0];
			rtm::qvvf lossy_obj_transform = lossy_local_pose[0];

			const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
			auto chain_bone_it = ++bone_chain.begin();	// Skip root bone
			const auto chain_bone_end = bone_chain.end();
			for (; chain_bone_it != chain_bone_end; ++chain_bone_it)
			{
				const uint16_t chain_bone_index = *chain_bone_it;
				raw_obj_transform = rtm::qvv_mul_no_scale(raw_local_pose[chain_bone_index], raw_obj_transform);
				lossy_obj_transform = rtm::qvv_mul_no_scale(lossy_local_pose[chain_bone_index], lossy_obj_transform);
			}

			const RigidBone& target_bone = skeleton.get_bone(bone_index);

			const rtm::vector4f vtx0 = rtm::vector_set(target_bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, target_bone.vertex_distance, 0.0F);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, raw_obj_transform);
			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, raw_obj_transform);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, lossy_obj_transform);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, lossy_obj_transform);

			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			return rtm::scalar_max(vtx0_error, vtx1_error);
		}
	};

	// Uses Transform_32 arithmetic for local and object space error.
	// This error metric should be used whenever a clip is additive or relative.
	// Note that this can cause inaccuracy when dealing with shear/skew.
	template<AdditiveClipFormat8 additive_format>
	class AdditiveTransformErrorMetric final : public ISkeletalErrorMetric
	{
	public:
		virtual const char* get_name() const override
		{
			switch (additive_format)
			{
			default:
			case AdditiveClipFormat8::None:			return "AdditiveTransformErrorMetric<None>";
			case AdditiveClipFormat8::Relative:		return "AdditiveTransformErrorMetric<Relative>";
			case AdditiveClipFormat8::Additive0:	return "AdditiveTransformErrorMetric<Additive0>";
			case AdditiveClipFormat8::Additive1:	return "AdditiveTransformErrorMetric<Additive1>";
			}
		}

		virtual uint32_t get_hash() const override { return hash32(get_name()); }

		virtual float calculate_local_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

			const RigidBone& bone = skeleton.get_bone(bone_index);

			// Note that because we have scale, we must measure all three axes
			const rtm::vector4f vtx0 = rtm::vector_set(bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, bone.vertex_distance, 0.0F);
			const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, bone.vertex_distance);

			const rtm::qvvf raw_transform = apply_additive_to_base(additive_format, base_local_pose[bone_index], raw_local_pose[bone_index]);
			const rtm::qvvf lossy_transform = apply_additive_to_base(additive_format, base_local_pose[bone_index], lossy_local_pose[bone_index]);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_transform);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3(vtx0, lossy_transform);
			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);

			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_transform);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3(vtx1, lossy_transform);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_transform);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3(vtx2, lossy_transform);
			const float vtx2_error = rtm::vector_distance3(raw_vtx2, lossy_vtx2);

			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
		}

		virtual float calculate_local_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

			const RigidBone& bone = skeleton.get_bone(bone_index);

			const rtm::vector4f vtx0 = rtm::vector_set(bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, bone.vertex_distance, 0.0F);

			const rtm::qvvf raw_transform = apply_additive_to_base_no_scale(additive_format, base_local_pose[bone_index], raw_local_pose[bone_index]);
			const rtm::qvvf lossy_transform = apply_additive_to_base_no_scale(additive_format, base_local_pose[bone_index], lossy_local_pose[bone_index]);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, raw_transform);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, lossy_transform);
			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);

			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, raw_transform);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, lossy_transform);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			return rtm::scalar_max(vtx0_error, vtx1_error);
		}

		virtual float calculate_object_bone_error(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

			rtm::qvvf raw_obj_transform = apply_additive_to_base(additive_format, base_local_pose[0], raw_local_pose[0]);
			rtm::qvvf lossy_obj_transform = apply_additive_to_base(additive_format, base_local_pose[0], lossy_local_pose[0]);

			const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
			auto chain_bone_it = ++bone_chain.begin();	// Skip root bone
			const auto chain_bone_end = bone_chain.end();
			for (; chain_bone_it != chain_bone_end; ++chain_bone_it)
			{
				const uint16_t chain_bone_index = *chain_bone_it;

				raw_obj_transform = rtm::qvv_mul(apply_additive_to_base(additive_format, base_local_pose[chain_bone_index], raw_local_pose[chain_bone_index]), raw_obj_transform);
				lossy_obj_transform = rtm::qvv_mul(apply_additive_to_base(additive_format, base_local_pose[chain_bone_index], lossy_local_pose[chain_bone_index]), lossy_obj_transform);
			}

			const RigidBone& target_bone = skeleton.get_bone(bone_index);

			// Note that because we have scale, we must measure all three axes
			const rtm::vector4f vtx0 = rtm::vector_set(target_bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, target_bone.vertex_distance, 0.0F);
			const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, target_bone.vertex_distance);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_obj_transform);
			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_obj_transform);
			const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_obj_transform);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3(vtx0, lossy_obj_transform);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3(vtx1, lossy_obj_transform);
			const rtm::vector4f lossy_vtx2 = rtm::qvv_mul_point3(vtx2, lossy_obj_transform);

			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);
			const float vtx2_error = rtm::vector_distance3(raw_vtx2, lossy_vtx2);

			return rtm::scalar_max(rtm::scalar_max(vtx0_error, vtx1_error), vtx2_error);
		}

		virtual float calculate_object_bone_error_no_scale(const RigidSkeleton& skeleton, const rtm::qvvf* raw_local_pose, const rtm::qvvf* base_local_pose, const rtm::qvvf* lossy_local_pose, uint16_t bone_index) const override
		{
			ACL_ASSERT(bone_index < skeleton.get_num_bones(), "Invalid bone index: %u", bone_index);

			rtm::qvvf raw_obj_transform = apply_additive_to_base_no_scale(additive_format, base_local_pose[0], raw_local_pose[0]);
			rtm::qvvf lossy_obj_transform = apply_additive_to_base_no_scale(additive_format, base_local_pose[0], lossy_local_pose[0]);

			const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);
			auto chain_bone_it = ++bone_chain.begin();	// Skip root bone
			const auto chain_bone_end = bone_chain.end();
			for (; chain_bone_it != chain_bone_end; ++chain_bone_it)
			{
				const uint16_t chain_bone_index = *chain_bone_it;

				raw_obj_transform = rtm::qvv_mul_no_scale(apply_additive_to_base_no_scale(additive_format, base_local_pose[chain_bone_index], raw_local_pose[chain_bone_index]), raw_obj_transform);
				lossy_obj_transform = rtm::qvv_mul_no_scale(apply_additive_to_base_no_scale(additive_format, base_local_pose[chain_bone_index], lossy_local_pose[chain_bone_index]), lossy_obj_transform);
			}

			const RigidBone& target_bone = skeleton.get_bone(bone_index);

			const rtm::vector4f vtx0 = rtm::vector_set(target_bone.vertex_distance, 0.0F, 0.0F);
			const rtm::vector4f vtx1 = rtm::vector_set(0.0F, target_bone.vertex_distance, 0.0F);

			const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, raw_obj_transform);
			const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, raw_obj_transform);
			const rtm::vector4f lossy_vtx0 = rtm::qvv_mul_point3_no_scale(vtx0, lossy_obj_transform);
			const rtm::vector4f lossy_vtx1 = rtm::qvv_mul_point3_no_scale(vtx1, lossy_obj_transform);

			const float vtx0_error = rtm::vector_distance3(raw_vtx0, lossy_vtx0);
			const float vtx1_error = rtm::vector_distance3(raw_vtx1, lossy_vtx1);

			return rtm::scalar_max(vtx0_error, vtx1_error);
		}
	};
}

ACL_IMPL_FILE_PRAGMA_POP
