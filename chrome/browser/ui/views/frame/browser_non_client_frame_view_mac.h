// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_

#include "base/memory/raw_ptr.h"

#import <CoreGraphics/CGBase.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/prefs/pref_member.h"

namespace views {
class Label;
}

@class FullscreenToolbarController;

class CaptionButtonPlaceholderContainer;

class BrowserNonClientFrameViewMac : public BrowserNonClientFrameView,
                                     public web_app::AppRegistrarObserver {
 public:
  // Mac implementation of BrowserNonClientFrameView.
  BrowserNonClientFrameViewMac(BrowserFrame* frame, BrowserView* browser_view);

  BrowserNonClientFrameViewMac(const BrowserNonClientFrameViewMac&) = delete;
  BrowserNonClientFrameViewMac& operator=(const BrowserNonClientFrameViewMac&) =
      delete;

  ~BrowserNonClientFrameViewMac() override;

  // BrowserNonClientFrameView:
  void OnFullscreenStateChanged() override;
  bool CaptionButtonsOnLeadingEdge() const override;
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateFullscreenTopUI() override;
  bool ShouldHideTopUIForFullscreen() const override;
  void UpdateThrobber(bool running) override;
  void PaintAsActiveChanged() override;
  void OnThemeChanged() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void UpdateWindowIcon() override;
  void SizeConstraintsChanged() override;
  void UpdateMinimumSize() override;
  void WindowControlsOverlayEnabledChanged() override;

  // views::View:
  gfx::Size GetMinimumSize() const override;
  void PaintChildren(const views::PaintInfo& info) override;

  // web_app::AppRegistrarObserver
  void OnAlwaysShowToolbarInFullscreenChanged(const web_app::AppId& app_id,
                                              bool show) override;
  void OnAppRegistrarDestroyed() override;

  gfx::Insets GetCaptionButtonInsets() const;

  // Used by TabContainerOverlayView to paint the tab strip background.
  void PaintThemedFrame(gfx::Canvas* canvas) override;

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewMacTest,
                           GetCenteredTitleBounds);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewMacTest,
                           GetCaptionButtonPlaceholderBounds);

  static gfx::Rect GetCenteredTitleBounds(gfx::Rect frame,
                                          gfx::Rect available_space,
                                          int preferred_title_width);
  static gfx::Rect GetCaptionButtonPlaceholderBounds(
      const gfx::Rect& frame,
      const gfx::Insets& caption_button_insets);

  CGFloat FullscreenBackingBarHeight() const;

  // Calculate the y offset the top UI needs to shift down due to showing the
  // slide down menu bar at the very top in full screen.
  int TopUIFullscreenYOffset() const;
  void LayoutWindowControlsOverlay();

  void UpdateCaptionButtonPlaceholderContainerBackground();

  // Toggle the visibility of the web_app_frame_toolbar_view() for PWAs with
  // window controls overlay display override when entering full screen or when
  // toolbar style is changed.
  void ToggleWebAppFrameToolbarViewVisibility();

  // Returns the current value of the "always show toolbar in fullscreen"
  // preference, either reading the value from the kShowFullscreenToolbar
  // preference or if this is a window for an app, from the settings for that
  // app.
  bool AlwaysShowToolbarInFullscreen() const;

  // Used to keep track of the update of kShowFullscreenToolbar preference.
  BooleanPrefMember show_fullscreen_toolbar_;
  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::AppRegistrarObserver>
      always_show_toolbar_in_fullscreen_observation_{this};

  // A placeholder container that lies on top of the traffic lights to indicate
  // NonClientArea. Only for PWAs with window controls overlay display override.
  raw_ptr<CaptionButtonPlaceholderContainer>
      caption_button_placeholder_container_ = nullptr;

  base::scoped_nsobject<FullscreenToolbarController>
      fullscreen_toolbar_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
