// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/policy/policy_ui.h"

#import <memory>
#import <string>

#import "base/json/json_writer.h"
#import "components/grit/policy_resources.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/strings/grit/components_chromium_strings.h"
#import "components/strings/grit/components_google_chrome_strings.h"
#import "components/strings/grit/components_strings.h"
#import "components/version_info/version_info.h"
#import "components/version_ui/version_handler_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/webui/policy/policy_ui_handler.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"
#import "ui/base/webui/web_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the version information to be displayed on the chrome://policy/logs
// page.
base::Value::Dict GetVersionInfo() {
  base::Value::Dict version_info;

  version_info.Set("revision", version_info::GetLastChange());
  version_info.Set("version", version_info::GetVersionNumber());
  version_info.Set("deviceOs", "iOS");
  version_info.Set("variations", version_ui::GetVariationsList());

  return version_info;
}

web::WebUIIOSDataSource* CreatePolicyUIHtmlSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIPolicyHost);
  PolicyUIHandler::AddCommonLocalizedStringsToSource(source);

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"copyPoliciesJSON", IDS_COPY_POLICIES_JSON},
      {"exportPoliciesJSON", IDS_EXPORT_POLICIES_JSON},
      {"filterPlaceholder", IDS_POLICY_FILTER_PLACEHOLDER},
      {"hideExpandedStatus", IDS_POLICY_HIDE_EXPANDED_STATUS},
      {"isAffiliatedYes", IDS_POLICY_IS_AFFILIATED_YES},
      {"isAffiliatedNo", IDS_POLICY_IS_AFFILIATED_NO},
      {"labelAssetId", IDS_POLICY_LABEL_ASSET_ID},
      {"labelClientId", IDS_POLICY_LABEL_CLIENT_ID},
      {"labelDirectoryApiId", IDS_POLICY_LABEL_DIRECTORY_API_ID},
      {"labelError", IDS_POLICY_LABEL_ERROR},
      {"labelWarning", IDS_POLICY_HEADER_WARNING},
      {"labelGaiaId", IDS_POLICY_LABEL_GAIA_ID},
      {"labelIsAffiliated", IDS_POLICY_LABEL_IS_AFFILIATED},
      {"labelLastCloudReportSentTimestamp",
       IDS_POLICY_LABEL_LAST_CLOUD_REPORT_SENT_TIMESTAMP},
      {"labelLocation", IDS_POLICY_LABEL_LOCATION},
      {"labelMachineEnrollmentDomain",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DOMAIN},
      {"labelMachineEnrollmentMachineName",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_MACHINE_NAME},
      {"labelMachineEnrollmentToken",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_TOKEN},
      {"labelMachineEntrollmentDeviceId",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DEVICE_ID},
      {"labelIsOffHoursActive", IDS_POLICY_LABEL_IS_OFFHOURS_ACTIVE},
      {"labelPoliciesPush", IDS_POLICY_LABEL_PUSH_POLICIES},
      {"labelPrecedence", IDS_POLICY_LABEL_PRECEDENCE},
      {"labelProfileId", IDS_POLICY_LABEL_PROFILE_ID},
      {"labelRefreshInterval", IDS_POLICY_LABEL_REFRESH_INTERVAL},
      {"labelStatus", IDS_POLICY_LABEL_STATUS},
      {"labelTimeSinceLastFetchAttempt",
       IDS_POLICY_LABEL_TIME_SINCE_LAST_FETCH_ATTEMPT},
      {"labelTimeSinceLastRefresh", IDS_POLICY_LABEL_TIME_SINCE_LAST_REFRESH},
      {"labelUsername", IDS_POLICY_LABEL_USERNAME},
      {"labelManagedBy", IDS_POLICY_LABEL_MANAGED_BY},
      {"labelVersion", IDS_POLICY_LABEL_VERSION},
      {"noPoliciesSet", IDS_POLICY_NO_POLICIES_SET},
      {"offHoursActive", IDS_POLICY_OFFHOURS_ACTIVE},
      {"offHoursNotActive", IDS_POLICY_OFFHOURS_NOT_ACTIVE},
      {"policiesPushOff", IDS_POLICY_PUSH_POLICIES_OFF},
      {"policiesPushOn", IDS_POLICY_PUSH_POLICIES_ON},
      {"policyCopyValue", IDS_POLICY_COPY_VALUE},
      {"policyLearnMore", IDS_POLICY_LEARN_MORE},
      {"reloadPolicies", IDS_POLICY_RELOAD_POLICIES},
      {"showExpandedStatus", IDS_POLICY_SHOW_EXPANDED_STATUS},
      {"showLess", IDS_POLICY_SHOW_LESS},
      {"showMore", IDS_POLICY_SHOW_MORE},
      {"showUnset", IDS_POLICY_SHOW_UNSET},
      {"signinProfile", IDS_POLICY_SIGNIN_PROFILE},
      {"status", IDS_POLICY_STATUS},
      {"statusErrorManagedNoPolicy", IDS_POLICY_STATUS_ERROR_MANAGED_NO_POLICY},
      {"statusFlexOrgNoPolicy", IDS_POLICY_STATUS_FLEX_ORG_NO_POLICY},
      {"statusDevice", IDS_POLICY_STATUS_DEVICE},
      {"statusMachine", IDS_POLICY_STATUS_MACHINE},
      {"statusUser", IDS_POLICY_STATUS_USER},
      {"uploadReport", IDS_UPLOAD_REPORT},

  };
  source->AddLocalizedStrings(kStrings);

  // Localized strings for chrome://policy/logs.
  static constexpr webui::LocalizedString kPolicyLogsStrings[] = {
      {"browserName", IDS_IOS_PRODUCT_NAME},
      {"exportLogsJSON", IDS_EXPORT_POLICY_LOGS_JSON},
      {"logsTitle", IDS_POLICY_LOGS_TITLE},
      {"os", IDS_VERSION_UI_OS},
      {"refreshLogs", IDS_REFRESH_POLICY_LOGS},
      {"revision", IDS_VERSION_UI_REVISION},
      {"versionInfoLabel", IDS_VERSION_INFO},
      {"variations", IDS_VERSION_UI_VARIATIONS},
  };
  source->AddLocalizedStrings(kPolicyLogsStrings);

  source->UseStringsJs();

  source->AddBoolean("hideExportButton", true);

  source->AddResourcePath("policy.css", IDR_POLICY_POLICY_CSS);
  source->AddResourcePath("policy_base.js", IDR_POLICY_POLICY_BASE_JS);
  source->AddResourcePath("policy.js", IDR_POLICY_POLICY_JS);
  source->AddResourcePath("policy_conflict.html.js",
                          IDR_POLICY_POLICY_CONFLICT_HTML_JS);
  source->AddResourcePath("policy_conflict.js", IDR_POLICY_POLICY_CONFLICT_JS);
  source->AddResourcePath("policy_row.html.js", IDR_POLICY_POLICY_ROW_HTML_JS);
  source->AddResourcePath("policy_row.js", IDR_POLICY_POLICY_ROW_JS);
  source->AddResourcePath("policy_precedence_row.html.js",
                          IDR_POLICY_POLICY_PRECEDENCE_ROW_HTML_JS);
  source->AddResourcePath("policy_precedence_row.js",
                          IDR_POLICY_POLICY_PRECEDENCE_ROW_JS);
  source->AddResourcePath("policy_table.html.js",
                          IDR_POLICY_POLICY_TABLE_HTML_JS);
  source->AddResourcePath("policy_table.js", IDR_POLICY_POLICY_TABLE_JS);
  source->AddResourcePath("status_box.html.js", IDR_POLICY_STATUS_BOX_HTML_JS);
  source->AddResourcePath("status_box.js", IDR_POLICY_STATUS_BOX_JS);

  source->AddBoolean(
      "loggingEnabled",
      policy::PolicyLogger::GetInstance()->IsPolicyLoggingEnabled());

  if (policy::PolicyLogger::GetInstance()->IsPolicyLoggingEnabled()) {
    std::string variations_json_value;
    base::JSONWriter::Write(GetVersionInfo(), &variations_json_value);
    source->AddString("versionInfo", variations_json_value);
  }
  source->AddResourcePath("logs/policy_logs.js",
                          IDR_POLICY_LOGS_POLICY_LOGS_JS);
  source->AddResourcePath("logs/", IDR_POLICY_LOGS_POLICY_LOGS_HTML);
  source->AddResourcePath("logs", IDR_POLICY_LOGS_POLICY_LOGS_HTML);

  source->SetDefaultResource(IDR_POLICY_POLICY_HTML);
  source->EnableReplaceI18nInJS();
  return source;
}

}  // namespace

PolicyUI::PolicyUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<PolicyUIHandler>());
  web::WebUIIOSDataSource::Add(ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreatePolicyUIHtmlSource());
}

PolicyUI::~PolicyUI() {}
