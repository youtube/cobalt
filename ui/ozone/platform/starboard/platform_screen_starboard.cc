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

#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "ui/display/tablet_state.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {
// Ozone display defaults.
constexpr int64_t kStarboardDisplayId = 1;
constexpr float kStarboardDisplayScale = 1.0f;
constexpr gfx::Size kStarboardDisplaySize(1, 1);

// Parse comma-separated screen width and height.
bool ParseScreenSize(const std::string& screen_size, int* width, int* height) {
  std::vector<base::StringPiece> width_and_height = base::SplitStringPiece(
      screen_size, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (width_and_height.size() != 2)
    return false;

  if (!base::StringToInt(width_and_height[0], width) ||
      !base::StringToInt(width_and_height[1], height)) {
    return false;
  }

  return true;
}

gfx::Rect GetDisplayBounds() {
  gfx::Rect bounds(kStarboardDisplaySize);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kOzoneOverrideScreenSize)) {
    int width, height;
    std::string screen_size =
        command_line.GetSwitchValueASCII(switches::kOzoneOverrideScreenSize);
    if (ParseScreenSize(screen_size, &width, &height)) {
      bounds.set_size(gfx::Size(width, height));
    }
  }

  return bounds;
}

}  // namespace

PlatformScreenStarboard::PlatformScreenStarboard() {
  display::Display display(kStarboardDisplayId);
  display.SetScaleAndBounds(kStarboardDisplayScale, GetDisplayBounds());
  display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
}

PlatformScreenStarboard::~PlatformScreenStarboard() = default;

const std::vector<display::Display>& PlatformScreenStarboard::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display PlatformScreenStarboard::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  DCHECK(iter != display_list_.displays().end());
  return *iter;
}

display::Display PlatformScreenStarboard::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return GetPrimaryDisplay();
}

gfx::Point PlatformScreenStarboard::GetCursorScreenPoint() const {
  return gfx::Point();
}

gfx::AcceleratedWidget PlatformScreenStarboard::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  return gfx::kNullAcceleratedWidget;
}

display::Display PlatformScreenStarboard::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return GetPrimaryDisplay();
}

display::Display PlatformScreenStarboard::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return GetPrimaryDisplay();
}

void PlatformScreenStarboard::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void PlatformScreenStarboard::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
