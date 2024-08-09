// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_

#include <fuchsia/media/cpp/fidl.h>

#include "fuchsia/mojom/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::AudioConsumerRequestDataView,
                    fidl::InterfaceRequest<fuchsia::media_m96::AudioConsumer>>
    : public FidlInterfaceRequestStructTraits<
          media_m96::mojom::AudioConsumerRequestDataView,
          fuchsia::media_m96::AudioConsumer> {};

template <>
struct StructTraits<media_m96::mojom::AudioCapturerRequestDataView,
                    fidl::InterfaceRequest<fuchsia::media_m96::AudioCapturer>>
    : public FidlInterfaceRequestStructTraits<
          media_m96::mojom::AudioCapturerRequestDataView,
          fuchsia::media_m96::AudioCapturer> {};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
