// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"

#include <surface-augmenter-client-protocol.h>

#include <cstdint>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace {

// Returns DIP bounds of the subsurface relative to the parent surface.
gfx::RectF AdjustSubsurfaceBounds(const gfx::RectF& bounds_px,
                                  const gfx::RectF& parent_bounds_px,
                                  float buffer_scale) {
  const auto bounds_dip = gfx::ScaleRect(bounds_px, 1.0f / buffer_scale);
  const auto parent_bounds_dip =
      gfx::ScaleRect(parent_bounds_px, 1.0f / buffer_scale);
  return wl::TranslateBoundsToParentCoordinatesF(bounds_dip, parent_bounds_dip);
}

const wl_fixed_t kMinusOne = wl_fixed_from_int(-1);

}  // namespace

namespace ui {

WaylandSubsurface::WaylandSubsurface(WaylandConnection* connection,
                                     WaylandWindow* parent)
    : wayland_surface_(connection, parent),
      connection_(connection),
      parent_(parent) {
  DCHECK(parent_);
  DCHECK(connection_);
  if (!surface()) {
    LOG(ERROR) << "Failed to create wl_surface";
    return;
  }
  wayland_surface_.Initialize();
}

WaylandSubsurface::~WaylandSubsurface() = default;

gfx::AcceleratedWidget WaylandSubsurface::GetWidget() const {
  return wayland_surface_.get_widget();
}

void WaylandSubsurface::Show() {
  if (visible_) {
    return;
  }

  if (subsurface_) {
    ResetSubsurface();
  }

  CreateSubsurface();
  visible_ = true;
}

void WaylandSubsurface::Hide() {
  if (!IsVisible() || !subsurface_) {
    return;
  }

  // Remove it from the stack.
  RemoveFromList();
  visible_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, subsurfaces need to be reset when hide to avoid glitches
  // (crbug.com/1408073). On Linux, however, we need to keep the subsurfaces to
  // make sure the window closing animation works well (crbug.com/1324548).
  ResetSubsurface();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void WaylandSubsurface::ResetSubsurface() {
  augmented_subsurface_.reset();
  subsurface_.reset();
  wayland_surface_.UnsetRootWindow();
}

bool WaylandSubsurface::IsVisible() const {
  return visible_;
}

void WaylandSubsurface::CreateSubsurface() {
  DCHECK(parent_);
  wayland_surface_.SetRootWindow(parent_);

  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  subsurface_ = wayland_surface()->CreateSubsurface(parent_->root_surface());
  position_dip_ = {0, 0};

  // A new sub-surface is initially added as the top-most in the stack.
  parent_->subsurface_stack_committed()->Append(this);

  DCHECK(subsurface_);
  wl_subsurface_set_sync(subsurface_.get());

  // Subsurfaces don't need to trap input events. Its display rect is fully
  // contained in |parent_|'s. Setting input_region to empty allows |parent_| to
  // dispatch all of the input to platform window.
  gfx::Rect region_px;
  wayland_surface()->set_input_region(&region_px);

  if (connection_->surface_augmenter()) {
    // |augmented_subsurface| might be null if the protocol's version is not
    // high enough.
    augmented_subsurface_ =
        connection_->surface_augmenter()->CreateAugmentedSubSurface(
            subsurface_.get());
  }
}

void WaylandSubsurface::ConfigureAndShowSurface(
    const gfx::RectF& bounds_px,
    const gfx::RectF& parent_bounds_px,
    const absl::optional<gfx::Rect>& clip_rect_px,
    float buffer_scale,
    WaylandSubsurface* new_below,
    WaylandSubsurface* new_above) {
  Show();

  // Chromium positions quads in display::Display coordinates in physical
  // pixels, but Wayland requires them to be in local surface coordinates a.k.a
  // relative to parent window.
  auto bounds_dip_in_parent_surface = AdjustSubsurfaceBounds(
      bounds_px, parent_bounds_px,
      connection_->surface_submission_in_pixel_coordinates() ? 1.f
                                                             : buffer_scale);
  if (bounds_dip_in_parent_surface.origin() != position_dip_) {
    position_dip_ = bounds_dip_in_parent_surface.origin();
    if (augmented_subsurface_) {
      DCHECK(
          connection_->surface_augmenter()->SupportsSubpixelAccuratePosition());
      augmented_sub_surface_set_position(
          augmented_subsurface_.get(),
          wl_fixed_from_double(bounds_dip_in_parent_surface.x()),
          wl_fixed_from_double(bounds_dip_in_parent_surface.y()));
    } else {
      gfx::Rect enclosed_rect_in_parent =
          gfx::ToEnclosedRect(bounds_dip_in_parent_surface);
      wl_subsurface_set_position(subsurface_.get(), enclosed_rect_in_parent.x(),
                                 enclosed_rect_in_parent.y());
    }
  }

  if (augmented_subsurface_ &&
      connection_->surface_augmenter()->SupportsClipRect()) {
    absl::optional<gfx::RectF> clip_dip_in_parent_surface;
    if (clip_rect_px) {
      clip_dip_in_parent_surface = AdjustSubsurfaceBounds(
          gfx::RectF(*clip_rect_px), parent_bounds_px,
          connection_->surface_submission_in_pixel_coordinates()
              ? 1.f
              : buffer_scale);
    }
    if (clip_dip_in_parent_surface != clip_dip_) {
      clip_dip_ = clip_dip_in_parent_surface;
      if (clip_dip_) {
        augmented_sub_surface_set_clip_rect(
            augmented_subsurface_.get(), wl_fixed_from_double(clip_dip_->x()),
            wl_fixed_from_double(clip_dip_->y()),
            wl_fixed_from_double(clip_dip_->width()),
            wl_fixed_from_double(clip_dip_->height()));
      } else {
        // Call set_clip_rect with all values -1 to clear the clip rect.
        augmented_sub_surface_set_clip_rect(augmented_subsurface_.get(),
                                            kMinusOne, kMinusOne, kMinusOne,
                                            kMinusOne);
      }
    }
  }

  // Setup the stacking order of this subsurface.
  DCHECK(!new_above || !new_below);
  if (new_below && new_below != previous()) {
    DCHECK_EQ(parent_, new_below->parent_);
    RemoveFromList();
    InsertAfter(new_below);
    wl_subsurface_place_above(subsurface_.get(), new_below->surface());
  } else if (new_above && new_above != next()) {
    DCHECK_EQ(parent_, new_above->parent_);
    RemoveFromList();
    InsertBefore(new_above);
    wl_subsurface_place_below(subsurface_.get(), new_above->surface());
  }
}

}  // namespace ui
