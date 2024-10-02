// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SUBTREE_CAPTURE_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SUBTREE_CAPTURE_ID_MOJOM_TRAITS_H_

#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "services/viz/public/mojom/compositing/subtree_capture_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::SubtreeCaptureIdDataView,
                    viz::SubtreeCaptureId> {
  static uint32_t subtree_id(const viz::SubtreeCaptureId& subtree_capture_id) {
    return subtree_capture_id.subtree_id();
  }

  static bool Read(viz::mojom::SubtreeCaptureIdDataView data,
                   viz::SubtreeCaptureId* out) {
    *out = viz::SubtreeCaptureId(data.subtree_id());
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SUBTREE_CAPTURE_ID_MOJOM_TRAITS_H_
