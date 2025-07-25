// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/transferable_resource_mojom_traits.h"

#include "build/build_config.h"
#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

// static
viz::mojom::SynchronizationType
EnumTraits<viz::mojom::SynchronizationType,
           viz::TransferableResource::SynchronizationType>::
    ToMojom(viz::TransferableResource::SynchronizationType type) {
  switch (type) {
    case viz::TransferableResource::SynchronizationType::kSyncToken:
      return viz::mojom::SynchronizationType::kSyncToken;
    case viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted:
      return viz::mojom::SynchronizationType::kGpuCommandsCompleted;
    case viz::TransferableResource::SynchronizationType::kReleaseFence:
      return viz::mojom::SynchronizationType::kReleaseFence;
  }
  NOTREACHED();
  return viz::mojom::SynchronizationType::kSyncToken;
}

// static
bool EnumTraits<viz::mojom::SynchronizationType,
                viz::TransferableResource::SynchronizationType>::
    FromMojom(viz::mojom::SynchronizationType input,
              viz::TransferableResource::SynchronizationType* out) {
  switch (input) {
    case viz::mojom::SynchronizationType::kSyncToken:
      *out = viz::TransferableResource::SynchronizationType::kSyncToken;
      return true;
    case viz::mojom::SynchronizationType::kGpuCommandsCompleted:
      *out =
          viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
      return true;
    case viz::mojom::SynchronizationType::kReleaseFence:
      *out = viz::TransferableResource::SynchronizationType::kReleaseFence;
      return true;
  }
  return false;
}

// static
bool StructTraits<viz::mojom::TransferableResourceDataView,
                  viz::TransferableResource>::
    Read(viz::mojom::TransferableResourceDataView data,
         viz::TransferableResource* out) {
  viz::ResourceId id;
  if (!data.ReadSize(&out->size) || !data.ReadFormat(&out->format) ||
      !data.ReadMailboxHolder(&out->mailbox_holder) ||
      !data.ReadColorSpace(&out->color_space) ||
      !data.ReadColorSpaceWhenSampled(&out->color_space_when_sampled) ||
      !data.ReadHdrMetadata(&out->hdr_metadata) ||
      !data.ReadYcbcrInfo(&out->ycbcr_info) || !data.ReadId(&id) ||
      !data.ReadSynchronizationType(&out->synchronization_type)) {
    return false;
  }
  out->id = id;
  out->is_software = data.is_software();
  out->is_overlay_candidate = data.is_overlay_candidate();

#if BUILDFLAG(IS_ANDROID)
  out->is_backed_by_surface_texture = data.is_backed_by_surface_texture();
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  out->wants_promotion_hint = data.wants_promotion_hint();
#endif

  return true;
}

}  // namespace mojo
