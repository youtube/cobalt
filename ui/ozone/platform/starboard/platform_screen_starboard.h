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

#ifndef UI_OZONE_PLATFORM_STARBOARD_PLATFORM_SCREEN_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_PLATFORM_SCREEN_STARBOARD_H_

#include "ui/display/display_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

class PlatformScreenStarboard : public PlatformScreen {
 public:
  PlatformScreenStarboard();

  ~PlatformScreenStarboard() override;

  const std::vector<display::Display>& GetAllDisplays() const override;

  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point_in_dip) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;

  gfx::Point GetCursorScreenPoint() const override;

  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point_in_dip) const override;

  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  display::DisplayList display_list_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_SCREEN_STARBOARD_H_
