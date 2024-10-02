// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_overlay_manager.h"

#include "base/logging.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_overlay_candidates.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

namespace {

void NotifyOverlayDelegationLimitedCapabilityOnce() {
  static bool logged_once = false;
  if (!logged_once) {
    DLOG(ERROR)
        << "Subpixel accurate position is not available. Only some quads "
           "can be forwarded as overlays.";
    logged_once = true;
  }
}

}  // namespace

WaylandOverlayManager::WaylandOverlayManager(
    WaylandBufferManagerGpu* manager_gpu)
    : manager_gpu_(manager_gpu) {}
WaylandOverlayManager::~WaylandOverlayManager() = default;

std::unique_ptr<OverlayCandidatesOzone>
WaylandOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<WaylandOverlayCandidates>(this, widget);
}

void WaylandOverlayManager::SetContextDelegated() {
  is_delegated_context_ = true;
}

void WaylandOverlayManager::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates,
    gfx::AcceleratedWidget widget) {
  for (auto& candidate : *candidates) {
    bool can_handle = CanHandleCandidate(candidate, widget);

    // CanHandleCandidate() should never return false if the candidate is
    // the primary plane.
    DCHECK(can_handle || candidate.plane_z_order != 0);

    candidate.overlay_handled = can_handle;
  }
}

bool WaylandOverlayManager::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate,
    gfx::AcceleratedWidget widget) const {
  if (candidate.buffer_size.IsEmpty())
    return false;

  if (!manager_gpu_->SupportsFormat(candidate.format))
    return false;

  // Setting the OverlayCandidate::|uv_rect| will eventually result in setting
  // the |crop_rect_| in wayland. If this results in an empty pixel scale the
  // wayland connection will be terminated. See: wayland_surface.cc
  // 'ApplyPendingState'
  // Because of the device scale factor (kMaxDeviceScaleFactor) we check against
  // a rect who's size is empty when converted to fixed point number.
  // TODO(https://crbug.com/1218678) : Move and generalize this fix in wayland
  // host.
  auto viewport_src =
      gfx::ScaleRect(candidate.crop_rect, candidate.buffer_size.width(),
                     candidate.buffer_size.height());

  constexpr int kAssumedMaxDeviceScaleFactor = 8;
  if (wl_fixed_from_double(viewport_src.width() /
                           kAssumedMaxDeviceScaleFactor) == 0 ||
      wl_fixed_from_double(viewport_src.height() /
                           kAssumedMaxDeviceScaleFactor) == 0)
    return false;

  // Passing an empty surface size through wayland will actually clear the size
  // restriction and display the buffer at full size. The function
  // 'set_destination_size' in augmenter will accept empty sizes without
  // protocol error but interprets this as a clear.
  // TODO(https://crbug.com/1306230) : Move and generalize this fix in wayland
  // host.
  if (wl_fixed_from_double(candidate.display_rect.width() /
                           kAssumedMaxDeviceScaleFactor) == 0 ||
      wl_fixed_from_double(candidate.display_rect.height() /
                           kAssumedMaxDeviceScaleFactor) == 0)
    return false;

  if (candidate.transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  if (candidate.background_color.has_value() &&
      !manager_gpu_->supports_surface_background_color()) {
    return false;
  }

  // If clipping isn't supported, reject candidates with a clip rect, unless
  // that clip wouldn't have any effect.
  if (!manager_gpu_->supports_clip_rect() && candidate.clip_rect &&
      !candidate.clip_rect->Contains(
          gfx::ToNearestRect(candidate.display_rect))) {
    return false;
  }

  if (is_delegated_context_) {
    // Support for subpixel accurate position could be checked in ctor, but the
    // WaylandBufferManagerGpu is not initialized when |this| is created. Thus,
    // do checks here.
    if (manager_gpu_->supports_subpixel_accurate_position())
      return true;
    else
      NotifyOverlayDelegationLimitedCapabilityOnce();
  }

  // Reject candidates that don't fall on a pixel boundary.
  if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f))
    return false;

  return true;
}

}  // namespace ui
