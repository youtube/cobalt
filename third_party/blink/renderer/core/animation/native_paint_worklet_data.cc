// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/native_paint_worklet_data.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace {

NativePaintWorkletData::CompositedPaintStatus
CalculateStatusFromNativePaintReasons(
    Animation::NativePaintWorkletReasons animation_type,
    Animation::NativePaintWorkletReasons aggregated_reasons,
    Animation::NativePaintWorkletReasons overlapping_reasons) {
  if (animation_type & aggregated_reasons) {
    return animation_type & overlapping_reasons
               ? NativePaintWorkletData::CompositedPaintStatus::kNotComposited
               : NativePaintWorkletData::CompositedPaintStatus::kNeedsRepaint;
  }
  return NativePaintWorkletData::CompositedPaintStatus::kNoAnimation;
}

}  // namespace

bool NativePaintWorkletData::SetStatus(CompositedPaintStatus status) {
  if (composited_paint_status_ != status) {
    composited_paint_status_ = status;
    if (status == CompositedPaintStatus::kNotComposited ||
        status == CompositedPaintStatus::kNoAnimation) {
      if (animation_ && animation_->HasActiveAnimationsOnCompositor()) {
        animation_->SetCompositorPending(
            Animation::CompositorPendingReason::kPendingDowngrade);
      }
      animation_ = nullptr;
    }
    return true;
  }

  return false;
}

void NativePaintWorkletData::TriggerPaintInvalidation() {
  if (element_->GetLayoutObject()) {
    element_->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
    if (update_triggers_paint_property_update_) {
      element_->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
    }
  }
}

bool NativePaintWorkletData::UpdateCompositedPaintStatus(
    Animation::NativePaintWorkletReasons aggregated_reasons,
    Animation::NativePaintWorkletReasons overlapping_reasons) {
  NativePaintWorkletData::CompositedPaintStatus status =
      CalculateStatusFromNativePaintReasons(npw_property_, aggregated_reasons,
                                            overlapping_reasons);
  bool changed = SetStatus(status);
  if (changed) {
    TriggerPaintInvalidation();
  }

  return changed;
}

void NativePaintWorkletData::SetNeedsKeyframeSnapshot() {
  if (composited_paint_status_ == CompositedPaintStatus::kComposited) {
    bool changed = SetStatus(CompositedPaintStatus::kNeedsRepaint);
    if (changed) {
      TriggerPaintInvalidation();
      // TODO(kevers): Once the worklet input is stored in this case, we'll need
      // to invalidate the input here as well to ensure we don't used the cached
      // keyframes.
    }
  }
}

void NativePaintWorkletData::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(animation_);
}

}  // namespace blink
