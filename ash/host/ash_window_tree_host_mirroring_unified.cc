// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_mirroring_unified.h"

#include "ash/host/ash_window_tree_host_delegate.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ash {

AshWindowTreeHostMirroringUnified::AshWindowTreeHostMirroringUnified(
    const gfx::Rect& initial_bounds,
    int64_t mirroring_display_id,
    AshWindowTreeHostDelegate* delegate)
    : AshWindowTreeHostPlatform(
          ui::PlatformWindowInitProperties{initial_bounds},
          delegate),
      mirroring_display_id_(mirroring_display_id) {
  DCHECK(delegate_);
}

AshWindowTreeHostMirroringUnified::~AshWindowTreeHostMirroringUnified() =
    default;

gfx::Transform
AshWindowTreeHostMirroringUnified::GetRootTransformForLocalEventCoordinates()
    const {
  gfx::Transform trans = GetRootTransform();

  if (!is_shutting_down_) {
    const auto* display = delegate_->GetDisplayById(mirroring_display_id_);
    DCHECK(display);
    // Undo the translation in the root window transform, since this transform
    // should be applied on local points to this host.
    trans.Translate(SkIntToScalar(display->bounds().x()),
                    SkIntToScalar(display->bounds().y()));
  }

  return trans;
}

void AshWindowTreeHostMirroringUnified::ConvertDIPToPixels(
    gfx::PointF* point) const {
  // GetRootTransform() returns a transform that takes a point from the
  // *unified* host coordinates to the *mirroring* host's pixel coordinates.
  // ConvertDIPToPixels() and ConvertDIPToScreenInPixels() are called on local
  // points in the *mirroring* host's root window, not on points in the unified
  // host's. That's why we use the GetRootTransformForLocalEventCoordinates()
  // defined above, which only scales those local points to the right size, and
  // leaves the translation to be done by the MirroringScreenPositionClient
  // functions.
  *point = GetRootTransformForLocalEventCoordinates().MapPoint(*point);
}

void AshWindowTreeHostMirroringUnified::ConvertPixelsToDIP(
    gfx::PointF* point) const {
  *point = GetInverseRootTransformForLocalEventCoordinates().MapPoint(*point);
}

void AshWindowTreeHostMirroringUnified::PrepareForShutdown() {
  is_shutting_down_ = true;

  AshWindowTreeHostPlatform::PrepareForShutdown();
}

void AshWindowTreeHostMirroringUnified::OnMouseEnter() {
  // No logical display change in unified desktop mode,so do nothing.
}

}  // namespace ash
