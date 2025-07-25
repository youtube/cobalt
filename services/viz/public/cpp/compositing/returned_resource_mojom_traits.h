// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RETURNED_RESOURCE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RETURNED_RESOURCE_MOJOM_TRAITS_H_

#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/returned_resource.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::ReturnedResourceDataView,
                    viz::ReturnedResource> {
  static const viz::ResourceId& id(const viz::ReturnedResource& resource) {
    return resource.id;
  }

  static const gpu::SyncToken& sync_token(
      const viz::ReturnedResource& resource) {
    return resource.sync_token;
  }

  static gfx::GpuFenceHandle release_fence(
      const viz::ReturnedResource& resource) {
    return resource.release_fence.Clone();
  }

  static int32_t count(const viz::ReturnedResource& resource) {
    return resource.count;
  }

  static bool lost(const viz::ReturnedResource& resource) {
    return resource.lost;
  }

  static bool Read(viz::mojom::ReturnedResourceDataView data,
                   viz::ReturnedResource* out) {
    if (!data.ReadSyncToken(&out->sync_token) ||
        !data.ReadReleaseFence(&out->release_fence) || !data.ReadId(&out->id)) {
      return false;
    }
    out->count = data.count();
    out->lost = data.lost();
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RETURNED_RESOURCE_MOJOM_TRAITS_H_
