// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_H_

#include "components/viz/common/surfaces/local_surface_id.h"
#include "third_party/blink/public/common/common_export.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// TODO(fsamuel): We might want to unify this with content::ResizeParams.
struct BLINK_COMMON_EXPORT FrameVisualProperties {
  FrameVisualProperties();
  FrameVisualProperties(const FrameVisualProperties& other);
  ~FrameVisualProperties();

  FrameVisualProperties& operator=(const FrameVisualProperties& other);

  // These fields are values from VisualProperties, see comments there for
  // descriptions. They exist here to propagate from each RenderWidget to its
  // child RenderWidgets. Here they are flowing from RenderWidget in a parent
  // renderer process up to the RenderWidgetHost for a child RenderWidget in
  // another renderer process. That RenderWidgetHost would then be responsible
  // for passing it along to the child RenderWidget.
  display::ScreenInfos screen_infos;
  bool auto_resize_enabled = false;
  bool is_pinch_gesture_active = false;
  uint32_t capture_sequence_number = 0u;
  double zoom_level = 0;
  float page_scale_factor = 1.f;
  float compositing_scale_factor = 1.f;
  gfx::Size visible_viewport_size;
  gfx::Size min_size_for_auto_resize;
  gfx::Size max_size_for_auto_resize;
  std::vector<gfx::Rect> root_widget_window_segments;

  // The size of the compositor viewport, to match the sub-frame's surface.
  gfx::Rect compositor_viewport;

  // The frame's rect relative to the first ancestor local root frame.
  gfx::Rect rect_in_local_root;

  // The size of the frame in its parent's coordinate space.
  gfx::Size local_frame_size;

  // The time at which the viz::LocalSurfaceId used to submit this was
  // allocated.
  viz::LocalSurfaceId local_surface_id;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_VISUAL_PROPERTIES_H_
