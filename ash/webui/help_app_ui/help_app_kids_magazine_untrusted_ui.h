// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_KIDS_MAGAZINE_UNTRUSTED_UI_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_KIDS_MAGAZINE_UNTRUSTED_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

class HelpAppKidsMagazineUntrustedUIConfig : public content::WebUIConfig {
 public:
  HelpAppKidsMagazineUntrustedUIConfig();
  ~HelpAppKidsMagazineUntrustedUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

// The Web UI for chrome-untrusted://help-app-kids-magazine.
class HelpAppKidsMagazineUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit HelpAppKidsMagazineUntrustedUI(content::WebUI* web_ui);
  HelpAppKidsMagazineUntrustedUI(const HelpAppKidsMagazineUntrustedUI&) =
      delete;
  HelpAppKidsMagazineUntrustedUI& operator=(
      const HelpAppKidsMagazineUntrustedUI&) = delete;
  ~HelpAppKidsMagazineUntrustedUI() override;
};

}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_KIDS_MAGAZINE_UNTRUSTED_UI_H_
