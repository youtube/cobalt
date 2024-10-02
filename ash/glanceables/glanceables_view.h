// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
}  // namespace views

namespace ash {

class GlanceablesUpNextView;
class GlanceablesWeatherView;
class GlanceablesWelcomeLabel;

// Container view for the "welcome back" glanceables screen shown on login.
class ASH_EXPORT GlanceablesView : public views::View {
 public:
  GlanceablesView();
  GlanceablesView(const GlanceablesView&) = delete;
  GlanceablesView& operator=(const GlanceablesView&) = delete;
  ~GlanceablesView() override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;

 private:
  friend class GlanceablesTest;

  raw_ptr<views::BoxLayout, ExperimentalAsh> layout_ = nullptr;
  raw_ptr<GlanceablesWelcomeLabel, ExperimentalAsh> welcome_label_ = nullptr;
  raw_ptr<GlanceablesWeatherView, ExperimentalAsh> weather_view_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> up_next_label_ = nullptr;
  raw_ptr<GlanceablesUpNextView, ExperimentalAsh> up_next_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_VIEW_H_
