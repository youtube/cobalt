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

PlatformScreenStarboard::PlatformScreenStarboard() {}

PlatformScreenStarboard::~PlatformScreenStarboard() = default;

const std::vector<display::Display>& PlatformScreenStarboard::GetAllDisplays()
    const {
  // TODO(b/371272304): Initialize display_list_ in the constructor.
  return display_list_.displays();
}

display::Display PlatformScreenStarboard::GetPrimaryDisplay() const {
  // TODO(b/371272304): Get the actual primary display from display_list_.
  return display::Display();
}
display::Display PlatformScreenStarboard::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  // TODO(b/371272304): Check if the widget exists within the current window and
  // maps to a display. If not, return primary display.
  return GetPrimaryDisplay();
}
display::Display PlatformScreenStarboard::GetDisplayNearestPoint(
    const gfx::Point& point_in_dip) const {
  // TODO(b/371272304): Iterate a display list and find the one nearest the
  // point.
  return GetPrimaryDisplay();
}
display::Display PlatformScreenStarboard::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  // TODO(b/371272304): Iterate a display list and find a matching display.
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
  // TODO(b/371272304): Add Observer to display::DisplayList.
}
void PlatformScreenStarboard::RemoveObserver(
    display::DisplayObserver* observer) {
  // TODO(b/371272304): Remove Observer from display::DisplayList.
}

}  // namespace ui
