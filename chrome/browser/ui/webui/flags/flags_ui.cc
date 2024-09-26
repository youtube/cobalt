// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags/flags_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/flags/flags_ui_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/infobars/simple_alert_infobar_creator.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateAndAddFlagsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIFlagsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");
  source->AddString(flags_ui::kVersion, version_info::GetVersionNumber());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!user_manager::UserManager::Get()->IsCurrentUserOwner() &&
      base::SysInfo::IsRunningOnChromeOS()) {
    // Set the string to show which user can actually change the flags.
    std::string owner;
    ash::CrosSettings::Get()->GetString(ash::kDeviceOwner, &owner);
    source->AddString("owner-warning",
                      l10n_util::GetStringFUTF16(IDS_FLAGS_UI_OWNER_WARNING,
                                                 base::UTF8ToUTF16(owner)));
  } else {
    source->AddString("owner-warning", std::u16string());
  }
#endif

  source->AddResourcePath(flags_ui::kFlagsJS, IDR_FLAGS_UI_FLAGS_JS);
  source->AddResourcePath(flags_ui::kFlagsCSS, IDR_FLAGS_UI_FLAGS_CSS);
  source->SetDefaultResource(IDR_FLAGS_UI_FLAGS_HTML);
  source->UseStringsJs();
  return source;
}

// On ChromeOS verifying if the owner is signed in is async operation and only
// after finishing it the UI can be properly populated. This function is the
// callback for whether the owner is signed in. It will respectively pick the
// proper PrefService for the flags interface.
template <class T>
void FinishInitialization(base::WeakPtr<T> flags_ui,
                          Profile* profile,
                          FlagsUIHandler* dom_handler,
                          std::unique_ptr<flags_ui::FlagsStorage> storage,
                          flags_ui::FlagAccess access) {
  // If the flags_ui has gone away, there's nothing to do.
  if (!flags_ui)
    return;

  // Note that |dom_handler| is owned by the web ui that owns |flags_ui|, so
  // it is still alive if |flags_ui| is.
  dom_handler->Init(std::move(storage), access);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Show a warning info bar when kSafeMode switch is present.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode)) {
    CreateSimpleAlertInfoBar(
        infobars::ContentInfoBarManager::FromWebContents(
            flags_ui->web_ui()->GetWebContents()),
        infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE,
        &vector_icons::kWarningIcon,
        l10n_util::GetStringUTF16(IDS_FLAGS_IGNORED_DUE_TO_CRASHY_CHROME),
        /*auto_expire=*/false, /*should_animate=*/true);
  }

  // Show a warning info bar for secondary users.
  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    CreateSimpleAlertInfoBar(
        infobars::ContentInfoBarManager::FromWebContents(
            flags_ui->web_ui()->GetWebContents()),
        infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE,
        &vector_icons::kWarningIcon,
        l10n_util::GetStringUTF16(IDS_FLAGS_IGNORED_SECONDARY_USERS),
        /*auto_expire=*/false, /*should_animate=*/true);
  }
#endif
}

}  // namespace

// static
void FlagsUI::AddStrings(content::WebUIDataSource* source) {
  // Strings added here are all marked a non-translatable, so they are not
  // actually localized.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {flags_ui::kFlagsRestartNotice, IDS_FLAGS_UI_RELAUNCH_NOTICE},
    {"available", IDS_FLAGS_UI_AVAILABLE_FEATURE},
    {"clear-search", IDS_FLAGS_UI_CLEAR_SEARCH},
    {"disabled", IDS_FLAGS_UI_DISABLED_FEATURE},
    {"enabled", IDS_FLAGS_UI_ENABLED_FEATURE},
    {"experiment-enabled", IDS_FLAGS_UI_EXPERIMENT_ENABLED},
    {"heading", IDS_FLAGS_UI_TITLE},
    {"no-results", IDS_FLAGS_UI_NO_RESULTS},
    {"not-available-platform", IDS_FLAGS_UI_NOT_AVAILABLE_ON_PLATFORM},
    {"page-warning", IDS_FLAGS_UI_PAGE_WARNING},
    {"page-warning-explanation", IDS_FLAGS_UI_PAGE_WARNING_EXPLANATION},
    {"relaunch", IDS_FLAGS_UI_RELAUNCH},
    {"reset", IDS_FLAGS_UI_PAGE_RESET},
    {"reset-acknowledged", IDS_FLAGS_UI_RESET_ACKNOWLEDGED},
    {"search-label", IDS_FLAGS_UI_SEARCH_LABEL},
    {"search-placeholder", IDS_FLAGS_UI_SEARCH_PLACEHOLDER},
#if BUILDFLAG(IS_CHROMEOS)
    {"os-flags-link", IDS_FLAGS_UI_OS_FLAGS_LINK},
    {"os-flags-text1", IDS_FLAGS_UI_OS_FLAGS_TEXT1},
    {"os-flags-text2", IDS_FLAGS_UI_OS_FLAGS_TEXT2},
#endif
    {"title", IDS_FLAGS_UI_TITLE},
    {"unavailable", IDS_FLAGS_UI_UNAVAILABLE_FEATURE},
    {"searchResultsSingular", IDS_FLAGS_UI_SEARCH_RESULTS_SINGULAR},
    {"searchResultsPlural", IDS_FLAGS_UI_SEARCH_RESULTS_PLURAL}
  };
  source->AddLocalizedStrings(kLocalizedStrings);
}

// static
void FlagsDeprecatedUI::AddStrings(content::WebUIDataSource* source) {
  source->AddString("page-warning", std::string());

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {flags_ui::kFlagsRestartNotice, IDS_DEPRECATED_FEATURES_RELAUNCH_NOTICE},
    {"available", IDS_DEPRECATED_FEATURES_AVAILABLE_FEATURE},
    {"clear-search", IDS_DEPRECATED_UI_CLEAR_SEARCH},
    {"disabled", IDS_DEPRECATED_FEATURES_DISABLED_FEATURE},
    {"enabled", IDS_DEPRECATED_FEATURES_ENABLED_FEATURE},
    {"experiment-enabled", IDS_DEPRECATED_UI_EXPERIMENT_ENABLED},
    {"heading", IDS_DEPRECATED_FEATURES_HEADING},
    {"no-results", IDS_DEPRECATED_FEATURES_NO_RESULTS},
    {"not-available-platform",
     IDS_DEPRECATED_FEATURES_NOT_AVAILABLE_ON_PLATFORM},
    {"page-warning-explanation",
     IDS_DEPRECATED_FEATURES_PAGE_WARNING_EXPLANATION},
    {"relaunch", IDS_DEPRECATED_FEATURES_RELAUNCH},
    {"reset", IDS_DEPRECATED_FEATURES_PAGE_RESET},
    {"reset-acknowledged", IDS_DEPRECATED_UI_RESET_ACKNOWLEDGED},
    {"search-label", IDS_FLAGS_UI_SEARCH_LABEL},
    {"search-placeholder", IDS_DEPRECATED_FEATURES_SEARCH_PLACEHOLDER},
#if BUILDFLAG(IS_CHROMEOS)
    {"os-flags-link", IDS_DEPRECATED_FLAGS_UI_OS_FLAGS_LINK},
    {"os-flags-text1", IDS_DEPRECATED_FLAGS_UI_OS_FLAGS_TEXT1},
    {"os-flags-text2", IDS_DEPRECATED_FLAGS_UI_OS_FLAGS_TEXT2},
#endif
    {"title", IDS_DEPRECATED_FEATURES_TITLE},
    {"unavailable", IDS_DEPRECATED_FEATURES_UNAVAILABLE_FEATURE},
    {"searchResultsSingular", IDS_ENTERPRISE_UI_SEARCH_RESULTS_SINGULAR},
    {"searchResultsPlural", IDS_ENTERPRISE_UI_SEARCH_RESULTS_PLURAL}
  };
  source->AddLocalizedStrings(kLocalizedStrings);
}

template <class T>
FlagsUIHandler* InitializeHandler(content::WebUI* web_ui,
                                  Profile* profile,
                                  base::WeakPtrFactory<T>& weak_factory) {
  auto handler_owner = std::make_unique<FlagsUIHandler>();
  FlagsUIHandler* handler = handler_owner.get();
  web_ui->AddMessageHandler(std::move(handler_owner));

  about_flags::GetStorage(
      profile, base::BindOnce(&FinishInitialization<T>,
                              weak_factory.GetWeakPtr(), profile, handler));
  return handler;
}

FlagsUI::FlagsUI(content::WebUI* web_ui)
    : WebUIController(web_ui), weak_factory_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* handler = InitializeHandler(web_ui, profile, weak_factory_);
  DCHECK(handler);
  handler->set_deprecated_features_only(false);

  // Set up the about:flags source.
  content::WebUIDataSource* source = CreateAndAddFlagsUIHTMLSource(profile);
  AddStrings(source);
}

FlagsUI::~FlagsUI() {}

// static
base::RefCountedMemory* FlagsUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_FLAGS_FAVICON, scale_factor);
}

FlagsDeprecatedUI::FlagsDeprecatedUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* handler = InitializeHandler(web_ui, profile, weak_factory_);
  DCHECK(handler);
  handler->set_deprecated_features_only(true);

  // Set up the about:flags/deprecated source.
  content::WebUIDataSource* source = CreateAndAddFlagsUIHTMLSource(profile);
  AddStrings(source);
}

FlagsDeprecatedUI::~FlagsDeprecatedUI() {}

// static
bool FlagsDeprecatedUI::IsDeprecatedUrl(const GURL& url) {
  return url.path() == "/deprecated" || url.path() == "/deprecated/";
}
