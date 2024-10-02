// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace media_router {

// Implements the chrome://media-router-internals/ user interface.
class MediaRouterInternalsUI : public content::WebUIController {
 public:
  // |web_ui| owns this object and is used to initialize the base class.
  explicit MediaRouterInternalsUI(content::WebUI* web_ui);

  MediaRouterInternalsUI(const MediaRouterInternalsUI&) = delete;
  MediaRouterInternalsUI& operator=(const MediaRouterInternalsUI&) = delete;

  ~MediaRouterInternalsUI() override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_INTERNALS_UI_H_
