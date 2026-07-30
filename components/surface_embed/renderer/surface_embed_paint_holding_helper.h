// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_PAINT_HOLDING_HELPER_H_
#define COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_PAINT_HOLDING_HELPER_H_

#include "base/component_export.h"
#include "base/timer/timer.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace surface_embed {

// Manages paint holding for a cc::SurfaceLayer used by SurfaceEmbedWebPlugin.
// When a surface transition occurs with paint holding enabled, the helper sets
// the old surface as a fallback so the compositor keeps displaying it until the
// new surface produces its first frame (or a timeout expires), preventing a
// visual flash during navigation.
//
// This is analogous to blink::ChildFrameCompositingHelper's paint holding logic
// for OOPIF RemoteFrames.
class COMPONENT_EXPORT(SURFACE_EMBED_RENDERER) SurfaceEmbedPaintHoldingHelper {
 public:
  SurfaceEmbedPaintHoldingHelper();
  ~SurfaceEmbedPaintHoldingHelper();

  SurfaceEmbedPaintHoldingHelper(const SurfaceEmbedPaintHoldingHelper&) =
      delete;
  SurfaceEmbedPaintHoldingHelper& operator=(
      const SurfaceEmbedPaintHoldingHelper&) = delete;

  // Updates the surface layer to display the given surface ID. If
  // `allow_paint_holding` is true and a previous surface was set, the old
  // surface will be used as a fallback until the new surface produces content
  // or a timeout expires.
  void SetSurfaceId(cc::SurfaceLayer* layer,
                    const viz::SurfaceId& surface_id,
                    bool allow_paint_holding);

  // Clears paint holding state. Should be called when the child process
  // crashes or the plugin is being destroyed.
  void ClearPaintHolding(cc::SurfaceLayer* layer);

  const viz::SurfaceId& current_surface_id() const {
    return current_surface_id_;
  }

 private:
  void MaybeSetUpPaintHolding(cc::SurfaceLayer* layer,
                              const viz::SurfaceId& fallback_id,
                              bool allow_paint_holding);
  void PaintHoldingTimerFired(cc::SurfaceLayer* layer);

  viz::SurfaceId current_surface_id_;
  base::OneShotTimer paint_holding_timer_;
};

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_RENDERER_SURFACE_EMBED_PAINT_HOLDING_HELPER_H_
