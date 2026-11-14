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
#include "acl/core/iterator.h"
#include "acl/core/memory_utils.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/transform_adapters.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/transform_metrics.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/rigid_shell_utils.h"
#include "acl/compression/impl/segment_context.h"
#include "acl/compression/impl/topology_metadata.h"

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

// This file names the deprecated legacy error metrics
ACL_IMPL_DEPRECATION_WARNING_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// The bit rate algorithm v2 composes and measures transforms through a policy.
		// A policy holds the transform math and the error metric of a segment.
		// Each policy has the same functions:
		//    - 'error_args_type': the arguments of the error metric
		//    - 'init_error_args(args, shell_distance)'
		//    - 'get_transform_size()', 'uses_cache()', 'needs_chain_child()', and 'needs_conversion()'
		//    - 'local_to_object_space(..)': converts a local space pose and takes it into object space
		//    - 'compute_shell_distances(..)': finds the rigid shell of each transform from the object space poses
		//    - 'measure_cached(..)': measures the error with the cached transforms
		//    - 'measure_chain(..)': measures the error with the transform chains
		//    - 'update_cached(..)' and 'store_local(..)': update the cached transforms after optimization
		//
		// The functions use the transforms through pointers: the transform type of a custom
		// transform adapter is known only at runtime. With a built-in type, the compiler
		// removes the pointers and the virtual function calls.
		//
		// When the policy uses the cache ('uses_cache()' is true), the cached transforms hold:
		//    - for a transform that is not optimized: its object space transform
		//    - for a critical transform: its transform relative to the parent of the last transform optimized in its chain
		// In the mesh rotation space, the cached transforms of the critical transforms use another form, see 'mesh_rotation_cache_t'.
		// When the policy uses the transform chains, the cached transforms hold:
		//    - for a transform that is not optimized: its object space transform
		//    - for a transform that is optimized: its local space transform
		// See 'find_optimal_bit_rates_v2' for details.
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// The cache of the mesh rotation space, for VQ and VQS transforms.
		//
		// In the mesh rotation space, the rotation of a transform is its object rotation. When the rotation of a
		// transform X changes, its children move but they do not turn. The subtree of X is not rigid with respect
		// to X, thus the transform of a descendant relative to X changes with the rotation of X. We cannot cache
		// relative transforms as in local space.
		//
		// The search optimizes the leaves first. When it optimizes X, each descendant D of X already has its final
		// value. Let A be the child of X on the chain from X to D. With VQS transforms (a uniform scale), the
		// mesh rotation product gives the object transform of D from the object transform of X:
		//    D.rotation    = q_D
		//    D.translation = X.translation + X.scale * (rotate(t_A, X.rotation) + K_D)
		//    D.scale       = X.scale * s_D
		// where t_A is the local translation of A, q_D is the rotation of D, and K_D and s_D use only the transforms
		// below A. When D is A, K_D is zero and s_D is the scale of A. VQ transforms use a scale of 1.0.
		//
		// The cached transform of a critical transform D holds:
		//    - when D is the last transform optimized in its chain (D is A): its local transform (q_A, t_A, s_A)
		//    - otherwise: q_D, K_D in the translation, and s_D in the scale
		// When the search selects the bit rate of X, it adds X into the cached transform of each descendant D,
		// from the local transform of X (q_X, t_X, s_X):
		//    K_D' = s_X * (rotate(t_A, q_X) + K_D)
		//    s_D' = s_X * s_D
		// Then the cached transform of X holds its local transform: X is the new A of its descendants.
		// The search measures each transform in constant time, as in local space.
		// VQV transforms need a 3x3 matrix for K_D, and the shear of VQM transforms turns with the rotations:
		// they use the transform chains.
		//////////////////////////////////////////////////////////////////////////
		template<class transform_math_t>
		struct mesh_rotation_cache_t
		{
			using transform_type = typename transform_math_t::transform_type;

			static constexpr bool is_supported() { return false; }

			static transform_type RTM_SIMD_CALL measure(const transform_type& transform_object, const transform_type& chain_child, const transform_type& measured, bool is_chain_child)
			{
				(void)chain_child;
				(void)measured;
				(void)is_chain_child;
				ACL_ASSERT(false, "The transform math does not support the mesh rotation cache");
				return transform_object;
			}

			static transform_type RTM_SIMD_CALL update(const transform_type& transform_local, const transform_type& chain_child, const transform_type& measured, bool is_chain_child)
			{
				(void)transform_local;
				(void)chain_child;
				(void)is_chain_child;
				ACL_ASSERT(false, "The transform math does not support the mesh rotation cache");
				return measured;
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// The cache of the mesh rotation space for VQ transforms: we do not use the scale
		template<>
		struct mesh_rotation_cache_t<vq_transform_math>
		{
			static constexpr bool is_supported() { return true; }

			// Returns rotate(t_A, rotation) + K_D
			static rtm::vector4f RTM_SIMD_CALL get_offset(rtm::quatf_arg0 rotation, const rtm::vqvf& chain_child, const rtm::vqvf& measured, bool is_chain_child)
			{
				const rtm::vector4f chain_child_offset = rtm::quat_mul_vector3(chain_child.translation, rotation);
				return is_chain_child ? chain_child_offset : rtm::vector_add(chain_child_offset, measured.translation);
			}

			// Returns the object transform of the measured transform from the object transform of the transform being optimized
			static rtm::vqvf RTM_SIMD_CALL measure(const rtm::vqvf& transform_object, const rtm::vqvf& chain_child, const rtm::vqvf& measured, bool is_chain_child)
			{
				const rtm::vector4f offset = get_offset(transform_object.rotation, chain_child, measured, is_chain_child);
				return rtm::vqv_set(measured.rotation, rtm::vector_add(transform_object.translation, offset), rtm::vector_set(1.0F));
			}

			// Returns the new cached transform of the measured transform from the local transform of the transform being optimized
			static rtm::vqvf RTM_SIMD_CALL update(const rtm::vqvf& transform_local, const rtm::vqvf& chain_child, const rtm::vqvf& measured, bool is_chain_child)
			{
				const rtm::vector4f offset = get_offset(transform_local.rotation, chain_child, measured, is_chain_child);
				return rtm::vqv_set(measured.rotation, offset, rtm::vector_set(1.0F));
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// The cache of the mesh rotation space for VQS transforms: the W component holds the uniform scale
		template<>
		struct mesh_rotation_cache_t<vqs_transform_math>
		{
			static constexpr bool is_supported() { return true; }

			// Returns rotate(t_A, rotation) + K_D with s_D in the W component
			static rtm::vector4f RTM_SIMD_CALL get_offset(rtm::quatf_arg0 rotation, const rtm::vqsf& chain_child, const rtm::vqsf& measured, bool is_chain_child)
			{
				const rtm::vector4f chain_child_offset = rtm::quat_mul_vector3(chain_child.translation_scale, rotation);
				const rtm::vector4f offset = is_chain_child ? chain_child_offset : rtm::vector_add(chain_child_offset, measured.translation_scale);
				return rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(offset, measured.translation_scale);
			}

			// Returns the object transform of the measured transform from the object transform of the transform being optimized
			static rtm::vqsf RTM_SIMD_CALL measure(const rtm::vqsf& transform_object, const rtm::vqsf& chain_child, const rtm::vqsf& measured, bool is_chain_child)
			{
				const rtm::vector4f scale = rtm::vector_dup_w(transform_object.translation_scale);
				const rtm::vector4f offset = get_offset(transform_object.rotation, chain_child, measured, is_chain_child);

				// The translation is X.translation + X.scale * offset, the scale is X.scale * s_D
				const rtm::vector4f translation = rtm::vector_mul_add(offset, scale, transform_object.translation_scale);
				const rtm::vector4f scale_w = rtm::vector_mul(offset, scale);
				return rtm::vqsf{ measured.rotation, rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(translation, scale_w) };
			}

			// Returns the new cached transform of the measured transform from the local transform of the transform being optimized
			static rtm::vqsf RTM_SIMD_CALL update(const rtm::vqsf& transform_local, const rtm::vqsf& chain_child, const rtm::vqsf& measured, bool is_chain_child)
			{
				const rtm::vector4f scale = rtm::vector_dup_w(transform_local.translation_scale);
				const rtm::vector4f offset = get_offset(transform_local.rotation, chain_child, measured, is_chain_child);
				return rtm::vqsf{ measured.rotation, rtm::vector_mul(offset, scale) };
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// The precise transform metric, for VQ and VQS transforms
		struct v2_precise_metric
		{
			using error_args_type = itransform_metric::calculate_error_args;

			static void init_error_args(error_args_type& args, float shell_distance)
			{
				args.construct_sphere_shell(shell_distance);
				args.adapter = nullptr;
				args.transform0 = nullptr;
				args.transform1 = nullptr;
			}

			// The VQ transforms use rtm::vqvf and we do not use the scale
			static rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const error_args_type& args, const rtm::vqvf& raw_transform, const rtm::vqvf& lossy_transform)
			{
				return rtm::scalar_set(float(calculate_vq_error_squared_precise(raw_transform, lossy_transform, double(args.shell_distance))));
			}

			static rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const error_args_type& args, const rtm::vqsf& raw_transform, const rtm::vqsf& lossy_transform)
			{
				return rtm::scalar_set(float(calculate_vqs_error_squared_precise(raw_transform, lossy_transform, double(args.shell_distance))));
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// The shell transform metric, for a built-in transform type
		template<class transform_math_t>
		struct v2_shell_metric
		{
			using error_args_type = itransform_metric::calculate_error_args;
			using transform_type = typename transform_math_t::transform_type;

			static void init_error_args(error_args_type& args, float shell_distance)
			{
				args.construct_sphere_shell(shell_distance);
				args.adapter = nullptr;
				args.transform0 = nullptr;
				args.transform1 = nullptr;
			}

			static rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const error_args_type& args, const transform_type& raw_transform, const transform_type& lossy_transform)
			{
				rtm::vector4f raw_points_xxxx;
				rtm::vector4f raw_points_yyyy;
				rtm::vector4f raw_points_zzzz;
				transform_math_t::mul_point3_x4(
					args.shell_points_xxxx, args.shell_points_yyyy, args.shell_points_zzzz,
					raw_transform,
					raw_points_xxxx, raw_points_yyyy, raw_points_zzzz);

				rtm::vector4f lossy_points_xxxx;
				rtm::vector4f lossy_points_yyyy;
				rtm::vector4f lossy_points_zzzz;
				transform_math_t::mul_point3_x4(
					args.shell_points_xxxx, args.shell_points_yyyy, args.shell_points_zzzz,
					lossy_transform,
					lossy_points_xxxx, lossy_points_yyyy, lossy_points_zzzz);

				return calculate_shell_error_squared(
					raw_points_xxxx, raw_points_yyyy, raw_points_zzzz,
					lossy_points_xxxx, lossy_points_yyyy, lossy_points_zzzz,
					transform_math_t::get_type() == transform_runtime_type_t::vqf);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// A legacy error metric that supports the bit rate algorithm v2.
		// It measures with the functions with scale, or with the functions without scale.
		template<bool use_scale_functions>
		struct v2_legacy_metric
		{
			using error_args_type = itransform_error_metric::calculate_error_args;

			const itransform_error_metric* error_metric;

			static void init_error_args(error_args_type& args, float shell_distance)
			{
				args.construct_sphere_shell(shell_distance);
				args.transform0 = nullptr;
				args.transform1 = nullptr;
			}

			template<class transform_type>
			rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const error_args_type& args, const transform_type& raw_transform, const transform_type& lossy_transform) const
			{
				error_args_type args_ = args;
				args_.transform0 = &raw_transform;
				args_.transform1 = &lossy_transform;

				return use_scale_functions ? error_metric->calculate_error_squared(args_) : error_metric->calculate_error_squared_no_scale(args_);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// Reads VQ object space transforms for 'compute_segment_shell_distances_with_reader'
		// VQ math does not use the scale: the largest scale is always 1.0
		struct vq_shell_transform_reader
		{
			const rtm::vqvf* object_transforms;
			uint32_t num_transforms;

			rtm::vector4f RTM_SIMD_CALL get_translation(uint32_t sample_index, uint32_t transform_index) const
			{
				return object_transforms[(sample_index * num_transforms) + transform_index].translation;
			}

			rtm::scalarf RTM_SIMD_CALL get_largest_scale(uint32_t sample_index, uint32_t transform_index) const
			{
				(void)sample_index;
				(void)transform_index;
				return rtm::scalar_set(1.0F);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// A policy for a built-in transform type and a metric that the compiler knows.
		// There is no virtual function call, except in the legacy error metrics.
		template<class transform_math_t, class metric_t>
		class v2_static_policy
		{
		public:
			using transform_math = transform_math_t;
			using transform_type = typename transform_math_t::transform_type;
			using error_args_type = typename metric_t::error_args_type;

			// In mesh rotation space, the policy composes the transforms with the mesh rotation product
			// VQ and VQS transforms use the mesh rotation cache, the other types use the transform chains
			explicit v2_static_policy(const metric_t& metric, bool is_mesh_rotation = false)
				: m_metric(metric)
				, m_is_mesh_rotation(is_mesh_rotation)
			{
				ACL_ASSERT(!is_mesh_rotation || transform_math_t::supports_mesh_rotation(), "The transform math does not support the mesh rotation space");
			}

			void init_error_args(error_args_type& args, float shell_distance) const { m_metric.init_error_args(args, shell_distance); }

			static constexpr size_t get_transform_size() { return sizeof(transform_type); }

			// Returns true when the search uses the cached transforms, false when it uses the transform chains
			bool uses_cache() const { return m_is_mesh_rotation ? mesh_rotation_cache_t<transform_math_t>::is_supported() : transform_math_t::is_associative(); }

			// Returns true when 'measure_cached' and 'update_cached' use the cached transform of the chain child
			bool needs_chain_child() const { return m_is_mesh_rotation && mesh_rotation_cache_t<transform_math_t>::is_supported(); }
			static constexpr bool needs_conversion() { return !std::is_same<transform_type, rtm::vqvf>::value; }

			// The output can be the input when the transform type is rtm::vqvf
			// The built-in transform types do not use the sample index or whether or not the pose is lossy
			void local_to_object_space(const clip_topology_t& topology, const rtm::vqvf* local_pose, void* out_object_pose, uint32_t sample_index, bool is_lossy) const
			{
				(void)sample_index;
				(void)is_lossy;
				transform_type* object_pose = static_cast<transform_type*>(out_object_pose);

				for (const uint32_t transform_index : topology.roots_first_iterator())
				{
					const transform_type local_transform = transform_math_t::from_vqv(local_pose[transform_index]);

					const uint32_t parent_transform_index = topology.transforms[transform_index].parent_index;
					if (parent_transform_index != k_invalid_track_index)
						object_pose[transform_index] = mul_with_parent(local_transform, object_pose[parent_transform_index]);
					else
						object_pose[transform_index] = local_transform;	// The root has no parent: its local and object space transforms are equal
				}
			}

			void compute_shell_distances(const segment_context& segment, const void* object_poses, rigid_shell_metadata_t* out_shell_metadata) const
			{
				compute_shell_distances_impl(std::integral_constant<bool, std::is_same<transform_math_t, vq_transform_math>::value>(), segment, static_cast<const transform_type*>(object_poses), out_shell_metadata);
			}

			// result = measured_to_transform * transform_being_optimized * parent_transform_to_root
			// A null 'parent_transform_to_root' or 'measured_to_transform' is the identity
			// In the mesh rotation space, 'chain_child' is the cached transform of the child of the transform being optimized
			// on the chain to the measured transform, see 'mesh_rotation_cache_t'. It is null when 'measured_to_transform' is null.
			rtm::scalarf RTM_SIMD_CALL measure_cached(
				const error_args_type& args, const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index,
				const void* parent_transform_to_root, const void* measured_to_transform, const void* chain_child, const void* measured_to_root_raw) const
			{
				(void)transform_index;
				(void)sample_index;
				const transform_type local_transform = transform_math_t::from_vqv(local_transform_vqv);

				const transform_type parent_transform_to_root_ = parent_transform_to_root != nullptr ? *static_cast<const transform_type*>(parent_transform_to_root) : transform_math_t::identity();

				if (m_is_mesh_rotation)
				{
					const transform_type transform_to_root = transform_math_t::mul_mesh_rotation(local_transform, parent_transform_to_root_);
					if (measured_to_transform == nullptr)
						return m_metric.calculate_error_squared(args, *static_cast<const transform_type*>(measured_to_root_raw), transform_to_root);

					ACL_ASSERT(chain_child != nullptr, "The mesh rotation cache needs the chain child");
					const transform_type measured_to_root = mesh_rotation_cache_t<transform_math_t>::measure(
						transform_to_root, *static_cast<const transform_type*>(chain_child), *static_cast<const transform_type*>(measured_to_transform), measured_to_transform == chain_child);

					return m_metric.calculate_error_squared(args, *static_cast<const transform_type*>(measured_to_root_raw), measured_to_root);
				}

				const transform_type transform_to_root = transform_math_t::mul(local_transform, parent_transform_to_root_);

				const transform_type measured_to_transform_ = measured_to_transform != nullptr ? *static_cast<const transform_type*>(measured_to_transform) : transform_math_t::identity();
				const transform_type measured_to_root = transform_math_t::mul(measured_to_transform_, transform_to_root);

				return m_metric.calculate_error_squared(args, *static_cast<const transform_type*>(measured_to_root_raw), measured_to_root);
			}

			// The chain holds the measured transform first and the transform being optimized last
			// The cached transforms of the chain hold local space transforms
			rtm::scalarf RTM_SIMD_CALL measure_chain(
				const error_args_type& args, const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index,
				const void* parent_transform_to_root, const void* cached_pose,
				const uint32_t* transform_chain_indices, uint32_t transform_chain_length,
				const void* measured_to_root_raw) const
			{
				(void)transform_index;
				(void)sample_index;
				const transform_type* cached_pose_ = static_cast<const transform_type*>(cached_pose);
				const transform_type local_transform = transform_math_t::from_vqv(local_transform_vqv);

				const transform_type parent_transform_to_root_ = parent_transform_to_root != nullptr ? *static_cast<const transform_type*>(parent_transform_to_root) : transform_math_t::identity();
				transform_type measured_to_root = mul_with_parent(local_transform, parent_transform_to_root_);

				// Skip the transform being optimized
				for (const uint32_t chain_transform_index : make_reverse_iterator(transform_chain_indices, transform_chain_length - 1))
					measured_to_root = mul_with_parent(cached_pose_[chain_transform_index], measured_to_root);

				return m_metric.calculate_error_squared(args, *static_cast<const transform_type*>(measured_to_root_raw), measured_to_root);
			}

			// measured_to_transform = measured_to_transform * transform_being_optimized
			// In the mesh rotation space, see 'mesh_rotation_cache_t' and 'measure_cached'
			void update_cached(const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index, void* measured_to_transform, const void* chain_child, bool is_measured_transform_optimized) const
			{
				(void)transform_index;
				(void)sample_index;
				transform_type& measured_to_transform_ = *static_cast<transform_type*>(measured_to_transform);
				const transform_type local_transform = transform_math_t::from_vqv(local_transform_vqv);

				if (m_is_mesh_rotation)
				{
					if (is_measured_transform_optimized)
					{
						measured_to_transform_ = local_transform;
					}
					else
					{
						ACL_ASSERT(chain_child != nullptr, "The mesh rotation cache needs the chain child");
						measured_to_transform_ = mesh_rotation_cache_t<transform_math_t>::update(
							local_transform, *static_cast<const transform_type*>(chain_child), measured_to_transform_, measured_to_transform == chain_child);
					}

					return;
				}

				const transform_type old_measured_to_transform = is_measured_transform_optimized ? transform_math_t::identity() : measured_to_transform_;
				measured_to_transform_ = transform_math_t::mul(old_measured_to_transform, local_transform);
			}

			void store_local(const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index, void* out_transform) const
			{
				(void)transform_index;
				(void)sample_index;
				*static_cast<transform_type*>(out_transform) = transform_math_t::from_vqv(local_transform_vqv);
			}

		private:
			// VQ math does not use the scale: the scale of each transform is 1.0
			static void compute_shell_distances_impl(std::true_type, const segment_context& segment, const rtm::vqvf* object_poses, rigid_shell_metadata_t* out_shell_metadata)
			{
				const vq_shell_transform_reader reader{ object_poses, segment.num_bones };
				compute_segment_shell_distances_with_reader(segment, reader, out_shell_metadata);
			}

			static void compute_shell_distances_impl(std::false_type, const segment_context& segment, const transform_type* object_poses, rigid_shell_metadata_t* out_shell_metadata)
			{
				compute_segment_shell_distances(segment, object_poses, out_shell_metadata);
			}

			// Returns the object transform of a transform from the object transform of its parent
			transform_type mul_with_parent(const transform_type& transform, const transform_type& parent_object_transform) const
			{
				return m_is_mesh_rotation ? transform_math_t::mul_mesh_rotation(transform, parent_object_transform) : transform_math_t::mul(transform, parent_object_transform);
			}

			metric_t m_metric;
			bool m_is_mesh_rotation;
		};

		//////////////////////////////////////////////////////////////////////////
		// Reads object space transforms through a transform adapter for 'compute_segment_shell_distances_with_reader'
		// We transform the unit axes and the origin with the transform
		struct adapter_shell_transform_reader
		{
			const itransform_adapter* adapter;
			const uint8_t* object_poses;
			size_t transform_size;
			uint32_t num_transforms;

			void RTM_SIMD_CALL get_points(uint32_t sample_index, uint32_t transform_index, rtm::vector4f& out_xxxx, rtm::vector4f& out_yyyy, rtm::vector4f& out_zzzz) const
			{
				// The X, Y, and Z lanes hold the X, Y, and Z unit axes, the W lane holds the origin
				const rtm::vector4f points_xxxx = rtm::vector_set(1.0F, 0.0F, 0.0F, 0.0F);
				const rtm::vector4f points_yyyy = rtm::vector_set(0.0F, 1.0F, 0.0F, 0.0F);
				const rtm::vector4f points_zzzz = rtm::vector_set(0.0F, 0.0F, 1.0F, 0.0F);

				const uint8_t* transform = object_poses + (((sample_index * num_transforms) + transform_index) * transform_size);
				adapter->mul_point3_x4(points_xxxx, points_yyyy, points_zzzz, transform, out_xxxx, out_yyyy, out_zzzz);
			}

			rtm::vector4f RTM_SIMD_CALL get_translation(uint32_t sample_index, uint32_t transform_index) const
			{
				rtm::vector4f points_xxxx;
				rtm::vector4f points_yyyy;
				rtm::vector4f points_zzzz;
				get_points(sample_index, transform_index, points_xxxx, points_yyyy, points_zzzz);

				return rtm::vector_set(rtm::vector_get_w(points_xxxx), rtm::vector_get_w(points_yyyy), rtm::vector_get_w(points_zzzz), 0.0F);
			}

			rtm::scalarf RTM_SIMD_CALL get_largest_scale(uint32_t sample_index, uint32_t transform_index) const
			{
				rtm::vector4f points_xxxx;
				rtm::vector4f points_yyyy;
				rtm::vector4f points_zzzz;
				get_points(sample_index, transform_index, points_xxxx, points_yyyy, points_zzzz);

				// The distance from the origin to each unit axis after the transform
				const rtm::vector4f axes_xxxx = rtm::vector_sub(points_xxxx, rtm::vector_dup_w(points_xxxx));
				const rtm::vector4f axes_yyyy = rtm::vector_sub(points_yyyy, rtm::vector_dup_w(points_yyyy));
				const rtm::vector4f axes_zzzz = rtm::vector_sub(points_zzzz, rtm::vector_dup_w(points_zzzz));
				const rtm::vector4f axes_length_sq = rtm::vector_add(rtm::vector_add(rtm::vector_mul(axes_xxxx, axes_xxxx), rtm::vector_mul(axes_yyyy, axes_yyyy)), rtm::vector_mul(axes_zzzz, axes_zzzz));

				// The W lane is zero, it is the origin
				return rtm::scalar_sqrt(rtm::vector_get_max_component_as_scalar(axes_length_sq));
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// The scratch memory of 'v2_adapter_policy'
		struct v2_adapter_policy_scratch
		{
			uint8_t* transforms;			// v2_adapter_policy::k_num_scratch_transforms runtime transforms
			rtm::vqvf* pose_vqvf;			// 1 rtm::vqvf per transform
			uint8_t* pose_runtime;			// 1 runtime transform per transform
			uint32_t num_transforms;
		};

		//////////////////////////////////////////////////////////////////////////
		// A policy that uses a transform adapter and a transform metric through their virtual functions.
		// Compression uses it with a custom transform adapter or a custom transform metric.
		// The conversion receives the index of each transform, the index of its sample in the clip, and
		// whether or not it is lossy: a custom transform adapter can use them.
		class v2_adapter_policy
		{
		public:
			using error_args_type = itransform_metric::calculate_error_args;

			static constexpr uint32_t k_num_scratch_transforms = 4;

			// In mesh rotation space, the policy composes the transforms with the mesh rotation product
			// The transform adapter must be a built-in 'vqf', 'vqsf', 'vqvf', or 'vqmf' type, the metric can be custom
			// The 'vqf' and 'vqsf' types use the mesh rotation cache, the other types use the transform chains
			v2_adapter_policy(const itransform_adapter& adapter, const itransform_metric& metric, const v2_adapter_policy_scratch& scratch, bool is_mesh_rotation = false)
				: m_adapter(adapter)
				, m_metric(metric)
				, m_transform_size(adapter.get_transform_size())
				, m_type(adapter.get_type())
				, m_is_mesh_rotation(is_mesh_rotation)
				, m_uses_cache(is_mesh_rotation ? (m_type == transform_runtime_type_t::vqf || m_type == transform_runtime_type_t::vqsf) : adapter.is_associative())
				, m_needs_conversion(adapter.needs_conversion())
				, m_num_transforms(scratch.num_transforms)
				, m_local_transform(scratch.transforms)
				, m_transform_to_root(scratch.transforms + m_transform_size)
				, m_measured_to_root(scratch.transforms + (m_transform_size * 2))
				, m_identity(scratch.transforms + (m_transform_size * 3))
				, m_pose_vqvf(scratch.pose_vqvf)
				, m_pose_runtime(scratch.pose_runtime)
			{
				ACL_ASSERT(m_needs_conversion || m_transform_size == sizeof(rtm::vqvf), "A transform adapter without conversion must use rtm::vqvf");
				ACL_ASSERT(!is_mesh_rotation || m_type == transform_runtime_type_t::vqf || m_type == transform_runtime_type_t::vqsf || m_type == transform_runtime_type_t::vqvf || m_type == transform_runtime_type_t::vqmf, "The transform adapter does not support the mesh rotation space");
				adapter.identity(m_identity);
			}

			v2_adapter_policy(const v2_adapter_policy&) = delete;
			v2_adapter_policy& operator=(const v2_adapter_policy&) = delete;

			void init_error_args(error_args_type& args, float shell_distance) const
			{
				args.construct_sphere_shell(shell_distance);
				args.adapter = &m_adapter;
				args.transform0 = nullptr;
				args.transform1 = nullptr;
			}

			size_t get_transform_size() const { return m_transform_size; }
			bool uses_cache() const { return m_uses_cache; }
			bool needs_chain_child() const { return m_is_mesh_rotation && m_uses_cache; }
			bool needs_conversion() const { return m_needs_conversion; }

			// The output can be the input when the transform type is rtm::vqvf
			void local_to_object_space(const clip_topology_t& topology, const rtm::vqvf* local_pose, void* out_object_pose, uint32_t sample_index, bool is_lossy) const
			{
				uint8_t* object_pose = static_cast<uint8_t*>(out_object_pose);

				// We convert the whole pose with one call
				const uint8_t* local_pose_runtime = reinterpret_cast<const uint8_t*>(local_pose);
				if (m_needs_conversion)
				{
					itransform_adapter::convert_transforms_args args;
					args.dirty_transform_indices = topology.roots_first_iterator().begin();
					args.num_dirty_transforms = m_num_transforms;
					args.transforms = local_pose;
					args.num_transforms = m_num_transforms;
					args.sample_index = sample_index;
					args.is_lossy = is_lossy;

					m_adapter.convert_transforms(args, m_pose_runtime);
					local_pose_runtime = m_pose_runtime;
				}

				for (const uint32_t transform_index : topology.roots_first_iterator())
				{
					const uint8_t* local_transform = local_pose_runtime + (transform_index * m_transform_size);
					uint8_t* object_transform = object_pose + (transform_index * m_transform_size);

					const uint32_t parent_transform_index = topology.transforms[transform_index].parent_index;
					if (parent_transform_index != k_invalid_track_index)
						mul_with_parent(local_transform, object_pose + (parent_transform_index * m_transform_size), object_transform);
					else if (object_transform != local_transform)
						std::memcpy(object_transform, local_transform, m_transform_size);	// The root has no parent: its local and object space transforms are equal
				}
			}

			void compute_shell_distances(const segment_context& segment, const void* object_poses, rigid_shell_metadata_t* out_shell_metadata) const
			{
				const adapter_shell_transform_reader reader{ &m_adapter, static_cast<const uint8_t*>(object_poses), m_transform_size, segment.num_bones };
				compute_segment_shell_distances_with_reader(segment, reader, out_shell_metadata);
			}

			rtm::scalarf RTM_SIMD_CALL measure_cached(
				const error_args_type& args, const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index,
				const void* parent_transform_to_root, const void* measured_to_transform, const void* chain_child, const void* measured_to_root_raw) const
			{
				convert_lossy(local_transform_vqv, transform_index, sample_index, m_local_transform);

				const void* parent_transform_to_root_ = parent_transform_to_root != nullptr ? parent_transform_to_root : m_identity;

				if (m_is_mesh_rotation)
				{
					// 'mul_with_parent' uses 'm_transform_to_root' as scratch memory
					mul_with_parent(m_local_transform, parent_transform_to_root_, m_measured_to_root);
					if (measured_to_transform != nullptr)
					{
						ACL_ASSERT(chain_child != nullptr, "The mesh rotation cache needs the chain child");
						if (m_type == transform_runtime_type_t::vqsf)
							measure_mesh_rotation_cached<vqs_transform_math>(chain_child, measured_to_transform);
						else
							measure_mesh_rotation_cached<vq_transform_math>(chain_child, measured_to_transform);
					}

					return calculate_error_squared(args, measured_to_root_raw, m_measured_to_root);
				}

				m_adapter.mul(m_local_transform, parent_transform_to_root_, m_transform_to_root);

				const void* measured_to_transform_ = measured_to_transform != nullptr ? measured_to_transform : m_identity;
				m_adapter.mul(measured_to_transform_, m_transform_to_root, m_measured_to_root);

				return calculate_error_squared(args, measured_to_root_raw, m_measured_to_root);
			}

			rtm::scalarf RTM_SIMD_CALL measure_chain(
				const error_args_type& args, const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index,
				const void* parent_transform_to_root, const void* cached_pose,
				const uint32_t* transform_chain_indices, uint32_t transform_chain_length,
				const void* measured_to_root_raw) const
			{
				const uint8_t* cached_pose_ = static_cast<const uint8_t*>(cached_pose);
				convert_lossy(local_transform_vqv, transform_index, sample_index, m_local_transform);

				const void* parent_transform_to_root_ = parent_transform_to_root != nullptr ? parent_transform_to_root : m_identity;
				mul_with_parent(m_local_transform, parent_transform_to_root_, m_measured_to_root);

				// Skip the transform being optimized
				// The output of 'mul' can be one of its inputs
				for (const uint32_t chain_transform_index : make_reverse_iterator(transform_chain_indices, transform_chain_length - 1))
					mul_with_parent(cached_pose_ + (chain_transform_index * m_transform_size), m_measured_to_root, m_measured_to_root);

				return calculate_error_squared(args, measured_to_root_raw, m_measured_to_root);
			}

			void update_cached(const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index, void* measured_to_transform, const void* chain_child, bool is_measured_transform_optimized) const
			{
				if (m_is_mesh_rotation && is_measured_transform_optimized)
				{
					convert_lossy(local_transform_vqv, transform_index, sample_index, measured_to_transform);
					return;
				}

				convert_lossy(local_transform_vqv, transform_index, sample_index, m_local_transform);

				if (m_is_mesh_rotation)
				{
					ACL_ASSERT(chain_child != nullptr, "The mesh rotation cache needs the chain child");
					if (m_type == transform_runtime_type_t::vqsf)
						update_mesh_rotation_cached<vqs_transform_math>(chain_child, measured_to_transform);
					else
						update_mesh_rotation_cached<vq_transform_math>(chain_child, measured_to_transform);
					return;
				}

				// The output of 'mul' can be one of its inputs
				const void* old_measured_to_transform = is_measured_transform_optimized ? m_identity : measured_to_transform;
				m_adapter.mul(old_measured_to_transform, m_local_transform, measured_to_transform);
			}

			void store_local(const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index, void* out_transform) const
			{
				convert_lossy(local_transform_vqv, transform_index, sample_index, out_transform);
			}

		private:
			// Writes the object transform of a transform from the object transform of its parent
			// The output can be one of the inputs
			void mul_with_parent(const void* transform, const void* parent_object_transform, void* out_object_transform) const
			{
				if (!m_is_mesh_rotation)
					m_adapter.mul(transform, parent_object_transform, out_object_transform);
				else if (m_type == transform_runtime_type_t::vqsf)
					mul_mesh_rotation<rtm::vqsf>(transform, parent_object_transform, out_object_transform);
				else if (m_type == transform_runtime_type_t::vqmf)
					mul_mesh_rotation<rtm::vqmf>(transform, parent_object_transform, out_object_transform);
				else
					mul_mesh_rotation<rtm::vqvf>(transform, parent_object_transform, out_object_transform);
			}

			// The mesh rotation product through the transform adapter, see 'mul_mesh_rotation_impl'
			// We use 'm_transform_to_root' as scratch memory: only the cached path uses it, the mesh rotation space does not
			template<class transform_type>
			void mul_mesh_rotation(const void* transform, const void* parent_object_transform, void* out_object_transform) const
			{
				const rtm::quatf rotation = static_cast<const transform_type*>(transform)->rotation;
				const rtm::quatf parent_rotation = static_cast<const transform_type*>(parent_object_transform)->rotation;

				transform_type& local_transform = *safe_ptr_cast<transform_type>(m_transform_to_root);
				std::memcpy(&local_transform, transform, sizeof(transform_type));
				local_transform.rotation = rtm::quat_normalize(rtm::quat_mul(rotation, rtm::quat_conjugate(parent_rotation)));

				m_adapter.mul(&local_transform, parent_object_transform, out_object_transform);
				static_cast<transform_type*>(out_object_transform)->rotation = rotation;
			}

			// Writes the object transform of the measured transform in 'm_measured_to_root', see 'mesh_rotation_cache_t'
			// 'm_measured_to_root' holds the object transform of the transform being optimized
			template<class transform_math_t>
			void measure_mesh_rotation_cached(const void* chain_child, const void* measured) const
			{
				using transform_type = typename transform_math_t::transform_type;
				transform_type& measured_to_root = *safe_ptr_cast<transform_type>(m_measured_to_root);
				measured_to_root = mesh_rotation_cache_t<transform_math_t>::measure(
					measured_to_root, *static_cast<const transform_type*>(chain_child), *static_cast<const transform_type*>(measured), measured == chain_child);
			}

			// Adds the transform being optimized into the cached transform of the measured transform, see 'mesh_rotation_cache_t'
			// 'm_local_transform' holds the local transform of the transform being optimized
			template<class transform_math_t>
			void update_mesh_rotation_cached(const void* chain_child, void* measured) const
			{
				using transform_type = typename transform_math_t::transform_type;
				transform_type& measured_ = *static_cast<transform_type*>(measured);
				measured_ = mesh_rotation_cache_t<transform_math_t>::update(
					*safe_ptr_cast<const transform_type>(m_local_transform), *static_cast<const transform_type*>(chain_child), measured_, measured == chain_child);
			}

			// Converts one lossy transform
			// We place it in a full pose: the adapter reads and writes the transform at its index
			void convert_lossy(const rtm::vqvf& transform, uint32_t transform_index, uint32_t sample_index, void* out_transform) const
			{
				if (!m_needs_conversion)
				{
					std::memcpy(out_transform, &transform, sizeof(rtm::vqvf));
					return;
				}

				m_pose_vqvf[transform_index] = transform;

				itransform_adapter::convert_transforms_args args;
				args.dirty_transform_indices = &transform_index;
				args.num_dirty_transforms = 1;
				args.transforms = m_pose_vqvf;
				args.num_transforms = m_num_transforms;
				args.sample_index = sample_index;
				args.is_lossy = true;

				m_adapter.convert_transforms(args, m_pose_runtime);
				std::memcpy(out_transform, m_pose_runtime + (transform_index * m_transform_size), m_transform_size);
			}

			rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const error_args_type& args, const void* raw_transform, const void* lossy_transform) const
			{
				error_args_type args_ = args;
				args_.transform0 = raw_transform;
				args_.transform1 = lossy_transform;

				return m_metric.calculate_error_squared(args_);
			}

			const itransform_adapter& m_adapter;
			const itransform_metric& m_metric;
			size_t m_transform_size;
			transform_runtime_type_t m_type;
			bool m_is_mesh_rotation;
			bool m_uses_cache;
			bool m_needs_conversion;
			uint32_t m_num_transforms;

			// Scratch memory, we write it in the const functions
			uint8_t* m_local_transform;
			uint8_t* m_transform_to_root;
			uint8_t* m_measured_to_root;
			uint8_t* m_identity;
			rtm::vqvf* m_pose_vqvf;
			uint8_t* m_pose_runtime;
		};
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_DEPRECATION_WARNING_POP

ACL_IMPL_FILE_PRAGMA_POP
