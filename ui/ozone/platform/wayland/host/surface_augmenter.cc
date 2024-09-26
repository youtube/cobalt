// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/surface_augmenter.h"

#include <surface-augmenter-client-protocol.h>
#include <wayland-util.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 4;
}

// static
constexpr char SurfaceAugmenter::kInterfaceName[];

// static
void SurfaceAugmenter::Instantiate(WaylandConnection* connection,
                                   wl_registry* registry,
                                   uint32_t name,
                                   const std::string& interface,
                                   uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->surface_augmenter_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto augmenter = wl::Bind<surface_augmenter>(registry, name,
                                               std::min(version, kMaxVersion));
  if (!augmenter) {
    LOG(ERROR) << "Failed to bind surface_augmenter";
    return;
  }
  connection->surface_augmenter_ =
      std::make_unique<SurfaceAugmenter>(augmenter.release(), connection);
}

SurfaceAugmenter::SurfaceAugmenter(surface_augmenter* surface_augmenter,
                                   WaylandConnection* connection)
    : augmenter_(surface_augmenter) {}

SurfaceAugmenter::~SurfaceAugmenter() = default;

bool SurfaceAugmenter::SupportsSubpixelAccuratePosition() const {
  return GetSurfaceAugmentorVersion() >=
         SURFACE_AUGMENTER_GET_AUGMENTED_SUBSURFACE_SINCE_VERSION;
}

bool SurfaceAugmenter::SupportsClipRect() const {
  return GetSurfaceAugmentorVersion() >=
         AUGMENTED_SUB_SURFACE_SET_CLIP_RECT_SINCE_VERSION;
}

uint32_t SurfaceAugmenter::GetSurfaceAugmentorVersion() const {
  return surface_augmenter_get_version(augmenter_.get());
}

wl::Object<augmented_surface> SurfaceAugmenter::CreateAugmentedSurface(
    wl_surface* surface) {
  return wl::Object<augmented_surface>(
      surface_augmenter_get_augmented_surface(augmenter_.get(), surface));
}

wl::Object<augmented_sub_surface> SurfaceAugmenter::CreateAugmentedSubSurface(
    wl_subsurface* subsurface) {
  if (!SupportsSubpixelAccuratePosition())
    return {};

  return wl::Object<augmented_sub_surface>(
      surface_augmenter_get_augmented_subsurface(augmenter_.get(), subsurface));
}

wl::Object<wl_buffer> SurfaceAugmenter::CreateSolidColorBuffer(
    const SkColor4f& color,
    const gfx::Size& size) {
  wl_array color_data;
  wl_array_init(&color_data);
  wl::SkColorToWlArray(color, color_data);
  auto buffer =
      wl::Object<wl_buffer>(surface_augmenter_create_solid_color_buffer(
          augmenter_.get(), &color_data, size.width(), size.height()));
  wl_array_release(&color_data);
  return buffer;
}

}  // namespace ui
