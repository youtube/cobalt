// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PWA_CONFIRMATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PWA_CONFIRMATION_BUBBLE_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/prefs/pref_service.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget.h"

class PageActionIconView;

namespace content {
class WebContents;
}  // namespace content

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace views {
class Checkbox;
}  // namespace views

namespace webapps {
class MlInstallOperationTracker;
}  // namespace webapps

// PWAConfirmationBubbleView provides a bubble dialog for accepting or rejecting
// the installation of a PWA (Progressive Web App) anchored off the PWA install
// icon in the omnibox.
class PWAConfirmationBubbleView : public LocationBarBubbleDelegateView {
 public:
  static bool IsShowing();
  static PWAConfirmationBubbleView* GetBubble();

  PWAConfirmationBubbleView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      PageActionIconView* highlight_icon_button,
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
      std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
      web_app::AppInstallationAcceptanceCallback callback,
      web_app::PwaInProductHelpState iph_state,
      PrefService* prefs,
      feature_engagement::Tracker* tracker);

  PWAConfirmationBubbleView(const PWAConfirmationBubbleView&) = delete;
  PWAConfirmationBubbleView& operator=(const PWAConfirmationBubbleView&) =
      delete;

  ~PWAConfirmationBubbleView() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInstallButton);
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kInstalledPWAEventId);

  // WidgetDelegate
  void OnWidgetInitialized() override;

  // LocationBarBubbleDelegateView:
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override;
  views::View* GetInitiallyFocusedView() override;
  void WindowClosing() override;
  bool Accept() override;

  static base::AutoReset<bool> SetDontCloseOnDeactivateForTesting();

 protected:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

 private:
  raw_ptr<PageActionIconView> highlight_icon_button_ = nullptr;
  std::unique_ptr<web_app::WebAppInstallInfo> web_app_info_;
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker_;
  web_app::AppInstallationAcceptanceCallback callback_;

  // Checkbox to launch window with tab strip.
  raw_ptr<views::Checkbox> tabbed_window_checkbox_ = nullptr;

  web_app::PwaInProductHelpState iph_state_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<feature_engagement::Tracker> tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PWA_CONFIRMATION_BUBBLE_VIEW_H_
