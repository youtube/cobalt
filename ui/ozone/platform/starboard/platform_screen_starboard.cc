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
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "content/shell/common/shell_switches.h"
#include "ui/display/display_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#include "ui/display/display.h"

namespace ui {

namespace {
constexpr int64_t kFirstDisplayId = 1;
}  // namespace

PlatformScreenStarboard::PlatformScreenStarboard() {
  display::Display display(kFirstDisplayId);

  // Screen size is the same as window size.
  // Cobalt is always treated as a fullscreen application.
  gfx::Rect bounds(gfx::Size(1, 1));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kContentShellHostWindowSize)) {
    int width, height;
    std::string screen_size =
        command_line.GetSwitchValueASCII(switches::kContentShellHostWindowSize);
    std::vector<base::StringPiece> width_and_height = base::SplitStringPiece(
        screen_size, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (!(width_and_height.size() == 2 &&
          base::StringToInt(width_and_height[0], &width) &&
          base::StringToInt(width_and_height[1], &height))) {
      // This is a hack to retrieve the window size from the command line args.
      width = 1280;
      height = 1024;
    }
    bounds.set_size(gfx::Size(width, height));
  }

  double device_scale_factor = 1.0f;
  if (command_line.HasSwitch(switches::kForceDeviceScaleFactor)) {
    // This is also a hack to receive this from the command line args.
    std::string device_scale_factor_str =
        command_line.GetSwitchValueASCII(switches::kForceDeviceScaleFactor);
    base::StringToDouble(device_scale_factor_str, &device_scale_factor);
  }

  display.SetScaleAndBounds(device_scale_factor, bounds);
  display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
}

PlatformScreenStarboard::~PlatformScreenStarboard() = default;

const std::vector<display::Display>& PlatformScreenStarboard::GetAllDisplays()
    const {
  return display_list_.displays();
}

void PlatformScreenStarboard::InitScreen() {
  // TODO: Logic for finding displays through correct calls to compoenents with
  // knowledge of these properties.
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
