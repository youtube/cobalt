// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_

#include <optional>

#include "build/build_config.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/common/vulkan_ycbcr_info_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"
#include "services/viz/public/mojom/compositing/transferable_resource.mojom-shared.h"
#include "skia/public/mojom/image_info_mojom_traits.h"
#include "skia/public/mojom/surface_origin_mojom_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::SynchronizationType,
                  viz::TransferableResource::SynchronizationType> {
  static viz::mojom::SynchronizationType ToMojom(
      viz::TransferableResource::SynchronizationType type);

  static bool FromMojom(viz::mojom::SynchronizationType input,
                        viz::TransferableResource::SynchronizationType* out);
};

template <>
struct EnumTraits<viz::mojom::ResourceSource,
                  viz::TransferableResource::ResourceSource> {
  static viz::mojom::ResourceSource ToMojom(
      viz::TransferableResource::ResourceSource source);

  static bool FromMojom(viz::mojom::ResourceSource input,
                        viz::TransferableResource::ResourceSource* out);
};

template <>
struct StructTraits<viz::mojom::TransferableResourceDataView,
                    viz::TransferableResource> {
  static const viz::ResourceId& id(const viz::TransferableResource& resource) {
    return resource.id;
  }

  static viz::SharedImageFormat format(
      const viz::TransferableResource& resource) {
    return resource.format;
  }

  static gfx::Size size(const viz::TransferableResource& resource) {
    return resource.size;
  }

  static gpu::Mailbox memory_buffer_id(
      const viz::TransferableResource& resource) {
    return resource.memory_buffer_id();
  }

  static const gpu::SyncToken& sync_token(
      const viz::TransferableResource& resource) {
    return resource.sync_token();
  }

  static uint32_t texture_target(const viz::TransferableResource& resource) {
    return resource.texture_target();
  }

  static viz::TransferableResource::SynchronizationType synchronization_type(
      const viz::TransferableResource& resource) {
    return resource.synchronization_type;
  }

  static bool is_software(const viz::TransferableResource& resource) {
    return resource.is_software;
  }

  static bool is_overlay_candidate(const viz::TransferableResource& resource) {
    return resource.is_overlay_candidate;
  }

  static bool is_low_latency_rendering(
      const viz::TransferableResource& resource) {
    return resource.is_low_latency_rendering;
  }

  static bool is_backed_by_surface_view(
      const viz::TransferableResource& resource) {
#if BUILDFLAG(IS_ANDROID)
    // TransferableResource has this in an #ifdef, but mojo doesn't let us.
    // TODO(crbug.com/40496893)
    return resource.is_backed_by_surface_view;
#else
    return false;
#endif
  }

  static bool wants_promotion_hint(const viz::TransferableResource& resource) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
    // TransferableResource has this in an #ifdef, but mojo doesn't let us.
    // TODO(crbug.com/40496893)
    return resource.wants_promotion_hint;
#else
    return false;
#endif
  }

  static const gfx::ColorSpace& color_space(
      const viz::TransferableResource& resource) {
    return resource.color_space;
  }

  static const gfx::HDRMetadata& hdr_metadata(
      const viz::TransferableResource& resource) {
    return resource.hdr_metadata;
  }

  static bool needs_detiling(const viz::TransferableResource& resource) {
    return resource.needs_detiling;
  }

  static const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info(
      const viz::TransferableResource& resource) {
    return resource.ycbcr_info;
  }

  static GrSurfaceOrigin origin(const viz::TransferableResource& resource) {
    return resource.origin;
  }

  static SkAlphaType alpha_type(const viz::TransferableResource& resource) {
    return resource.alpha_type;
  }

  static viz::TransferableResource::ResourceSource resource_source(
      const viz::TransferableResource& resource) {
    return resource.resource_source;
  }

  static bool Read(viz::mojom::TransferableResourceDataView data,
                   viz::TransferableResource* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_
