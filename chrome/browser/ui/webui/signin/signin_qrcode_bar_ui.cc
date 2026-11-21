// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_qrcode_bar_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/signin_resources.h"
#include "chrome/grit/signin_resources_map.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

bool SigninQRCodeBarUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return switches::IsMagiChromePasskeyBannerEnabled();
}

SigninQRCodeBarUI::SigninQRCodeBarUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISigninQRCodeBarHost);

  webui::SetupWebUIDataSource(
      source, kSigninResources,
      IDR_SIGNIN_SIGNIN_QRCODE_BAR_SIGNIN_QRCODE_BAR_HTML);
}

SigninQRCodeBarUI::~SigninQRCodeBarUI() = default;
