// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt.h"

#include <limits>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace {

void ResetCheckDefaultBrowserPref(const base::FilePath& profile_path) {
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (profile)
    ResetDefaultBrowserPrompt(profile);
}

void ShowPrompt() {
  // Show the default browser request prompt in the most recently active,
  // visible, tabbed browser. Do not show the prompt if no such browser exists.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    // |browser| may be null in UI tests. Also, don't show the prompt in an app
    // window, which is not meant to be treated as a Chrome window. Only show in
    // a normal, tabbed browser.
    if (browser && !browser->is_type_normal())
      continue;

    // In ChromeBot tests, there might be a race. This line appears to get
    // called during shutdown and the active web contents can be nullptr.
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!web_contents ||
        web_contents->GetVisibility() != content::Visibility::VISIBLE) {
      continue;
    }

    // Never show the default browser prompt over the first run promos.
    // TODO(pmonette): The whole logic that determines when to show the default
    // browser prompt is due for a refactor. ShouldShowDefaultBrowserPrompt()
    // should be aware of the first run promos and return false instead of
    // counting on the early return here. See bug crbug.com/693292.
    if (first_run::IsOnWelcomePage(web_contents))
      continue;

    chrome::DefaultBrowserInfoBarDelegate::Create(
        infobars::ContentInfoBarManager::FromWebContents(web_contents),
        browser->profile());
    break;
  }
}

// Returns true if the default browser prompt should be shown if Chrome is not
// the user's default browser.
bool ShouldShowDefaultBrowserPrompt(Profile* profile) {
  // Do not show the prompt if "suppress_default_browser_prompt_for_version" in
  // the initial preferences is set to the current version.
  const std::string disable_version_string =
      g_browser_process->local_state()->GetString(
          prefs::kBrowserSuppressDefaultBrowserPrompt);
  const base::Version disable_version(disable_version_string);
  DCHECK(disable_version_string.empty() || disable_version.IsValid());
  if (disable_version.IsValid() &&
      disable_version == version_info::GetVersion()) {
    return false;
  }

  // Do not show if the prompt period has yet to pass since the user previously
  // dismissed the infobar.
  int64_t last_dismissed_value =
      profile->GetPrefs()->GetInt64(prefs::kDefaultBrowserLastDeclined);
  if (last_dismissed_value) {
    int period_days = 0;
    base::StringToInt(base::GetFieldTrialParamValue("DefaultBrowserInfobar",
                                                    "RefreshPeriodDays"),
                      &period_days);
    if (period_days <= 0 || period_days == std::numeric_limits<int>::max())
      return false;  // Failed to parse a reasonable period.
    base::Time show_on_or_after =
        base::Time::FromInternalValue(last_dismissed_value) +
        base::Days(period_days);
    if (base::Time::Now() < show_on_or_after)
      return false;
  }

  return true;
}

void OnCheckIsDefaultBrowserFinished(
    const base::FilePath& profile_path,
    bool show_prompt,
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    ResetCheckDefaultBrowserPref(profile_path);
  } else if (show_prompt && state == shell_integration::NOT_DEFAULT &&
             shell_integration::CanSetAsDefaultBrowser()) {
    // Only show the prompt if some other program is the user's default browser.
    // In particular, don't show it if another install mode is default (e.g.,
    // don't prompt for Chrome Beta if stable Chrome is the default).
    ShowPrompt();
  }
}

}  // namespace

void RegisterDefaultBrowserPromptPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kBrowserSuppressDefaultBrowserPrompt, std::string());
}

void ShowDefaultBrowserPrompt(Profile* profile) {
  // Do not check if Chrome is the default browser if there is a policy in
  // control of this setting.
  if (g_browser_process->local_state()->IsManagedPreference(
      prefs::kDefaultBrowserSettingEnabled)) {
    // Handling of the browser.default_browser_setting_enabled policy setting is
    // taken care of in BrowserProcessImpl.
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  // Reset preferences if kResetCheckDefaultBrowser is true.
  if (prefs->GetBoolean(prefs::kResetCheckDefaultBrowser)) {
    prefs->SetBoolean(prefs::kResetCheckDefaultBrowser, false);
    ResetDefaultBrowserPrompt(profile);
  }

  scoped_refptr<shell_integration::DefaultBrowserWorker>(
      new shell_integration::DefaultBrowserWorker())
      ->StartCheckIsDefault(
          base::BindOnce(&OnCheckIsDefaultBrowserFinished, profile->GetPath(),
                         ShouldShowDefaultBrowserPrompt(profile)));
}

void DefaultBrowserPromptDeclined(Profile* profile) {
  profile->GetPrefs()->SetInt64(prefs::kDefaultBrowserLastDeclined,
                                base::Time::Now().ToInternalValue());
}

void ResetDefaultBrowserPrompt(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kDefaultBrowserLastDeclined);
}
