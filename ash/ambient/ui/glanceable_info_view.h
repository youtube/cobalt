// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_
#define ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_

#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/ambient/model/ambient_weather_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class AmbientViewDelegate;
class TimeView;

// Container for displaying a glanceable clock and weather info.
class GlanceableInfoView : public views::View,
                           public AmbientWeatherModelObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the color for time and temperature text in |GlanceableInfoView|.
    virtual SkColor GetTimeTemperatureFontColor() = 0;
  };

  METADATA_HEADER(GlanceableInfoView);

  GlanceableInfoView(
      AmbientViewDelegate* delegate,
      GlanceableInfoView::Delegate* glanceable_info_view_delegate,
      int time_font_size_dip);
  GlanceableInfoView(const GlanceableInfoView&) = delete;
  GlanceableInfoView& operator=(const GlanceableInfoView&) = delete;
  ~GlanceableInfoView() override;

  // views::View:
  void OnThemeChanged() override;

  // AmbientWeatherModelObserver:
  void OnWeatherInfoUpdated() override;

  void Show();

  int GetTimeFontDescent();

 private:
  void InitLayout();

  std::u16string GetTemperatureText() const;

  // View for the time info. Owned by the view hierarchy.
  raw_ptr<TimeView, ExperimentalAsh> time_view_ = nullptr;

  // Views for weather icon and temperature.
  raw_ptr<views::ImageView, ExperimentalAsh> weather_condition_icon_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> temperature_ = nullptr;

  // Owned by |AmbientController|.
  const raw_ptr<AmbientViewDelegate, ExperimentalAsh> delegate_ = nullptr;

  // Unowned. Must out live |GlancealeInfoView|.
  raw_ptr<GlanceableInfoView::Delegate> const glanceable_info_view_delegate_ =
      nullptr;

  const int time_font_size_dip_;

  base::ScopedObservation<AmbientWeatherModel, AmbientWeatherModelObserver>
      scoped_weather_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_GLANCEABLE_INFO_VIEW_H_
