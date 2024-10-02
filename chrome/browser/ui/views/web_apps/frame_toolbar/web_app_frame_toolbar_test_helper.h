// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "content/public/browser/web_contents.h"

struct WebAppInstallInfo;
class Browser;
class BrowserNonClientFrameView;
class BrowserView;
class GURL;
class WebAppFrameToolbarView;

namespace base {
class ScopedTempDir;
}  // namespace base

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace views {
class View;
}  // namespace views

// Mixin for setting up and launching a web app in a browser test.
class WebAppFrameToolbarTestHelper {
 public:
  WebAppFrameToolbarTestHelper();
  WebAppFrameToolbarTestHelper(const WebAppFrameToolbarTestHelper&) = delete;
  WebAppFrameToolbarTestHelper& operator=(const WebAppFrameToolbarTestHelper&) =
      delete;
  ~WebAppFrameToolbarTestHelper();

  web_app::AppId InstallAndLaunchWebApp(Browser* browser,
                                        const GURL& start_url);
  web_app::AppId InstallAndLaunchCustomWebApp(
      Browser* browser,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      const GURL& start_url);

  GURL LoadWindowControlsOverlayTestPageWithDataAndGetURL(
      net::test_server::EmbeddedTestServer* embedded_test_server,
      base::ScopedTempDir* temp_dir);

  // WebContents is used to run JS to parse rectangle values into a list value.
  static base::Value::List GetXYWidthHeightListValue(
      content::WebContents* web_contents,
      const std::string& rect_value_list,
      const std::string& rect_var_name);

  // WebContents is used to run JS to parse rectangle values into a rectangle
  // object.
  static gfx::Rect GetXYWidthHeightRect(content::WebContents* web_contents,
                                        const std::string& rect_value_list,
                                        const std::string& rect_var_name);

  // Add window-controls-overlay's ongeometrychange callback into the document.
  void SetupGeometryChangeCallback(content::WebContents* web_contents);

  void TestDraggableRegions();

  // Opens a new popup window from |app_browser_| by running
  // |window_open_script| and returns the |BrowserView| it opened in.
  BrowserView* OpenPopup(const std::string& window_open_script);

  Browser* app_browser() { return app_browser_; }
  BrowserView* browser_view() { return browser_view_; }
  BrowserNonClientFrameView* frame_view() { return frame_view_; }
  views::View* root_view() { return root_view_; }
  WebAppFrameToolbarView* web_app_frame_toolbar() {
    return web_app_frame_toolbar_;
  }

 private:
  raw_ptr<Browser, DanglingUntriaged> app_browser_ = nullptr;
  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<BrowserNonClientFrameView, DanglingUntriaged> frame_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> root_view_ = nullptr;
  raw_ptr<WebAppFrameToolbarView, DanglingUntriaged> web_app_frame_toolbar_ =
      nullptr;

  GURL LoadTestPageWithDataAndGetURL(
      net::test_server::EmbeddedTestServer* embedded_test_server,
      base::ScopedTempDir* temp_dir,
      const char kTestHTML[]);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_TEST_HELPER_H_
