// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/renderer/surface_embed_paint_holding_helper.h"

#include "base/functional/bind.h"
#include "cc/layers/deadline_policy.h"
#include "third_party/blink/public/common/widget/constants.h"

namespace surface_embed {

SurfaceEmbedPaintHoldingHelper::SurfaceEmbedPaintHoldingHelper() = default;
SurfaceEmbedPaintHoldingHelper::~SurfaceEmbedPaintHoldingHelper() = default;

void SurfaceEmbedPaintHoldingHelper::SetSurfaceId(
    cc::SurfaceLayer* layer,
    const viz::SurfaceId& surface_id,
    bool allow_paint_holding) {
  if (current_surface_id_ == surface_id) {
    return;
  }

  const viz::SurfaceId old_surface_id = current_surface_id_;
  current_surface_id_ = surface_id;
  paint_holding_timer_.Stop();

  layer->SetSurfaceId(surface_id, cc::DeadlinePolicy::UseDefaultDeadline());
  MaybeSetUpPaintHolding(layer, old_surface_id, allow_paint_holding);
}

void SurfaceEmbedPaintHoldingHelper::ClearPaintHolding(
    cc::SurfaceLayer* layer) {
  paint_holding_timer_.Stop();
  if (layer) {
    layer->SetOldestAcceptableFallback(viz::SurfaceId());
  }
  current_surface_id_ = viz::SurfaceId();
}

void SurfaceEmbedPaintHoldingHelper::MaybeSetUpPaintHolding(
    cc::SurfaceLayer* layer,
    const viz::SurfaceId& fallback_id,
    bool allow_paint_holding) {
  if (fallback_id.is_valid() && allow_paint_holding) {
    layer->SetOldestAcceptableFallback(fallback_id);

    paint_holding_timer_.Start(
        FROM_HERE, blink::kNewContentRenderingDelay,
        base::BindOnce(&SurfaceEmbedPaintHoldingHelper::PaintHoldingTimerFired,
                       base::Unretained(this), base::Unretained(layer)));
  } else {
    layer->SetOldestAcceptableFallback(viz::SurfaceId());
  }
}

void SurfaceEmbedPaintHoldingHelper::PaintHoldingTimerFired(
    cc::SurfaceLayer* layer) {
  layer->SetOldestAcceptableFallback(viz::SurfaceId());
}

}  // namespace surface_embed
