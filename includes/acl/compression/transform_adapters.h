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
#include "acl/core/track_types.h"
#include "acl/compression/fwd.h"
#include "acl/math/matrix3x4f.h"
#include "acl/math/vqmf.h"
#include "acl/math/vqsf.h"
#include "acl/math/vqvf.h"

#include <rtm/matrix3x4f.h>
#include <rtm/vqmf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// The transform types that compression uses at runtime to compose transforms
	// and to measure the error.
	// The runtime type is not the type of the raw tracks: compression converts
	// each rtm::vqvf source transform into the runtime type.
	enum class transform_runtime_type_t : uint8_t
	{
		//////////////////////////////////////////////////////////////////////////
		// A transform type that ACL does not know.
		// ACL uses this transform type only through the functions of its transform adapter.
		custom		= 0,

		//////////////////////////////////////////////////////////////////////////
		// A rotation and a translation, stored as rtm::vqvf. The scale is not used.
		// The product of two VQ transforms is exact and associative.
		vqf			= 1,

		//////////////////////////////////////////////////////////////////////////
		// A rotation, a translation, and a uniform scale, stored as rtm::vqsf.
		// The product of two VQS transforms is exact and associative: a uniform scale creates no shear.
		vqsf		= 2,

		//////////////////////////////////////////////////////////////////////////
		// A rotation, a translation, and a 3D scale, stored as rtm::vqvf.
		// The product of two VQV transforms discards the shear. Thus the product is not associative.
		vqvf		= 3,

		//////////////////////////////////////////////////////////////////////////
		// A rotation, a translation, and a 3x3 scale/shear matrix, stored as rtm::vqmf.
		// The product of two VQM transforms keeps the shear. Thus the product is exact and associative.
		vqmf		= 4,

		//////////////////////////////////////////////////////////////////////////
		// A 3x4 affine matrix, stored as rtm::matrix3x4f.
		// The product of two matrices keeps the shear. Thus the product is exact and associative.
		matrix3x4f	= 5,

		//////////////////////////////////////////////////////////////////////////
		// For the compression settings only: compression selects the runtime transform type
		// from the raw tracks. See 'compression_settings::transform_runtime_type'.
		// A transform adapter never returns this value.
		automatic	= 6,
	};

	//////////////////////////////////////////////////////////////////////////
	// Interface for all transform adapters.
	// A transform adapter gives the operations of a runtime transform type to compression.
	// Compression uses them to put the transforms in object space and to measure the error:
	//    - converting from rtm::vqvf into the runtime transform type
	//    - the identity transform and the product of two transforms
	//    - taking local space transforms into object space
	//    - transforming points
	//
	// The functions use the runtime transform type through 'void' pointers.
	// The runtime transform type must have an alignment of 16 bytes or less.
	//
	// ACL calls these functions in the inner loops of compression. Thus most functions
	// take a list of transforms, to keep the number of virtual function calls low.
	//////////////////////////////////////////////////////////////////////////
	class itransform_adapter
	{
	public:
		virtual ~itransform_adapter() = default;

		//////////////////////////////////////////////////////////////////////////
		// Returns the unique name of the transform adapter.
		virtual const char* get_name() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns a unique hash to represent the transform adapter.
		virtual uint32_t get_hash() const { return hash32(get_name()); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the runtime transform type of the adapter.
		// A custom transform adapter returns 'custom'.
		virtual transform_runtime_type_t get_type() const { return transform_runtime_type_t::custom; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of the runtime transform type.
		virtual size_t get_transform_size() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not the product of two transforms is associative.
		// When it is, compression can keep object space transforms and update only what changes.
		// When it is not, compression must multiply the full chain of transforms from the root.
		virtual bool is_associative() const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not the runtime transform type is different from rtm::vqvf.
		// When it is not, compression uses the rtm::vqvf transforms directly and does not
		// call 'convert_transforms'.
		virtual bool needs_conversion() const { return true; }

		//////////////////////////////////////////////////////////////////////////
		// Input arguments for the 'convert_transforms' function.
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
			// The sample time is 'sample_index / sample_rate', clamped to the clip duration.
			uint32_t sample_index;

			//////////////////////////////////////////////////////////////////////////
			// True if these transforms come from the lossy data that compression tests.
			// False if these transforms come from the raw data.
			// A transform adapter can use it with data that compression does not see: for example,
			// a value that you compress in another track and that you add to the lossy transforms only.
			bool is_lossy;
		};

		//////////////////////////////////////////////////////////////////////////
		// Converts from rtm::vqvf into the runtime transform type.
		// Called when 'needs_conversion' returns true.
		virtual void convert_transforms(const convert_transforms_args& args, void* out_transforms) const
		{
			(void)args;
			(void)out_transforms;
			ACL_ASSERT(false, "Not implemented");
		}

		//////////////////////////////////////////////////////////////////////////
		// Writes the identity transform.
		virtual void identity(void* out_transform) const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Multiplies two transforms and writes the result.
		// Multiplication order is as follow: local_to_world = mul(local_to_object, object_to_world)
		// The output can be one of the inputs.
		virtual void mul(const void* lhs, const void* rhs, void* out_transform) const = 0;

		//////////////////////////////////////////////////////////////////////////
		// Input arguments for the 'local_to_object_space' function.
		//////////////////////////////////////////////////////////////////////////
		struct local_to_object_space_args
		{
			//////////////////////////////////////////////////////////////////////////
			// A list of transform indices that are dirty and need transformation.
			// A parent transform comes before its children in the list.
			const uint32_t* dirty_transform_indices;

			//////////////////////////////////////////////////////////////////////////
			// The number of dirty transforms that need transformation.
			uint32_t num_dirty_transforms;

			//////////////////////////////////////////////////////////////////////////
			// A list of parent transform indices for every transform.
			// An index of 'k_invalid_track_index' represents a root transform with no parent.
			const uint32_t* parent_transform_indices;

			//////////////////////////////////////////////////////////////////////////
			// The input local space transforms in the runtime transform type.
			const void* local_transforms;

			//////////////////////////////////////////////////////////////////////////
			// The number of transforms in the input and output buffers.
			uint32_t num_transforms;
		};

		//////////////////////////////////////////////////////////////////////////
		// Takes local space transforms into object space.
		// The object space transform of a root is its local space transform.
		// The default implementation uses 'mul'. A transform adapter can replace it with a faster version.
		virtual void local_to_object_space(const local_to_object_space_args& args, void* out_object_transforms) const
		{
			const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
			const uint32_t* parent_transform_indices = args.parent_transform_indices;
			const uint8_t* local_transforms_ = static_cast<const uint8_t*>(args.local_transforms);
			uint8_t* out_object_transforms_ = static_cast<uint8_t*>(out_object_transforms);
			const size_t transform_size = get_transform_size();

			const uint32_t num_dirty_transforms = args.num_dirty_transforms;
			for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
			{
				const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
				const uint32_t parent_transform_index = parent_transform_indices[transform_index];

				const uint8_t* local_transform = local_transforms_ + (transform_index * transform_size);
				uint8_t* out_object_transform = out_object_transforms_ + (transform_index * transform_size);

				if (parent_transform_index == k_invalid_track_index)
					std::memcpy(out_object_transform, local_transform, transform_size);	// The root has no parent: its local and object space transforms are equal
				else
					mul(local_transform, out_object_transforms_ + (parent_transform_index * transform_size), out_object_transform);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Multiplies a transform and four 3D points stored transposed.
		// Multiplication order is as follow: world_position = mul_point3_x4(local_position, local_to_world)
		virtual void RTM_SIMD_CALL mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			const void* transform,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) const = 0;
	};

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// The math of each built-in runtime transform type.
		// Compression uses these types directly when it knows the transform type,
		// and through the built-in transform adapters otherwise.
		// Each type has:
		//    - 'transform_type': the type that holds a transform
		//    - 'get_type()' and 'is_associative()'
		//    - 'identity()', 'from_vqv(..)', 'mul(..)', and 'mul_point3_x4(..)'
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// VQ math: we use rtm::vqvf and we do not use the scale.
		struct vq_transform_math
		{
			using transform_type = rtm::vqvf;

			static constexpr transform_runtime_type_t get_type() { return transform_runtime_type_t::vqf; }
			static constexpr bool is_associative() { return true; }

			static rtm::vqvf identity() { return rtm::vqv_identity(); }
			static rtm::vqvf from_vqv(const rtm::vqvf& transform) { return transform; }
			static rtm::vqvf mul(const rtm::vqvf& lhs, const rtm::vqvf& rhs) { return rtm::vqv_normalize(rtm::vqv_mul_no_scale(lhs, rhs)); }

			static RTM_FORCE_INLINE void RTM_SIMD_CALL mul_point3_x4(
				rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
				const rtm::vqvf& transform,
				rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz)
			{
				vqv_mul_point3_no_scale_x4(point_xxxx, point_yyyy, point_zzzz, transform, out_result_xxxx, out_result_yyyy, out_result_zzzz);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// VQS math: we use rtm::vqsf, the uniform scale comes from the X component of the rtm::vqvf scale.
		struct vqs_transform_math
		{
			using transform_type = rtm::vqsf;

			static constexpr transform_runtime_type_t get_type() { return transform_runtime_type_t::vqsf; }
			static constexpr bool is_associative() { return true; }

			static rtm::vqsf identity() { return rtm::vqs_identity(); }
			static rtm::vqsf from_vqv(const rtm::vqvf& transform)
			{
				const float scale = rtm::vector_get_x(transform.scale);
				return rtm::vqs_set(transform.rotation, transform.translation, scale);
			}
			static rtm::vqsf mul(const rtm::vqsf& lhs, const rtm::vqsf& rhs) { return rtm::vqs_normalize(rtm::vqs_mul(lhs, rhs)); }

			static RTM_FORCE_INLINE void RTM_SIMD_CALL mul_point3_x4(
				rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
				const rtm::vqsf& transform,
				rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz)
			{
				vqs_mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, transform, out_result_xxxx, out_result_yyyy, out_result_zzzz);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// VQV math: we use rtm::vqvf, the product discards the shear.
		struct vqv_transform_math
		{
			using transform_type = rtm::vqvf;

			static constexpr transform_runtime_type_t get_type() { return transform_runtime_type_t::vqvf; }
			static constexpr bool is_associative() { return false; }

			static rtm::vqvf identity() { return rtm::vqv_identity(); }
			static rtm::vqvf from_vqv(const rtm::vqvf& transform) { return transform; }
			static rtm::vqvf mul(const rtm::vqvf& lhs, const rtm::vqvf& rhs) { return rtm::vqv_normalize(rtm::vqv_mul(lhs, rhs)); }

			static RTM_FORCE_INLINE void RTM_SIMD_CALL mul_point3_x4(
				rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
				const rtm::vqvf& transform,
				rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz)
			{
				vqv_mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, transform, out_result_xxxx, out_result_yyyy, out_result_zzzz);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// VQM math: we use rtm::vqmf, the product keeps the shear.
		struct vqm_transform_math
		{
			using transform_type = rtm::vqmf;

			static constexpr transform_runtime_type_t get_type() { return transform_runtime_type_t::vqmf; }
			static constexpr bool is_associative() { return true; }

			static rtm::vqmf identity() { return rtm::vqm_identity(); }
			static rtm::vqmf from_vqv(const rtm::vqvf& transform) { return rtm::vqm_set(transform.rotation, transform.translation, transform.scale); }
			static rtm::vqmf mul(const rtm::vqmf& lhs, const rtm::vqmf& rhs) { return rtm::vqm_normalize(rtm::vqm_mul(lhs, rhs)); }

			static RTM_FORCE_INLINE void RTM_SIMD_CALL mul_point3_x4(
				rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
				const rtm::vqmf& transform,
				rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz)
			{
				vqm_mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, transform, out_result_xxxx, out_result_yyyy, out_result_zzzz);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// Matrix math: we use rtm::matrix3x4f, the product keeps the shear.
		struct matrix3x4_transform_math
		{
			using transform_type = rtm::matrix3x4f;

			static constexpr transform_runtime_type_t get_type() { return transform_runtime_type_t::matrix3x4f; }
			static constexpr bool is_associative() { return true; }

			static rtm::matrix3x4f identity() { return rtm::matrix_identity(); }
			static rtm::matrix3x4f from_vqv(const rtm::vqvf& transform) { return rtm::matrix_from_vqv(transform); }
			static rtm::matrix3x4f mul(const rtm::matrix3x4f& lhs, const rtm::matrix3x4f& rhs) { return rtm::matrix_mul(lhs, rhs); }

			static RTM_FORCE_INLINE void RTM_SIMD_CALL mul_point3_x4(
				rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
				const rtm::matrix3x4f& transform,
				rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz)
			{
				matrix_mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, transform, out_result_xxxx, out_result_yyyy, out_result_zzzz);
			}
		};

		//////////////////////////////////////////////////////////////////////////
		// Implements the transform adapter interface with the math of a built-in runtime transform type.
		template<class transform_math_type>
		class builtin_transform_adapter : public itransform_adapter
		{
		public:
			using transform_math = transform_math_type;
			using transform_type = typename transform_math_type::transform_type;

			virtual transform_runtime_type_t get_type() const override { return transform_math_type::get_type(); }
			virtual size_t get_transform_size() const override { return sizeof(transform_type); }
			virtual bool is_associative() const override { return transform_math_type::is_associative(); }
			virtual bool needs_conversion() const override { return !std::is_same<transform_type, rtm::vqvf>::value; }

			virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void convert_transforms(const convert_transforms_args& args, void* out_transforms) const override
			{
				const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
				const rtm::vqvf* transforms_ = args.transforms;
				transform_type* out_transforms_ = static_cast<transform_type*>(out_transforms);

				const uint32_t num_dirty_transforms = args.num_dirty_transforms;
				for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
				{
					const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
					out_transforms_[transform_index] = transform_math_type::from_vqv(transforms_[transform_index]);
				}
			}

			virtual void identity(void* out_transform) const override
			{
				*static_cast<transform_type*>(out_transform) = transform_math_type::identity();
			}

			virtual void mul(const void* lhs, const void* rhs, void* out_transform) const override
			{
				*static_cast<transform_type*>(out_transform) = transform_math_type::mul(*static_cast<const transform_type*>(lhs), *static_cast<const transform_type*>(rhs));
			}

			virtual RTM_DISABLE_SECURITY_COOKIE_CHECK void local_to_object_space(const local_to_object_space_args& args, void* out_object_transforms) const override
			{
				const uint32_t* dirty_transform_indices = args.dirty_transform_indices;
				const uint32_t* parent_transform_indices = args.parent_transform_indices;
				const transform_type* local_transforms_ = static_cast<const transform_type*>(args.local_transforms);
				transform_type* out_object_transforms_ = static_cast<transform_type*>(out_object_transforms);

				const uint32_t num_dirty_transforms = args.num_dirty_transforms;
				for (uint32_t dirty_transform_index = 0; dirty_transform_index < num_dirty_transforms; ++dirty_transform_index)
				{
					const uint32_t transform_index = dirty_transform_indices[dirty_transform_index];
					const uint32_t parent_transform_index = parent_transform_indices[transform_index];

					if (parent_transform_index == k_invalid_track_index)
						out_object_transforms_[transform_index] = local_transforms_[transform_index];	// The root has no parent: its local and object space transforms are equal
					else
						out_object_transforms_[transform_index] = transform_math_type::mul(local_transforms_[transform_index], out_object_transforms_[parent_transform_index]);
				}
			}

			virtual void RTM_SIMD_CALL mul_point3_x4(
				rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
				const void* transform,
				rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) const override
			{
				transform_math_type::mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, *static_cast<const transform_type*>(transform), out_result_xxxx, out_result_yyyy, out_result_zzzz);
			}
		};
	}

	//////////////////////////////////////////////////////////////////////////
	// The transform adapter for VQ transforms: a rotation and a translation.
	// It uses rtm::vqvf and it does not use the scale. Compression uses it when a clip has no scale.
	//////////////////////////////////////////////////////////////////////////
	class vqf_transform_adapter final : public acl_impl::builtin_transform_adapter<acl_impl::vq_transform_math>
	{
	public:
		virtual const char* get_name() const override { return "vqf_transform_adapter"; }
	};

	//////////////////////////////////////////////////////////////////////////
	// The transform adapter for VQS transforms: a rotation, a translation, and a uniform scale.
	// It uses rtm::vqsf. The source transforms must have a uniform scale.
	//////////////////////////////////////////////////////////////////////////
	class vqsf_transform_adapter final : public acl_impl::builtin_transform_adapter<acl_impl::vqs_transform_math>
	{
	public:
		virtual const char* get_name() const override { return "vqsf_transform_adapter"; }
	};

	//////////////////////////////////////////////////////////////////////////
	// The transform adapter for VQV transforms: a rotation, a translation, and a 3D scale.
	// It uses rtm::vqvf. The product discards the shear, as in many game engines.
	//////////////////////////////////////////////////////////////////////////
	class vqvf_transform_adapter final : public acl_impl::builtin_transform_adapter<acl_impl::vqv_transform_math>
	{
	public:
		virtual const char* get_name() const override { return "vqvf_transform_adapter"; }
	};

	//////////////////////////////////////////////////////////////////////////
	// The transform adapter for VQM transforms: a rotation, a translation, and a 3x3 scale/shear matrix.
	// It uses rtm::vqmf. The product keeps the shear and gives exact object space transforms.
	//////////////////////////////////////////////////////////////////////////
	class vqmf_transform_adapter final : public acl_impl::builtin_transform_adapter<acl_impl::vqm_transform_math>
	{
	public:
		virtual const char* get_name() const override { return "vqmf_transform_adapter"; }
	};

	//////////////////////////////////////////////////////////////////////////
	// The transform adapter for 3x4 affine matrices.
	// It uses rtm::matrix3x4f. The product keeps the shear and gives exact object space transforms.
	//////////////////////////////////////////////////////////////////////////
	class matrix3x4f_transform_adapter final : public acl_impl::builtin_transform_adapter<acl_impl::matrix3x4_transform_math>
	{
	public:
		virtual const char* get_name() const override { return "matrix3x4f_transform_adapter"; }
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
