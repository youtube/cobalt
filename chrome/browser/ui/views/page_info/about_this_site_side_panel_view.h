// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/page_info/about_this_site_side_panel_throttle.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/layout/flex_layout_view.h"

class BrowserView;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

// Owns the webview and navigates to a google search URL when requested. Its
// owned by the side panel registry.
class AboutThisSiteSidePanelView
    : public views::FlexLayoutView,
      public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public AboutThisSiteWebContentsUserData::Delegate,
      public base::SupportsWeakPtr<AboutThisSiteSidePanelView> {
 public:
  explicit AboutThisSiteSidePanelView(BrowserView* browser_view);
  AboutThisSiteSidePanelView(const AboutThisSiteSidePanelView&) = delete;
  AboutThisSiteSidePanelView& operator=(const AboutThisSiteSidePanelView&) =
      delete;
  ~AboutThisSiteSidePanelView() override;

  void OpenUrl(const content::OpenURLParams& params);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // Remove parameters that shouldn't be passed to the main browser.
  GURL CleanUpQueryParams(const GURL& url);

  // Shows / hides the page and the loading view to avoid showing
  // loading artifacts.
  void SetContentVisible(bool visible);

  // AboutThisSiteWebContentsUserData::Delegate
  void OpenUrlInBrowser(const content::OpenURLParams& params) override;
  bool IsNavigationAllowed(const GURL& new_url, const GURL& old_url) override;

  // content::WebContentsObserver:
  void LoadProgressChanged(double progress) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  content::WebContentsDelegate* outer_delegate();

  GURL last_url_;
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<views::WebView> loading_indicator_web_view_;
  raw_ptr<views::WebView> web_view_;
};

#endif // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_
