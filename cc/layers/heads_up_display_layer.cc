// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::Create() {
  return base::WrapRefCounted(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer()
    : typeface_(SkTypeface::MakeFromName("Arial", SkFontStyle())) {
  if (!typeface_.Read(*this)) {
    typeface_.Write(*this) =
        SkTypeface::MakeFromName("monospace", SkFontStyle::Bold());
  }
  DCHECK(typeface_.Read(*this).get());
  SetIsDrawable(true);
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer() = default;

void HeadsUpDisplayLayer::UpdateLocationAndSize(
    const gfx::Size& device_viewport,
    float device_scale_factor) {
  float multiplier = 1.f / (device_scale_factor *
                            layer_tree_host()->painted_device_scale_factor());
  gfx::Size device_viewport_in_dips =
      gfx::ScaleToFlooredSize(device_viewport, multiplier);

  gfx::Size bounds_in_dips;

  // If the HUD is not displaying full-viewport rects (e.g., it is showing the
  // Frame Rendering Stats), use a fixed size.
  constexpr int kDefaultHUDSize = 256;
  bounds_in_dips.SetSize(kDefaultHUDSize, kDefaultHUDSize);

  if (layer_tree_host()->GetDebugState().ShowDebugRects()) {
    bounds_in_dips = device_viewport_in_dips;
  } else if (layer_tree_host()->GetDebugState().show_web_vital_metrics ||
             layer_tree_host()->GetDebugState().show_smoothness_metrics) {
    // If the HUD is used to display performance metrics (which is on the right
    // hand side_, make sure the bounds has the correct width, with a fixed
    // height.
    bounds_in_dips.set_width(device_viewport_in_dips.width());
    // Increase HUD layer height to make sure all the metrics are showing.
    bounds_in_dips.set_height(kDefaultHUDSize * 2);
  }

  // DIPs are layout coordinates if painted dsf is 1. If it's not 1, then layout
  // coordinates are DIPs * painted dsf.
  auto bounds_in_layout_space = gfx::ScaleToCeiledSize(
      bounds_in_dips, layer_tree_host()->painted_device_scale_factor());

  SetBounds(bounds_in_layout_space);
}

bool HeadsUpDisplayLayer::HasDrawableContent() const {
  return true;
}

std::unique_ptr<LayerImpl> HeadsUpDisplayLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id());
}

const std::vector<gfx::Rect>& HeadsUpDisplayLayer::LayoutShiftRects() const {
  return layout_shift_rects_.Read(*this);
}

void HeadsUpDisplayLayer::SetLayoutShiftRects(
    const std::vector<gfx::Rect>& rects) {
  layout_shift_rects_.Write(*this) = rects;
}

void HeadsUpDisplayLayer::UpdateWebVitalMetrics(
    std::unique_ptr<WebVitalMetrics> web_vital_metrics) {
  web_vital_metrics_.Write(*this) = std::move(web_vital_metrics);
}

void HeadsUpDisplayLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  Layer::PushPropertiesTo(layer, commit_state, unsafe_state);
  TRACE_EVENT0("cc", "HeadsUpDisplayLayer::PushPropertiesTo");
  HeadsUpDisplayLayerImpl* layer_impl =
      static_cast<HeadsUpDisplayLayerImpl*>(layer);

  layer_impl->SetHUDTypeface(typeface_.Write(*this));
  layer_impl->SetLayoutShiftRects(LayoutShiftRects());
  layout_shift_rects_.Write(*this).clear();
  auto& metrics = web_vital_metrics_.Write(*this);
  if (metrics && metrics->HasValue())
    layer_impl->SetWebVitalMetrics(std::move(metrics));
}

}  // namespace cc
