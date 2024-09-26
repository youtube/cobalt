// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_page.h"

#include <utility>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace security_interstitials {

SecurityInterstitialPage::SecurityInterstitialPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    std::unique_ptr<SecurityInterstitialControllerClient> controller)
    : web_contents_(web_contents),
      request_url_(request_url),
      create_view_(true),
      on_show_extended_reporting_pref_exists_(false),
      on_show_extended_reporting_pref_value_(false),
      controller_(std::move(controller)) {
  // Determine if any prefs need to be updated prior to showing the security
  // interstitial. Note that some content embedders (such as Android WebView)
  // uses security interstitials without a prefservice.
  if (controller_->GetPrefService()) {
    safe_browsing::UpdatePrefsBeforeSecurityInterstitial(
        controller_->GetPrefService());
  }
  SetUpMetrics();
}

SecurityInterstitialPage::~SecurityInterstitialPage() {
}

content::WebContents* SecurityInterstitialPage::web_contents() const {
  return web_contents_;
}

GURL SecurityInterstitialPage::request_url() const {
  return request_url_;
}

void SecurityInterstitialPage::DontCreateViewForTesting() {
  create_view_ = false;
}

bool SecurityInterstitialPage::ShouldDisplayURL() const {
  return true;
}

SecurityInterstitialPage::TypeID SecurityInterstitialPage::GetTypeForTesting() {
  // TODO(crbug.com/1077074): Once all subclasses define a TypeID this method
  // can become pure virtual.
  return nullptr;
}

std::string SecurityInterstitialPage::GetHTMLContents() {
  base::Value::Dict load_time_data;
  PopulateInterstitialStrings(load_time_data);
  webui::SetLoadTimeDataDefaults(controller()->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          GetHTMLTemplateId());

  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, load_time_data);
}

SecurityInterstitialControllerClient* SecurityInterstitialPage::controller()
    const {
  return controller_.get();
}

void SecurityInterstitialPage::SetUpMetrics() {
  // Remember the initial state of the extended reporting pref, to be compared
  // to the same data when the interstitial is closed.
  PrefService* prefs = controller_->GetPrefService();
  if (prefs) {
    on_show_extended_reporting_pref_exists_ =
        safe_browsing::ExtendedReportingPrefExists(*prefs);
    on_show_extended_reporting_pref_value_ =
        safe_browsing::IsExtendedReportingEnabled(*prefs);
  }
}

std::u16string SecurityInterstitialPage::GetFormattedHostName() const {
  return security_interstitials::common_string_util::GetFormattedHostName(
      request_url_);
}

int SecurityInterstitialPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

}  // security_interstitials
