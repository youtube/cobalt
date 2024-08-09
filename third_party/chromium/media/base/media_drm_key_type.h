// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_DRM_KEY_TYPE_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_DRM_KEY_TYPE_H_

#include <stdint.h>

namespace media_m96 {

// These must be in sync with Android MediaDrm KEY_TYPE_XXX constants, except
// UNKNOWN and MAX:
// https://developer.android.com/reference/android/media/MediaDrm.html#KEY_TYPE_OFFLINE
enum class MediaDrmKeyType : uint32_t {
  UNKNOWN = 0,
  MIN = UNKNOWN,
  STREAMING = 1,
  OFFLINE = 2,
  RELEASE = 3,
  MAX = RELEASE,
};

}  // namespace media_m96
#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_DRM_KEY_TYPE_H_
