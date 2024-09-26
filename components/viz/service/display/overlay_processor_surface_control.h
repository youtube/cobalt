// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_

#include "components/viz/service/display/overlay_processor_using_strategy.h"

namespace viz {

// This is an overlay processor implementation for Android SurfaceControl.
class VIZ_SERVICE_EXPORT OverlayProcessorSurfaceControl
    : public OverlayProcessorUsingStrategy {
 public:
  OverlayProcessorSurfaceControl();
  ~OverlayProcessorSurfaceControl() override;

  bool IsOverlaySupported() const override;

  bool NeedsSurfaceDamageRectList() const override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  void SetViewportSize(const gfx::Size& size) override;
  void AdjustOutputSurfaceOverlay(
      absl::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;
  void CheckOverlaySupportImpl(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const override;

 private:
  // Historically, android media was hardcoding color space to srgb. This
  // indicates that we going to use real one.
  const bool use_real_color_space_;
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Size viewport_size_;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_
