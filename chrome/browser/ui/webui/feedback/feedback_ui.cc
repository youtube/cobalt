// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_ui.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/feedback_resources.h"
#include "chrome/grit/feedback_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/arc/arc_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void AddStringResources(content::WebUIDataSource* source,
                        const Profile* profile) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"additionalInfo", IDS_FEEDBACK_ADDITIONAL_INFO_LABEL},
      {"anonymousUser", IDS_FEEDBACK_ANONYMOUS_EMAIL_OPTION},
      {"appTitle", IDS_FEEDBACK_REPORT_APP_TITLE},
      {"assistantInfo", IDS_FEEDBACK_INCLUDE_ASSISTANT_INFORMATION_CHKBOX},
      {"assistantLogsMessage", IDS_FEEDBACK_ASSISTANT_LOGS_MESSAGE},
      {"attachFileLabel", IDS_FEEDBACK_ATTACH_FILE_LABEL},
      {"attachFileNote", IDS_FEEDBACK_ATTACH_FILE_NOTE},
      {"attachFileToBig", IDS_FEEDBACK_ATTACH_FILE_TO_BIG},
      {"autofillMetadataPageTitle", IDS_FEEDBACK_AUTOFILL_METADATA_PAGE_TITLE},
      {"autofillMetadataInfo", IDS_FEEDBACK_INCLUDE_AUTOFILL_METADATA_CHECKBOX},
      {"bluetoothLogsInfo", IDS_FEEDBACK_BLUETOOTH_LOGS_CHECKBOX},
      {"bluetoothLogsMessage", IDS_FEEDBACK_BLUETOOTH_LOGS_MESSAGE},
      {"cancel", IDS_CANCEL},
      {"consentCheckboxLabel", IDS_FEEDBACK_CONSENT_CHECKBOX_LABEL},
      {"freeFormText", IDS_FEEDBACK_FREE_TEXT_LABEL},
      {"logsMapPageCollapseAllBtn", IDS_ABOUT_SYS_COLLAPSE_ALL},
      {"logsMapPageCollapseBtn", IDS_ABOUT_SYS_COLLAPSE},
      {"logsMapPageExpandAllBtn", IDS_ABOUT_SYS_EXPAND_ALL},
      {"logsMapPageExpandBtn", IDS_ABOUT_SYS_EXPAND},
      {"logsMapPageStatusLoading", IDS_FEEDBACK_SYSINFO_PAGE_LOADING},
      {"logsMapPageTableTitle", IDS_ABOUT_SYS_TABLE_TITLE},
      {"minimizeBtnLabel", IDS_FEEDBACK_MINIMIZE_BUTTON_LABEL},
      {"noDescription", IDS_FEEDBACK_NO_DESCRIPTION},
      {"pageTitle", IDS_FEEDBACK_REPORT_PAGE_TITLE},
      {"pageUrl", IDS_FEEDBACK_REPORT_URL_LABEL},
      {"performanceTrace", IDS_FEEDBACK_INCLUDE_PERFORMANCE_TRACE_CHECKBOX},
      {"privacyNote", IDS_FEEDBACK_PRIVACY_NOTE},
      {"screenshot", IDS_FEEDBACK_SCREENSHOT_LABEL},
      {"screenshotA11y", IDS_FEEDBACK_SCREENSHOT_A11Y_TEXT},
      {"sendReport", IDS_FEEDBACK_SEND_REPORT},
      {"sysinfoPageDescription", IDS_ABOUT_SYS_DESC},
      {"sysinfoPageTitle", IDS_FEEDBACK_SYSINFO_PAGE_TITLE},
      {"userEmail", IDS_FEEDBACK_USER_EMAIL_LABEL},
  };

  source->AddLocalizedStrings(kStrings);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddLocalizedString(
      "sysInfo",
      arc::IsArcPlayStoreEnabledForProfile(profile)
          ? IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX_ARC
          : IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX);
#else
  source->AddLocalizedString("sysInfo",
                             IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_CHKBOX);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void CreateAndAddFeedbackHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIFeedbackHost);
  source->AddResourcePaths(
      base::make_span(kFeedbackResources, kFeedbackResourcesSize));
  source->AddResourcePath("", IDR_FEEDBACK_HTML_DEFAULT_HTML);

  // Register the CSS file from chrome://system manually as that style is
  // re-used by chrome://feedback/html/sys_info.html.
  source->AddResourcePath("css/about_sys.css", IDR_ABOUT_SYS_CSS);

  source->UseStringsJs();

  AddStringResources(source, profile);
}

FeedbackUI::FeedbackUI(content::WebUI* web_ui) : WebDialogUI(web_ui) {
  CreateAndAddFeedbackHTMLSource(Profile::FromWebUI(web_ui));
}

FeedbackUI::~FeedbackUI() = default;

bool FeedbackUI::IsFeedbackEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kUserFeedbackAllowed);
}
