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
#include "acl/core/error_result.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/track_types.h"
#include "acl/compression/transform_adapters.h"
#include "acl/compression/transform_metrics.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// The transform adapter and the transform metric that compression uses for a clip.
		struct transform_runtime_selection
		{
			const itransform_adapter* adapter = nullptr;
			const itransform_metric* metric = nullptr;
		};

		//////////////////////////////////////////////////////////////////////////
		// Holds the built-in transform adapters and metrics, and selects the ones a clip uses.
		// See 'compression_settings::transform_runtime_type' and 'compression_settings::transform_metric'.
		class transform_runtime_selector
		{
		public:
			//////////////////////////////////////////////////////////////////////////
			// Returns the built-in transform adapter of a runtime transform type.
			// Returns 'null' with 'custom', 'automatic', or a value that is not valid.
			const itransform_adapter* get_builtin_adapter(transform_runtime_type_t type) const
			{
				switch (type)
				{
				case transform_runtime_type_t::vqf:			return &m_vqf_adapter;
				case transform_runtime_type_t::vqsf:		return &m_vqsf_adapter;
				case transform_runtime_type_t::vqvf:		return &m_vqvf_adapter;
				case transform_runtime_type_t::vqmf:		return &m_vqmf_adapter;
				case transform_runtime_type_t::matrix3x4f:	return &m_matrix3x4f_adapter;
				case transform_runtime_type_t::custom:
				case transform_runtime_type_t::automatic:
				default:									return nullptr;
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the default transform metric of a runtime transform type.
			// The precise metric gives the exact error for VQ and VQS transforms.
			// The shell metric gives an approximation for the other types.
			const itransform_metric* get_default_metric(transform_runtime_type_t type) const
			{
				if (type == transform_runtime_type_t::vqf || type == transform_runtime_type_t::vqsf)
					return &m_precise_metric;

				return &m_shell_metric;
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the built-in transform metrics.
			// Compression uses a faster code path when the selection uses them with a built-in transform adapter.
			const itransform_metric* get_shell_metric() const { return &m_shell_metric; }
			const itransform_metric* get_precise_metric() const { return &m_precise_metric; }

			//////////////////////////////////////////////////////////////////////////
			// Validates the runtime transform type settings without a clip.
			// We validate what depends on the clip in 'select'.
			error_result validate(transform_runtime_type_t type, const itransform_adapter* custom_adapter, const itransform_metric* metric) const
			{
				if (type > transform_runtime_type_t::automatic)
					return error_result("Invalid transform runtime type");

				if (type == transform_runtime_type_t::custom && custom_adapter == nullptr)
					return error_result("The 'custom' transform runtime type requires a custom transform adapter");

				if (type != transform_runtime_type_t::custom && custom_adapter != nullptr)
					return error_result("A custom transform adapter requires the 'custom' transform runtime type");

				if (metric != nullptr && type != transform_runtime_type_t::automatic)
				{
					const itransform_adapter* adapter = type == transform_runtime_type_t::custom ? custom_adapter : get_builtin_adapter(type);
					if (!metric->is_supported(*adapter))
						return error_result("The transform metric does not support the transform runtime type");
				}

				return error_result();
			}

			//////////////////////////////////////////////////////////////////////////
			// Selects the transform adapter and the transform metric of a clip.
			//    - 'source_type': the track type of the raw tracks
			//    - 'has_scale': whether or not the clip has a scale, after we apply the additive base (if any)
			//    - 'has_non_uniform_base_scale': whether or not the additive base (if any) has a scale that is not uniform
			error_result select(
				transform_runtime_type_t type, const itransform_adapter* custom_adapter, const itransform_metric* metric,
				track_type8 source_type, bool has_scale, bool has_non_uniform_base_scale,
				transform_runtime_selection& out_selection) const
			{
				const error_result result = validate(type, custom_adapter, metric);
				if (result.any())
					return result;

				// Without scale, each built-in runtime transform type gives the same transforms: we use the cheapest
				// We keep the selected type when the transform metric does not support VQ transforms
				// We always keep a custom transform adapter: its transforms can use data that the source scale does not show
				if (!has_scale && type != transform_runtime_type_t::custom && (metric == nullptr || metric->is_supported(m_vqf_adapter)))
				{
					out_selection.adapter = &m_vqf_adapter;
					out_selection.metric = metric != nullptr ? metric : get_default_metric(transform_runtime_type_t::vqf);
					return error_result();
				}

				// The compressor keeps the lossy scale uniform only with 'vqsf' raw tracks
				// An additive base with a scale that is not uniform gives a scale that is not uniform
				const bool can_use_vqsf = source_type == track_type8::vqsf && !has_non_uniform_base_scale;

				if (type == transform_runtime_type_t::automatic)
					type = can_use_vqsf ? transform_runtime_type_t::vqsf : transform_runtime_type_t::vqmf;
				else if (type == transform_runtime_type_t::vqsf && !can_use_vqsf)
					return error_result("The 'vqsf' transform runtime type requires 'vqsf' raw tracks and an additive base with a uniform scale");

				const itransform_adapter* adapter = type == transform_runtime_type_t::custom ? custom_adapter : get_builtin_adapter(type);
				const itransform_metric* selected_metric = metric != nullptr ? metric : get_default_metric(type);

				if (!selected_metric->is_supported(*adapter))
					return error_result("The transform metric does not support the transform runtime type");

				out_selection.adapter = adapter;
				out_selection.metric = selected_metric;
				return error_result();
			}

		private:
			vqf_transform_adapter			m_vqf_adapter;
			vqsf_transform_adapter			m_vqsf_adapter;
			vqvf_transform_adapter			m_vqvf_adapter;
			vqmf_transform_adapter			m_vqmf_adapter;
			matrix3x4f_transform_adapter	m_matrix3x4f_adapter;

			shell_transform_metric			m_shell_metric;
			precise_transform_metric		m_precise_metric;
		};
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
