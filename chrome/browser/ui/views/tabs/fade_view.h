// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FADE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FADE_VIEW_H_

#include "ui/color/color_provider.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

// Adapts any view `T` so that the view can fade when used by a FadeView
template <typename T, typename V>
class FadeWrapper : public T {
 public:
  template <typename... Args>
  explicit FadeWrapper(Args&&... args) : T(std::forward<Args>(args)...) {}

  virtual void SetData(const V& data) = 0;
  const V& GetData() { return data_; }

  // Sets the fade of this FadeWrapper's fade as |percent| in the range [0, 1]
  virtual void SetFade(double percent) = 0;

 protected:
  V data_;
};

// Cross fades any view so that view `U` is the old view that would be faded
// away. This allows view `T` to fade in for the user to ensure a smooth
// transition from the view with the old data `U` to the updated data in `T`
template <typename T, typename U, typename V>
class FadeView : public views::View {
 public:
  FadeView(std::unique_ptr<T> primary_view, std::unique_ptr<U> fade_out_view) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    primary_view_ = AddChildView(std::move(primary_view));
    fade_out_view_ = AddChildView(std::move(fade_out_view));
  }

  ~FadeView() override = default;

  void SetData(const V& data) {
    fade_out_view_->SetData(primary_view_->GetData());
    primary_view_->SetData(data);
  }

  // Sets the fade-out of the `fade_out_view_` as |percent| in the range [0, 1].
  // Since FadeView is designed to mask new view with the old and then fade
  // away, the higher the percentage the less opaque the view.
  void SetFade(double percent) {
    percent_ = percent;
    fade_out_view_->SetFade(percent);
    if (percent == 1.0) {
      fade_out_view_->SetData(V());
    }
  }

  gfx::Size GetMinimumSize() const override {
    return primary_view_->GetMinimumSize();
  }

  gfx::Size CalculatePreferredSize() const override {
    return primary_view_->CalculatePreferredSize();
  }

  int GetHeightForWidth(int width) const override {
    return primary_view_->GetHeightForWidth(width);
  }

  gfx::Size GetMaximumSize() const override {
    return gfx::Tween::SizeValueBetween(percent_,
                                        fade_out_view_->GetPreferredSize(),
                                        primary_view_->GetPreferredSize());
  }

 protected:
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterUpdatesTabAlertStatus);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterShowsDiscardStatus);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterShowsMemoryUsage);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardShowsMemoryOnMemoryRefresh);
  raw_ptr<T> primary_view_ = nullptr;
  raw_ptr<U> fade_out_view_ = nullptr;
  double percent_ = 1.0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FADE_VIEW_H_
