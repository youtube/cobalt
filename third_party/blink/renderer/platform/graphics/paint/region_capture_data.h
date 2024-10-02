// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REGION_CAPTURE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REGION_CAPTURE_DATA_H_

#include "base/containers/flat_map.h"
#include "base/token.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// Represents a map from a region capture crop identifier, which is a
// randomly generated token, to a rectangle representing the bounds
// of the HTML element associated with the crop identifier.
// See the design document at:
// https://docs.google.com/document/d/1dULARMnMZggfWqa_Ti_GrINRNYXGIli3XK9brzAKEV
using RegionCaptureData = base::flat_map<RegionCaptureCropId, gfx::Rect>;

PLATFORM_EXPORT String ToString(const RegionCaptureData& data);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REGION_CAPTURE_DATA_H_
