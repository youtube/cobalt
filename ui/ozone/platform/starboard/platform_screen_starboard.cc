// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  LOG(INFO) << "PlatformScreenStarboard::PlatformScreenStarboard";
  display::Display display(kStarboardDisplayId);
  display.SetScaleAndBounds(kStarboardDisplayScale, GetDisplayBounds());
  display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
}

PlatformScreenStarboard::~PlatformScreenStarboard() = default;

const std::vector<display::Display>& PlatformScreenStarboard::GetAllDisplays() const {
  LOG(INFO) << "PlatformScreenStarboard::GetAllDisplays";
  return display_list_.displays();
}

display::Display PlatformScreenStarboard::GetPrimaryDisplay() const {
  //LOG(INFO) << "PlatformScreenStarboard::GetPrimaryDisplay";
  auto iter = display_list_.GetPrimaryDisplayIterator();
  DCHECK(iter != display_list_.displays().end());
  return *iter;
}

display::Display PlatformScreenStarboard::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  //LOG(INFO) << "PlatformScreenStarboard::GetDisplayForAcceleratedWidget";
  return GetPrimaryDisplay();
}

gfx::Point PlatformScreenStarboard::GetCursorScreenPoint() const {
  //LOG(INFO) << "PlatformScreenStarboard::GetCursorScreenPoint";
  return gfx::Point();
}

gfx::AcceleratedWidget PlatformScreenStarboard::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  LOG(INFO) << "PlatformScreenStarboard::GetAcceleratedWidgetAtScreenPoint";
  return gfx::kNullAcceleratedWidget;
}

display::Display PlatformScreenStarboard::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  LOG(INFO) << "PlatformScreenStarboard::GetDisplayNearestPoint";
  return GetPrimaryDisplay();
}

display::Display PlatformScreenStarboard::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  LOG(INFO) << "PlatformScreenStarboard::GetDisplayMatching";
  return GetPrimaryDisplay();
}

void PlatformScreenStarboard::AddObserver(display::DisplayObserver* observer) {
  LOG(INFO) << "PlatformScreenStarboard::AddObserver";
  display_list_.AddObserver(observer);
}

void PlatformScreenStarboard::RemoveObserver(display::DisplayObserver* observer) {
  LOG(INFO) << "PlatformScreenStarboard::RemoveObserver";
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
