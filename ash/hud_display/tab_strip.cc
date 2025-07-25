// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/tab_strip.h"

#include <cmath>

#include "ash/hud_display/hud_display.h"
#include "ash/hud_display/hud_properties.h"
#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {
namespace hud_display {
namespace {

// The width in pixels of overlaying adjacent tabs. Must be an even number.
constexpr int kHUDTabOverlayWidth = 2 * kHUDTabOverlayCornerRadius / 3;

// Border around tab text (the tab overlay width will be added to this).
constexpr int kHUDTabTitleBorder = 3;

class HUDTabStripLayout : public views::LayoutManager {
 public:
  HUDTabStripLayout() = default;

  HUDTabStripLayout(const HUDTabStripLayout&) = delete;
  HUDTabStripLayout& operator=(const HUDTabStripLayout&) = delete;

  ~HUDTabStripLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override;
  gfx::Size GetPreferredSize(const views::View* host) const override;
};

gfx::Size HUDTabStripLayout::GetPreferredSize(const views::View* host) const {
  gfx::Size result;
  for (const auto* child : host->children()) {
    const gfx::Size child_preferred = child->GetPreferredSize();
    // Tab strip is always horizontal.
    result.set_width(result.width() + child_preferred.width() -
                     kHUDTabOverlayWidth);
    result.set_height(std::max(result.height(), child_preferred.height()));
  }
  // Assume all children have equal left and right border, which is used to
  // overlay the tabs. Add one overlay width to compensate one edge.
  if (host->children().size())
    result.set_width(result.width() + kHUDTabOverlayWidth);

  // Add right padding equal to the padding of the settings icon.
  result.set_width(result.width() + kHUDSettingsIconBorder);
  return result;
}

void HUDTabStripLayout::Layout(views::View* host) {
  // Assume all children have equal left and right border, which is used to
  // overlay the tabs.
  int left_offset = 0;
  for (auto* child : host->children()) {
    const gfx::Size preferred = child->GetPreferredSize();
    const gfx::Size child_size({preferred.width(), host->height()});
    child->SetSize(child_size);
    child->SetPosition({left_offset, 0});
    left_offset += child_size.width() - kHUDTabOverlayWidth;
  }
}

}  // namespace

BEGIN_METADATA(HUDTabButton, views::LabelButton)
END_METADATA

HUDTabButton::HUDTabButton(Style style,
                           const HUDDisplayMode display_mode,
                           const std::u16string& text)
    : views::LabelButton(views::Button::PressedCallback(), text),
      style_(style),
      display_mode_(display_mode) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetEnabledTextColors(kHUDDefaultColor);
  SetProperty(kHUDClickHandler, HTCLIENT);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kHUDSettingsIconBorder, kHUDTabOverlayWidth + kHUDTabTitleBorder,
      kHUDSettingsIconBorder, kHUDTabOverlayWidth + kHUDTabTitleBorder)));

  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
}

void HUDTabButton::SetStyle(Style style) {
  if (style_ == style)
    return;

  style_ = style;
  SchedulePaint();
}

void HUDTabButton::PaintButtonContents(gfx::Canvas* canvas) {
  // Horizontal offset from tab {0,0} where two tab arcs cross.
  constexpr int kTabArcCrossedX = kHUDTabOverlayWidth / 2;

  // Reduce kTabArcCrossedX by one pixel when calculating partial arc so that
  // the pixels along kTabArcCrossedX vertical line are drawn by full arc only.
  static const float kTabPartialArcAngle =
      90 - 180 *
               asinf((kHUDTabOverlayCornerRadius - kTabArcCrossedX + 1) /
                     (float)kHUDTabOverlayCornerRadius) /
               M_PI;

  constexpr SkScalar kCircleSize = kHUDTabOverlayCornerRadius * 2;
  const SkScalar right_edge = width();
  const SkScalar bottom_edge = height();

  SkPath path;

  // Draw left vertical line and arc
  if (style_ == Style::RIGHT) {
    /* |true| - move to the start of the arc */
    path.arcTo({0, 0, kCircleSize, kCircleSize}, -90 - kTabPartialArcAngle,
               kTabPartialArcAngle, true);
  } else {
    if (style_ == Style::LEFT) {
      // Draw bottom line from the right edge. Adjust for 2 pixels crossing the
      // right vertical line.
      path.moveTo(right_edge - kHUDTabOverlayWidth / 2 - 2, bottom_edge);
      path.lineTo(0, bottom_edge);
    } else {
      // No bottom line. Just move to the start of the vertical line.
      path.moveTo(0, bottom_edge);
    }
    /* |false| will draw straight line to the start of the arc */
    path.arcTo({0, 0, kCircleSize - 1, kCircleSize}, -180, 90, false);
  }
  // Draw top line, right arc and right vertical line
  if (style_ == Style::LEFT) {
    /* |false| will draw straight line to the start of the arc */
    path.arcTo({right_edge - kCircleSize, 0, right_edge, kCircleSize}, -90,
               kTabPartialArcAngle, false);
  } else {
    /* |false| will draw straight line to the start of the arc */
    path.arcTo({right_edge - kCircleSize, 0, right_edge, kCircleSize}, -90, 90,
               false);
    path.lineTo(right_edge, bottom_edge);
    if (style_ == Style::RIGHT) {
      // Draw bottom line to the left edge. Adjust for 2 pixels crossing the
      // left vertical line.
      path.lineTo(kHUDTabOverlayWidth / 2 + 2, bottom_edge);
    }
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setColor(kHUDDefaultColor);
  canvas->DrawPath(path, flags);
}

BEGIN_METADATA(HUDTabStrip, views::View)
END_METADATA

HUDTabStrip::HUDTabStrip(HUDDisplayView* hud) : hud_(hud) {
  SetLayoutManager(std::make_unique<HUDTabStripLayout>());
}

HUDTabStrip::~HUDTabStrip() = default;

HUDTabButton* HUDTabStrip::AddTabButton(const HUDDisplayMode display_mode,
                                        const std::u16string& label) {
  CHECK_NE(static_cast<int>(display_mode), 0);
  // Make first tab active by default.
  HUDTabButton* tab_button = AddChildView(std::make_unique<HUDTabButton>(
      tabs_.size() ? HUDTabButton::Style::RIGHT : HUDTabButton::Style::ACTIVE,
      display_mode, label));
  tab_button->SetCallback(base::BindRepeating(
      [](HUDTabButton* sender, HUDTabStrip* tab_strip) {
        for (const auto* tab : tab_strip->tabs_) {
          if (tab == sender) {
            tab_strip->hud_->SetDisplayMode(tab->display_mode());
            return;
          }
        }
        NOTREACHED();
      },
      base::Unretained(tab_button), base::Unretained(this)));
  tabs_.push_back(tab_button);
  return tab_button;
}

void HUDTabStrip::ActivateTab(const HUDDisplayMode mode) {
  // True if we find given active tab.
  bool found = false;

  for (HUDTabButton* tab : tabs_) {
    if (found) {
      tab->SetStyle(HUDTabButton::Style::RIGHT);
      continue;
    }
    if (tab->display_mode() == mode) {
      found = true;
      tab->SetStyle(HUDTabButton::Style::ACTIVE);
      continue;
    }
    tab->SetStyle(HUDTabButton::Style::LEFT);
  }
  DCHECK(found);
}

}  // namespace hud_display
}  // namespace ash
