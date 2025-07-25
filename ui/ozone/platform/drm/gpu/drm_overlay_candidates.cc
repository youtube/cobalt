// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_candidates.h"

#include "media/media_buildflags.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_manager.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DrmOverlayCandidates::DrmOverlayCandidates(DrmOverlayManager* manager,
                                           gfx::AcceleratedWidget widget)
    : overlay_manager_(manager), widget_(widget) {}

DrmOverlayCandidates::~DrmOverlayCandidates() {
  overlay_manager_->RegisterOverlayRequirement(widget_, false);
  overlay_manager_->StopObservingHardwareCapabilities(widget_);
}

void DrmOverlayCandidates::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates) {
  overlay_manager_->CheckOverlaySupport(candidates, widget_);
}

void DrmOverlayCandidates::ObserveHardwareCapabilities(
    HardwareCapabilitiesCallback receive_callback) {
  overlay_manager_->StartObservingHardwareCapabilities(
      widget_, std::move(receive_callback));
}

void DrmOverlayCandidates::RegisterOverlayRequirement(bool requires_overlay) {
#if !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  DCHECK(!requires_overlay);
#endif
  overlay_manager_->RegisterOverlayRequirement(widget_, requires_overlay);
}

}  // namespace ui
