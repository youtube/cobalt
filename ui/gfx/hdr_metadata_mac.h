// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_METADATA_MAC_H_
#define UI_GFX_HDR_METADATA_MAC_H_

#include "base/mac/scoped_cftyperef.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space_export.h"

#include <CoreFoundation/CoreFoundation.h>

namespace gfx {

struct HDRMetadata;

// This can be used for rendering content using AVSampleBufferDisplayLayer via
// the key kCVImageBufferContentLightLevelInfoKey or for rendering content using
// a CAMetalLayer via CAEDRMetadata.
COLOR_SPACE_EXPORT base::ScopedCFTypeRef<CFDataRef>
GenerateContentLightLevelInfo(
    const absl::optional<gfx::HDRMetadata>& hdr_metadata);

// This can be used for rendering content using AVSampleBufferDisplayLayer via
// the key kCVImageBufferMasteringDisplayColorVolumeKey or for rendering content
// using a CAMetalLayer via CAEDRMetadata.
COLOR_SPACE_EXPORT base::ScopedCFTypeRef<CFDataRef>
GenerateMasteringDisplayColorVolume(
    const absl::optional<gfx::HDRMetadata>& hdr_metadata);

}  // namespace gfx

#endif  // UI_GFX_HDR_METADATA_MAC_H_
