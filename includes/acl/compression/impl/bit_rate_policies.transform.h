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
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/transform_adapters.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/transform_metrics.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/rigid_shell_utils.h"
#include "acl/compression/impl/segment_context.h"
#include "acl/compression/impl/topology_metadata.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
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
		//    - 'get_transform_size()', 'is_associative()', and 'needs_conversion()'
		//    - 'local_to_object_space(..)': converts a local space pose and takes it into object space
		//    - 'compute_shell_distances(..)': finds the rigid shell of each transform from the object space poses
		//    - 'measure_cached(..)': measures the error when the product is associative
		//    - 'measure_chain(..)': measures the error when the product is not associative
		//    - 'update_cached(..)' and 'store_local(..)': update the cached transforms after optimization
		//
		// The functions use the transforms through pointers: the transform type of a custom
		// transform adapter is known only at runtime. With a built-in type, the compiler
		// removes the pointers and the virtual function calls.
		//
		// When the product is associative, the cached transforms hold:
		//    - for a transform that is not optimized: its object space transform
		//    - for a critical transform: its transform relative to the last transform optimized in its chain
		// When the product is not associative, the cached transforms hold:
		//    - for a transform that is not optimized: its object space transform
		//    - for a transform that is optimized: its local space transform
		// See 'find_optimal_bit_rates_v2' for details.
		//////////////////////////////////////////////////////////////////////////

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

			explicit v2_static_policy(const metric_t& metric) : m_metric(metric) {}

			void init_error_args(error_args_type& args, float shell_distance) const { m_metric.init_error_args(args, shell_distance); }

			static constexpr size_t get_transform_size() { return sizeof(transform_type); }
			static constexpr bool is_associative() { return transform_math_t::is_associative(); }
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
						object_pose[transform_index] = transform_math_t::mul(local_transform, object_pose[parent_transform_index]);
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
			rtm::scalarf RTM_SIMD_CALL measure_cached(
				const error_args_type& args, const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index,
				const void* parent_transform_to_root, const void* measured_to_transform, const void* measured_to_root_raw) const
			{
				(void)transform_index;
				(void)sample_index;
				const transform_type local_transform = transform_math_t::from_vqv(local_transform_vqv);

				const transform_type parent_transform_to_root_ = parent_transform_to_root != nullptr ? *static_cast<const transform_type*>(parent_transform_to_root) : transform_math_t::identity();
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
				transform_type measured_to_root = transform_math_t::mul(local_transform, parent_transform_to_root_);

				// Skip the transform being optimized
				for (const uint32_t chain_transform_index : make_reverse_iterator(transform_chain_indices, transform_chain_length - 1))
					measured_to_root = transform_math_t::mul(cached_pose_[chain_transform_index], measured_to_root);

				return m_metric.calculate_error_squared(args, *static_cast<const transform_type*>(measured_to_root_raw), measured_to_root);
			}

			// measured_to_transform = measured_to_transform * transform_being_optimized
			void update_cached(const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index, void* measured_to_transform, bool is_measured_transform_optimized) const
			{
				(void)transform_index;
				(void)sample_index;
				transform_type& measured_to_transform_ = *static_cast<transform_type*>(measured_to_transform);
				const transform_type local_transform = transform_math_t::from_vqv(local_transform_vqv);

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

			metric_t m_metric;
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

			v2_adapter_policy(const itransform_adapter& adapter, const itransform_metric& metric, const v2_adapter_policy_scratch& scratch)
				: m_adapter(adapter)
				, m_metric(metric)
				, m_transform_size(adapter.get_transform_size())
				, m_is_associative(adapter.is_associative())
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
			bool is_associative() const { return m_is_associative; }
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
						m_adapter.mul(local_transform, object_pose + (parent_transform_index * m_transform_size), object_transform);
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
				const void* parent_transform_to_root, const void* measured_to_transform, const void* measured_to_root_raw) const
			{
				convert_lossy(local_transform_vqv, transform_index, sample_index, m_local_transform);

				const void* parent_transform_to_root_ = parent_transform_to_root != nullptr ? parent_transform_to_root : m_identity;
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
				m_adapter.mul(m_local_transform, parent_transform_to_root_, m_measured_to_root);

				// Skip the transform being optimized
				// The output of 'mul' can be one of its inputs
				for (const uint32_t chain_transform_index : make_reverse_iterator(transform_chain_indices, transform_chain_length - 1))
					m_adapter.mul(cached_pose_ + (chain_transform_index * m_transform_size), m_measured_to_root, m_measured_to_root);

				return calculate_error_squared(args, measured_to_root_raw, m_measured_to_root);
			}

			void update_cached(const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index, void* measured_to_transform, bool is_measured_transform_optimized) const
			{
				convert_lossy(local_transform_vqv, transform_index, sample_index, m_local_transform);

				// The output of 'mul' can be one of its inputs
				const void* old_measured_to_transform = is_measured_transform_optimized ? m_identity : measured_to_transform;
				m_adapter.mul(old_measured_to_transform, m_local_transform, measured_to_transform);
			}

			void store_local(const rtm::vqvf& local_transform_vqv, uint32_t transform_index, uint32_t sample_index, void* out_transform) const
			{
				convert_lossy(local_transform_vqv, transform_index, sample_index, out_transform);
			}

		private:
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
			bool m_is_associative;
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
