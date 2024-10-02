// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_FRAME_H_

#include <memory>
#include <vector>

#include "components/viz/service/viz_service_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/latency/latency_info.h"

namespace viz {

// Metadata given to the OutputSurface for it to swap what was drawn and make
// current frame visible.
class VIZ_SERVICE_EXPORT OutputSurfaceFrame {
 public:
  OutputSurfaceFrame();
  OutputSurfaceFrame(OutputSurfaceFrame&& other);

  OutputSurfaceFrame(const OutputSurfaceFrame&) = delete;
  OutputSurfaceFrame& operator=(const OutputSurfaceFrame&) = delete;

  ~OutputSurfaceFrame();

  OutputSurfaceFrame& operator=(OutputSurfaceFrame&& other);

  gfx::Size size;
  // Providing both |sub_buffer_rect| and |content_bounds| is not supported;
  // if neither is present, regular swap is used.
  // Optional rect for partial or empty swap.
  absl::optional<gfx::Rect> sub_buffer_rect;
  // Optional content area for SwapWithBounds. Rectangles may overlap.
  std::vector<gfx::Rect> content_bounds;
  std::vector<ui::LatencyInfo> latency_info;
  absl::optional<int64_t> choreographer_vsync_id;
  bool top_controls_visible_height_changed = false;
  // FrameData for GLSurface.
  gfx::FrameData data;
  // Metadata containing information to draw a delegated ink trail using
  // platform APIs.
  std::unique_ptr<gfx::DelegatedInkMetadata> delegated_ink_metadata;
#if BUILDFLAG(IS_APPLE)
  gfx::CALayerResult ca_layer_error_code = gfx::kCALayerSuccess;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_FRAME_H_
