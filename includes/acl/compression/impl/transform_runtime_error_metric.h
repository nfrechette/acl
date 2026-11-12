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
#include "acl/core/additive_utils.h"
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/track_types.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_adapters.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/transform_metrics.h"
#include "acl/compression/impl/transform_runtime.h"
#include "acl/compression/impl/transform_track_utils.h"

#include <rtm/mask4f.h>
#include <rtm/vector4f.h>
#include <rtm/vqvf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

// This file reads the deprecated error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Compression uses the error metric interface internally.
		// This error metric implements that interface with the transform adapter and the
		// transform metric that the compression settings select:
		//    - the transform adapter converts and composes the transforms
		//    - the transform metric measures the error
		//    - ACL applies the additive base to the rtm::vqvf source transforms
		//
		// It holds two selections: one for a clip with scale, one for a clip without scale.
		// Compression knows whether the clip has scale only after it compacts the constant sub-tracks.
		// Until 'set_clip_has_scale' is called, the functions with scale use the selection with scale.
		// After, a clip without scale uses the selection without scale for all functions.
		//
		// The bit rate algorithm v2 does not use the functions of this interface: it uses the
		// selections directly, see 'get_selection'.
		//////////////////////////////////////////////////////////////////////////
		class transform_runtime_error_metric final : public itransform_error_metric
		{
		public:
			transform_runtime_error_metric() = default;

			// We hold a pointer to one of our members, we cannot be copied
			transform_runtime_error_metric(const transform_runtime_error_metric&) = delete;
			transform_runtime_error_metric& operator=(const transform_runtime_error_metric&) = delete;

			//////////////////////////////////////////////////////////////////////////
			// Selects the transform adapters and the transform metrics of a clip.
			// 'source_type' is the track type of the raw tracks.
			// 'additive_base' can be 'null' when the clip is not additive.
			error_result initialize(
				transform_runtime_type_t type, const itransform_adapter* custom_adapter, const itransform_metric* metric,
				track_type8 source_type, additive_clip_format8 additive_format, const track_array* additive_base)
			{
				m_additive_format = additive_format;

				// An additive base with scale gives a clip with scale
				// An additive base with a scale that is not uniform gives a scale that is not uniform
				bool base_has_scale = false;
				bool base_has_non_uniform_scale = false;
				if (additive_format != additive_clip_format8::none && additive_base != nullptr && !additive_base->is_empty())
					find_base_scale(*additive_base, base_has_scale, base_has_non_uniform_scale);

				m_base_has_scale = base_has_scale;

				error_result result = m_selector.select(type, custom_adapter, metric, source_type, true, base_has_non_uniform_scale, m_scale_selection);
				if (result.any())
					return result;

				result = m_selector.select(type, custom_adapter, metric, source_type, false, base_has_non_uniform_scale, m_no_scale_selection);
				if (result.any())
					return result;

				m_scale_function_selection = &m_scale_selection;
				return error_result();
			}

			//////////////////////////////////////////////////////////////////////////
			// Sets whether or not the clip has scale, before we apply the additive base.
			// Without scale in the clip and in the additive base, the functions with scale use the selection without scale.
			void set_clip_has_scale(bool clip_has_scale)
			{
				m_scale_function_selection = (clip_has_scale || m_base_has_scale) ? &m_scale_selection : &m_no_scale_selection;
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the adapter and the metric that the functions use.
			const transform_runtime_selection& get_selection(bool has_scale) const { return has_scale ? *m_scale_function_selection : m_no_scale_selection; }

			virtual const char* get_name() const override { return "transform_runtime_error_metric"; }

			virtual uint32_t get_hash() const override
			{
				uint32_t hash_value = hash32(get_name());
				hash_value = hash_combine(hash_value, m_scale_selection.adapter->get_hash());
				hash_value = hash_combine(hash_value, m_scale_selection.metric->get_hash());
				hash_value = hash_combine(hash_value, m_no_scale_selection.adapter->get_hash());
				hash_value = hash_combine(hash_value, m_no_scale_selection.metric->get_hash());
				hash_value = hash_combine(hash_value, hash32(m_additive_format));
				return hash_value;
			}

			virtual size_t get_transform_size(bool has_scale) const override { return get_selection(has_scale).adapter->get_transform_size(); }
			virtual bool needs_conversion(bool has_scale) const override { return get_selection(has_scale).adapter->needs_conversion(); }

			// The bit rate algorithm v2 uses the transform adapters and the transform metrics directly, see 'get_selection'
			virtual bool supports_bit_rate_algorithm_v2() const override { return true; }

			//////////////////////////////////////////////////////////////////////////
			// Returns the selector that holds the built-in transform adapters and metrics.
			const transform_runtime_selector& get_selector() const { return m_selector; }

			virtual void convert_transforms(const convert_transforms_args& args, void* out_transforms) const override { convert_transforms_impl(get_selection(true), args, out_transforms); }
			virtual void convert_transforms_no_scale(const convert_transforms_args& args, void* out_transforms) const override { convert_transforms_impl(get_selection(false), args, out_transforms); }

			virtual void local_to_object_space(const local_to_object_space_args& args, void* out_object_transforms) const override { local_to_object_space_impl(get_selection(true), args, out_object_transforms); }
			virtual void local_to_object_space_no_scale(const local_to_object_space_args& args, void* out_object_transforms) const override { local_to_object_space_impl(get_selection(false), args, out_object_transforms); }

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
					out_transforms_[transform_index] = acl::apply_additive_to_base(m_additive_format, base_transforms_[transform_index], local_transforms_[transform_index]);
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
					out_transforms_[transform_index] = acl::apply_additive_to_base_no_scale(m_additive_format, base_transforms_[transform_index], local_transforms_[transform_index]);
				}
			}

			virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const override { return calculate_error_squared_impl(get_selection(true), args); }
			virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared_no_scale(const calculate_error_args& args) const override { return calculate_error_squared_impl(get_selection(false), args); }

		private:
			static void convert_transforms_impl(const transform_runtime_selection& selection, const convert_transforms_args& args, void* out_transforms)
			{
				itransform_adapter::convert_transforms_args adapter_args;
				adapter_args.dirty_transform_indices = args.dirty_transform_indices;
				adapter_args.num_dirty_transforms = args.num_dirty_transforms;
				adapter_args.transforms = args.transforms;
				adapter_args.num_transforms = args.num_transforms;
				adapter_args.sample_index = args.sample_index;
				adapter_args.is_lossy = args.is_lossy;

				selection.adapter->convert_transforms(adapter_args, out_transforms);
			}

			static void local_to_object_space_impl(const transform_runtime_selection& selection, const local_to_object_space_args& args, void* out_object_transforms)
			{
				itransform_adapter::local_to_object_space_args adapter_args;
				adapter_args.dirty_transform_indices = args.dirty_transform_indices;
				adapter_args.num_dirty_transforms = args.num_dirty_transforms;
				adapter_args.parent_transform_indices = args.parent_transform_indices;
				adapter_args.local_transforms = args.local_transforms;
				adapter_args.num_transforms = args.num_transforms;

				selection.adapter->local_to_object_space(adapter_args, out_object_transforms);
			}

			static rtm::scalarf RTM_SIMD_CALL calculate_error_squared_impl(const transform_runtime_selection& selection, const calculate_error_args& args)
			{
				itransform_metric::calculate_error_args metric_args;
#if defined(ACL_IMPL_USE_SOA_ERROR_METRIC)
				metric_args.shell_points_xxxx = args.shell_points_xxxx;
				metric_args.shell_points_yyyy = args.shell_points_yyyy;
				metric_args.shell_points_zzzz = args.shell_points_zzzz;
				metric_args.shell_distance = rtm::vector_get_x(args.shell_point_x);
#else
				metric_args.construct_sphere_shell(rtm::vector_get_x(args.shell_point_x));
#endif
				metric_args.adapter = selection.adapter;
				metric_args.transform0 = args.transform0;
				metric_args.transform1 = args.transform1;

				return selection.metric->calculate_error_squared(metric_args);
			}

			// Finds whether or not the additive base has scale, and whether or not its scale is uniform
			static void find_base_scale(const track_array& additive_base, bool& out_has_scale, bool& out_has_non_uniform_scale)
			{
				const rtm::vector4f identity_scale = rtm::vector_set(1.0F);

				rtm::mask4f is_identity = rtm::mask_set(true, true, true, true);
				bool is_uniform = true;

				const uint32_t num_samples = additive_base.get_num_samples_per_track();
				for (const track& base_track : additive_base)
				{
					for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
					{
						const rtm::vector4f scale = get_transform_track_sample(base_track, sample_index, identity_scale).scale;
						is_identity = rtm::mask_and(rtm::vector_equal(scale, identity_scale), is_identity);
						is_uniform &= rtm::vector_all_equal3(scale, rtm::vector_dup_x(scale));
					}
				}

				out_has_scale = !rtm::mask_all_true3(is_identity);
				out_has_non_uniform_scale = !is_uniform;
			}

			transform_runtime_selector m_selector;

			transform_runtime_selection m_scale_selection;
			transform_runtime_selection m_no_scale_selection;
			const transform_runtime_selection* m_scale_function_selection = &m_scale_selection;

			additive_clip_format8 m_additive_format = additive_clip_format8::none;
			bool m_base_has_scale = false;
		};

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not transform tracks have a scale that is not the default scale.
		// The default scale is 0.0 with the 'additive1' format, and 1.0 otherwise.
		inline bool transform_tracks_have_scale(const track_array& tracks, additive_clip_format8 additive_format)
		{
			if (tracks.is_empty() || !is_transform_track_type(tracks.get_track_type()))
				return false;

			const rtm::vector4f default_scale = rtm::vector_set(additive_format == additive_clip_format8::additive1 ? 0.0F : 1.0F);

			rtm::mask4f is_default = rtm::mask_set(true, true, true, true);

			const uint32_t num_samples = tracks.get_num_samples_per_track();
			for (const track& track_ : tracks)
			{
				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					const rtm::vector4f scale = get_transform_track_sample(track_, sample_index, default_scale).scale;
					is_default = rtm::mask_and(rtm::vector_equal(scale, default_scale), is_default);
				}
			}

			return !rtm::mask_all_true3(is_default);
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the error metric that measures the error of transform tracks as compression does with these settings.
		// With the legacy error metric, it returns it. Otherwise, it initializes the runtime error metric and returns it.
		// The runtime error metric knows whether the clip has scale from the raw tracks and the additive base.
		// Returns 'null' when the settings are not valid for these tracks.
		inline const itransform_error_metric* get_settings_error_metric(
			const compression_settings& settings, const track_array& raw_tracks,
			additive_clip_format8 additive_format, const track_array* additive_base,
			transform_runtime_error_metric& runtime_error_metric)
		{
			if (settings.error_metric != nullptr)
				return settings.error_metric;

			// An empty track array has no type, we use vqvf
			const track_type8 track_type = raw_tracks.is_empty() ? track_type8::vqvf : raw_tracks.get_track_type();

			const error_result result = runtime_error_metric.initialize(
				settings.transform_runtime_type, settings.custom_transform_adapter, settings.transform_metric,
				track_type, additive_format, additive_base);
			if (result.any())
				return nullptr;

			runtime_error_metric.set_clip_has_scale(transform_tracks_have_scale(raw_tracks, additive_format));
			return &runtime_error_metric;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_DEPRECATION_WARNING_POP

ACL_IMPL_FILE_PRAGMA_POP
