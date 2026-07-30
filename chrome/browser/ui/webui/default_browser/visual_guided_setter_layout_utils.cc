// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_layout_utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "base/numerics/ranges.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"

namespace visual_guided_setter {

namespace {

// Minimum dimensions of the WebUI docking area bounding box (in physical
// pixels) required to dock the Settings window. If the docking area is smaller
// than this, we degrade the flow to floating.
constexpr int kMinAnchorWidthPx = 320;
constexpr int kMinAnchorHeightPx = 160;

// Layout constants in DIPs. These values were determined based on the visual
// alignment with the native Windows Settings app to match the UX spec.
constexpr int kHorizontalInsetDip = 61;
constexpr int kBottomInsetDip = -8;
constexpr int kPreferredHeightDip = 220;
constexpr int kMinHeightDip = 180;

}  // namespace

bool IsAnchorLargeEnoughForDocking(const gfx::Rect& anchor_rect) {
  return anchor_rect.width() >= kMinAnchorWidthPx &&
         anchor_rect.height() >= kMinAnchorHeightPx;
}

gfx::Rect ComputeDockedSettingsRectFromAnchor(HWND chrome_hwnd,
                                              const gfx::Rect& anchor_rect_dip,
                                              const gfx::Rect& work_area_px) {
  // 1. Calculate the target bounds in DIPs.
  gfx::Rect target_dip = anchor_rect_dip;
  target_dip.Inset(gfx::Insets::TLBR(0, kHorizontalInsetDip, kBottomInsetDip,
                                     kHorizontalInsetDip));
  target_dip.set_y(target_dip.bottom() - kPreferredHeightDip);
  target_dip.set_height(kPreferredHeightDip);

  // 2. Translate to physical screen pixels.
  gfx::Rect target_px =
      display::win::GetScreenWin()->DIPToScreenRect(chrome_hwnd, target_dip);

  // 3. Minimum height enforcement in physical pixels.
  int min_height_px =
      display::win::GetScreenWin()
          ->DIPToScreenSize(chrome_hwnd, gfx::Size(0, kMinHeightDip))
          .height();

  int target_height_px =
      std::clamp(target_px.height(), min_height_px,
                 std::max(min_height_px, work_area_px.height()));

  target_px.set_y(target_px.bottom() - target_height_px);
  target_px.set_height(target_height_px);

  // 4. Clamp the final pixel rect to the work area.
  target_px.AdjustToFit(work_area_px);

  return target_px;
}

gfx::Point ComputeArrowStartPointFromAnchor(const gfx::Rect& anchor_rect) {
  return gfx::Point(anchor_rect.right(),
                    anchor_rect.y() + anchor_rect.height() / 2);
}

gfx::Point ComputeArrowEndPoint(const gfx::Rect& target_rect) {
  return target_rect.CenterPoint();
}

bool IsDpiCompatibleForDocking(HWND chrome_hwnd,
                               const gfx::Rect& target_screen_px) {
  if (!chrome_hwnd || !display::Screen::Get()) {
    return false;
  }

  // Passing a nullptr HWND instructs ScreenWin to evaluate scaling strictly
  // based on the monitor nearest to the physical rect.
  gfx::Rect target_dip =
      display::win::GetScreenWin()->ScreenToDIPRect(nullptr, target_screen_px);

  display::Display target_display =
      display::Screen::Get()->GetDisplayMatching(target_dip);

  float target_scale = target_display.device_scale_factor();
  float chrome_scale =
      display::win::GetScreenWin()->GetScaleFactorForHWND(chrome_hwnd);

  return base::IsApproximatelyEqual(target_scale, chrome_scale,
                                    std::numeric_limits<float>::epsilon());
}

}  // namespace visual_guided_setter
