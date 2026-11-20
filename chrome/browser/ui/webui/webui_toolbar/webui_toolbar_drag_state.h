// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_DRAG_STATE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_DRAG_STATE_H_

#include "content/public/browser/web_contents_user_data.h"

namespace webui_toolbar {

class WebUIToolbarDragState
    : public content::WebContentsUserData<WebUIToolbarDragState> {
 public:
  static WebUIToolbarDragState* GetOrCreateForWebContents(
      content::WebContents* contents);

  ~WebUIToolbarDragState() override;

  bool drag_originated_from_renderer() const {
    return drag_originated_from_renderer_;
  }
  void set_drag_originated_from_renderer(bool val) {
    drag_originated_from_renderer_ = val;
  }

 private:
  explicit WebUIToolbarDragState(content::WebContents* contents);
  friend class content::WebContentsUserData<WebUIToolbarDragState>;

  bool drag_originated_from_renderer_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_DRAG_STATE_H_
