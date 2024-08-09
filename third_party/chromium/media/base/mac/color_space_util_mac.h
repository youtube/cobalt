// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>

#include "third_party/chromium/media/base/media_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

namespace media_m96 {

MEDIA_EXPORT gfx::ColorSpace GetImageBufferColorSpace(
    CVImageBufferRef image_buffer);

MEDIA_EXPORT gfx::ColorSpace GetFormatDescriptionColorSpace(
    CMFormatDescriptionRef format_description);

MEDIA_EXPORT CFDataRef
GenerateContentLightLevelInfo(const gfx::HDRMetadata& hdr_metadata);

MEDIA_EXPORT CFDataRef
GenerateMasteringDisplayColorVolume(const gfx::HDRMetadata& hdr_metadata);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_
