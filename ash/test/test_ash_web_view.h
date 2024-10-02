// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_ASH_WEB_VIEW_H_
#define ASH_TEST_TEST_ASH_WEB_VIEW_H_

#include "ash/public/cpp/ash_web_view.h"
#include "base/observer_list.h"
#include "url/gurl.h"

namespace views {
class View;
}  // namespace views
namespace ash {

// An implementation of AshWebView for use in unittests.
class TestAshWebView : public AshWebView {
 public:
  explicit TestAshWebView(const AshWebView::InitParams& init_params);
  ~TestAshWebView() override;

  TestAshWebView(const TestAshWebView&) = delete;
  TestAshWebView& operator=(const TestAshWebView&) = delete;

  // AshWebView:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  gfx::NativeView GetNativeView() override;
  bool GoBack() override;
  void Navigate(const GURL& url) override;
  views::View* GetInitiallyFocusedView() override;
  void RequestFocus() override;
  bool HasFocus() const override;

  const AshWebView::InitParams& init_params_for_testing() const {
    return init_params_;
  }

  // The most recent url that was requested via Navigate(). Will be empty if
  // Navigate() has not been called.
  const GURL& current_url() const { return current_url_; }

 private:
  base::ObserverList<Observer> observers_;
  bool focused_ = false;
  AshWebView::InitParams init_params_;
  GURL current_url_;

  base::WeakPtrFactory<TestAshWebView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_TEST_TEST_ASH_WEB_VIEW_H_
