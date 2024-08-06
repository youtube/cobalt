// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/services/gpu_mojo_media_client.h"

namespace media_m96 {

std::unique_ptr<GpuMojoMediaClient::PlatformDelegate>
GpuMojoMediaClient::PlatformDelegate::Create(GpuMojoMediaClient* client) {
  return std::make_unique<GpuMojoMediaClient::PlatformDelegate>();
}

}  // namespace media_m96
