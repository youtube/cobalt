// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_CONSTRAINTS_CONSTS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_CONSTRAINTS_CONSTS_H_

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// Possible values of the echo canceller type constraint.
PLATFORM_EXPORT extern const char kEchoCancellationTypeBrowser[];
PLATFORM_EXPORT extern const char kEchoCancellationTypeAec3[];
PLATFORM_EXPORT extern const char kEchoCancellationTypeSystem[];

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_CONSTRAINTS_CONSTS_H_
