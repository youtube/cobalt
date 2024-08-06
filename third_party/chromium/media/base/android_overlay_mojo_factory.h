// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_OVERLAY_MOJO_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_OVERLAY_MOJO_FACTORY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "third_party/chromium/media/base/android_overlay_config.h"

namespace media_m96 {

// Note that this compiles on non-android too.
using AndroidOverlayMojoFactoryCB =
    base::RepeatingCallback<std::unique_ptr<AndroidOverlay>(
        const base::UnguessableToken&,
        AndroidOverlayConfig)>;

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_OVERLAY_MOJO_FACTORY_H_
