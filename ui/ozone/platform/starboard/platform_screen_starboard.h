// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_STARBOARD_PLATFORM_SCREEN_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_PLATFORM_SCREEN_STARBOARD_H_

#include <vector>

#include "base/observer_list.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

class PlatformScreenStarboard : public PlatformScreen {
 public:
  PlatformScreenStarboard();

  PlatformScreenStarboard(const PlatformScreenStarboard&) = delete;
  PlatformScreenStarboard& operator=(const PlatformScreenStarboard&) = delete;

  ~PlatformScreenStarboard() override;

  // Overridden from ui::PlatformScreen:
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  display::TabletState GetTabletState() const override;
#endif

 private:
  display::DisplayList display_list_;

  base::ObserverList<display::DisplayObserver> observers_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_SCREEN_STARBOARD_H_
