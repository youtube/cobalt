// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"

class BrowserView;

// The 'app menu' button for a web app window.
class WebAppMenuButton : public AppMenuButton {
 public:
  METADATA_HEADER(WebAppMenuButton);
  static int GetMenuButtonSizeForBrowser(Browser* browser);
  explicit WebAppMenuButton(BrowserView* browser_view,
                            std::u16string accessible_name = std::u16string());
  WebAppMenuButton(const WebAppMenuButton&) = delete;
  WebAppMenuButton& operator=(const WebAppMenuButton&) = delete;
  ~WebAppMenuButton() override;

  // Sets the color of the menu button icon and highlight.
  void SetColor(SkColor color);
  SkColor GetColor() const;

  // Sets the icon.
  void set_icon(const gfx::VectorIcon& icon) { icon_ = &icon; }

  // Fades the menu button highlight on and off.
  void StartHighlightAnimation();

  virtual void ButtonPressed(const ui::Event& event);

 protected:
  BrowserView* browser_view() { return browser_view_; }

 private:
  void FadeHighlightOff();

  // The containing browser view.
  raw_ptr<BrowserView> browser_view_;

  SkColor color_ = gfx::kPlaceholderColor;
  raw_ptr<const gfx::VectorIcon> icon_ = &kBrowserToolsIcon;

  base::OneShotTimer highlight_off_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_
