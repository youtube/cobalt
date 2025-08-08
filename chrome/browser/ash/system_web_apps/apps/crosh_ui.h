// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CROSH_UI_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CROSH_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

// Loads DataSource at startup for Crosh (the Chromium OS shell).
class CroshUIConfig : public content::WebUIConfig {
 public:
  CroshUIConfig();
  ~CroshUIConfig() override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

class CroshUI : public ui::UntrustedWebUIController {
 public:
  explicit CroshUI(content::WebUI* web_ui);
  CroshUI(const CroshUI&) = delete;
  CroshUI& operator=(const CroshUI&) = delete;
  ~CroshUI() override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CROSH_UI_H_
