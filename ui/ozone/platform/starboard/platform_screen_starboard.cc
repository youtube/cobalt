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

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "ui/display/display.h"

namespace ui {

namespace {

constexpr int64_t kFirstDisplayId = 1;
constexpr float kDefaultDeviceScaleFactor = 1.f;

namespace switches {
// NOTE: This is a redefinition of the same shell switch declared in:
//  content/shell/common/shell_switches.h
// but due to lack of visibility into the targets that define it, we redefine it
// here in order to query the command line for its initial configuration.
const char kContentShellHostWindowSize[] = "content-shell-host-window-size";
}  // namespace switches

gfx::Rect GetWindowSizeFromCommandLine() {
  // TODO(b/416313825): This method is part of the workaround which derives the
  // screen size value from the command line. This is a temporary workaround and
  // should be removed when the proper fix is developed.
  gfx::Rect bounds(gfx::Size(1920, 1080));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kContentShellHostWindowSize)) {
    int width, height;
    std::string screen_size =
        command_line.GetSwitchValueASCII(switches::kContentShellHostWindowSize);
    std::vector<std::string_view> width_and_height = base::SplitStringPiece(
        screen_size, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (!(width_and_height.size() == 2 &&
          base::StringToInt(width_and_height[0], &width) &&
          base::StringToInt(width_and_height[1], &height))) {
      return bounds;
    }
    bounds.set_size(gfx::Size(width, height));
  }
  return bounds;
}

}  // namespace

PlatformScreenStarboard::PlatformScreenStarboard() {}

PlatformScreenStarboard::~PlatformScreenStarboard() = default;

void PlatformScreenStarboard::InitScreen() {
  display::Display display(kFirstDisplayId);

  // TODO(b/416313825): Derive this value without the commandline/hardcoding.
  gfx::Rect window_size = GetWindowSizeFromCommandLine();

  // TODO(b/416313825): Derive this value without the commandline/hardcoding.
  float scale_factor = kDefaultDeviceScaleFactor;

  // We use the window bounds as a proxy for the display bounds because Chrobalt
  // is a fullscreen application and we expect these to be identical.
  display.SetScaleAndBounds(scale_factor, window_size);
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
