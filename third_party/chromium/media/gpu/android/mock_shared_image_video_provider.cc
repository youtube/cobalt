// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/gpu/android/mock_shared_image_video_provider.h"

namespace media_m96 {

MockSharedImageVideoProvider::RequestImageArgs::RequestImageArgs(
    ImageReadyCB cb,
    ImageSpec spec)
    : cb_(std::move(cb)), spec_(std::move(spec)) {}

MockSharedImageVideoProvider::RequestImageArgs::~RequestImageArgs() = default;

MockSharedImageVideoProvider::MockSharedImageVideoProvider()
    : weak_factory_(this) {}

MockSharedImageVideoProvider::~MockSharedImageVideoProvider() = default;

}  // namespace media_m96
