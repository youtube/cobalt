// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ui/ozone/platform/starboard/platform_screen_starboard.h"

#include "ui/display/display.h"

namespace ui {

namespace {

constexpr int64_t kFirstDisplayId = 1;
constexpr float kDefaultDeviceScaleFactor = 1.f;

}  // namespace

PlatformScreenStarboard::PlatformScreenStarboard() {}

PlatformScreenStarboard::~PlatformScreenStarboard() = default;

void PlatformScreenStarboard::InitScreen(const gfx::Rect& window_bounds) {
  // TODO(b/416313825): Derive this value without the commandline/hardcoding.
  //
  // One possible source is through the PlatformWindow handle via:
  //  -> PlatformWindowDelegate
  //     -> WebContents
  //        -> RenderWidgetHostView
  //
  // However this is not accessible since WebContents and RenderWidgetHostView
  // interfaces cannot be used in //ui/ozone/platform.
  float scale_factor = kDefaultDeviceScaleFactor;

  display::Display display(kFirstDisplayId);
  // We use the window bounds as a proxy for the display bounds because Chrobalt
  // is a fullscreen application and we expect these to be identical.
  display.SetScaleAndBounds(scale_factor, window_bounds);
  display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
}

const std::vector<display::Display>& PlatformScreenStarboard::GetAllDisplays()
    const {
  return display_list_.displays();
}

display::Display PlatformScreenStarboard::GetPrimaryDisplay() const {
  const auto& displays = GetAllDisplays();
  return displays[0];
}

display::Display PlatformScreenStarboard::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return GetPrimaryDisplay();
}
display::Display PlatformScreenStarboard::GetDisplayNearestPoint(
    const gfx::Point& point_in_dip) const {
  return GetPrimaryDisplay();
}

display::Display PlatformScreenStarboard::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return GetPrimaryDisplay();
}

gfx::Point PlatformScreenStarboard::GetCursorScreenPoint() const {
  // TODO(b/371272304): Return the actual screen point of the cursor.
  return gfx::Point();
}

gfx::AcceleratedWidget
PlatformScreenStarboard::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point_in_dip) const {
  // TODO(b/371272304): Check if a widget exists within the current window and
  // return that, otherwise return kNullAcceleratedWidget.
  return gfx::kNullAcceleratedWidget;
}

void PlatformScreenStarboard::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}
void PlatformScreenStarboard::RemoveObserver(
    display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
