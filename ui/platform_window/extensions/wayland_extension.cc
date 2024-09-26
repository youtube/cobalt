// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/wayland_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WaylandExtension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(WaylandExtension*, kWaylandExtensionKey, nullptr)

WaylandExtension::~WaylandExtension() = default;

void WaylandExtension::SetWaylandExtension(PlatformWindow* window,
                                           WaylandExtension* extension) {
  window->SetProperty(kWaylandExtensionKey, extension);
}

WaylandExtension* GetWaylandExtension(const PlatformWindow& window) {
  return window.GetProperty(kWaylandExtensionKey);
}

}  // namespace ui
