// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_MOJOM_FUCHSIA_CDM_PROVIDER_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_MOJOM_FUCHSIA_CDM_PROVIDER_MOJOM_TRAITS_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include "fuchsia/mojom/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<
    media_m96::mojom::CdmRequestDataView,
    fidl::InterfaceRequest<fuchsia::media_m96::drm::ContentDecryptionModule>>
    : public FidlInterfaceRequestStructTraits<
          media_m96::mojom::CdmRequestDataView,
          fuchsia::media_m96::drm::ContentDecryptionModule> {};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_MOJOM_FUCHSIA_CDM_PROVIDER_MOJOM_TRAITS_H_
