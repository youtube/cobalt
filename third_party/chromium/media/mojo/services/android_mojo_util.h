// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_ANDROID_MOJO_UTIL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_ANDROID_MOJO_UTIL_H_

#include <memory>

#include "third_party/chromium/media/mojo/mojom/frame_interface_factory.mojom.h"
#include "third_party/chromium/media/mojo/services/mojo_media_drm_storage.h"
#include "third_party/chromium/media/mojo/services/mojo_provision_fetcher.h"

namespace media_m96 {
namespace android_mojo_util {

std::unique_ptr<ProvisionFetcher> CreateProvisionFetcher(
    media_m96::mojom::FrameInterfaceFactory* frame_interfaces);

std::unique_ptr<MediaDrmStorage> CreateMediaDrmStorage(
    media_m96::mojom::FrameInterfaceFactory* frame_interfaces);

}  // namespace android_mojo_util
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_ANDROID_MOJO_UTIL_H_
