// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths.h"
#include "base/path_service.h"
#include "chrome/installer/util/shell_util.h"

namespace {

void ResetShortcutsOnBlockingThread() {
  // Get full path of chrome.
  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe))
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  for (int location = ShellUtil::SHORTCUT_LOCATION_FIRST;
       location <= ShellUtil::SHORTCUT_LOCATION_LAST; ++location) {
    ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
        static_cast<ShellUtil::ShortcutLocation>(location),
        ShellUtil::CURRENT_USER, chrome_exe, true, nullptr, nullptr);
    ShellUtil::ResetShortcutFileAttributes(
        static_cast<ShellUtil::ShortcutLocation>(location),
        ShellUtil::CURRENT_USER, chrome_exe);
  }
}

}  // namespace
#endif  // BUILDFLAG(IS_WIN)

ProfileResetter::ProfileResetter(Profile* profile)
    : profile_(profile),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      pending_reset_flags_(0),
      cookies_remover_(nullptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
}

ProfileResetter::~ProfileResetter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cookies_remover_)
    cookies_remover_->RemoveObserver(this);
}

void ProfileResetter::Reset(
    ProfileResetter::ResettableFlags resettable_flags,
    std::unique_ptr<BrandcodedDefaultSettings> master_settings,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(master_settings);

  // We should never be called with unknown flags.
  CHECK_EQ(static_cast<ResettableFlags>(0), resettable_flags & ~ALL);

  // We should never be called when a previous reset has not finished.
  CHECK_EQ(static_cast<ResettableFlags>(0), pending_reset_flags_);

  if (!resettable_flags) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(callback));
    return;
  }

  master_settings_.swap(master_settings);
  callback_ = std::move(callback);

  // These flags are set to false by the individual reset functions.
  pending_reset_flags_ = resettable_flags;

  struct {
    Resettable flag;
    void (ProfileResetter::*method)();
  } flagToMethod[] = {
      {DEFAULT_SEARCH_ENGINE, &ProfileResetter::ResetDefaultSearchEngine},
      {HOMEPAGE, &ProfileResetter::ResetHomepage},
      {CONTENT_SETTINGS, &ProfileResetter::ResetContentSettings},
      {COOKIES_AND_SITE_DATA, &ProfileResetter::ResetCookiesAndSiteData},
      {EXTENSIONS, &ProfileResetter::ResetExtensions},
      {STARTUP_PAGES, &ProfileResetter::ResetStartupPages},
      {PINNED_TABS, &ProfileResetter::ResetPinnedTabs},
      {SHORTCUTS, &ProfileResetter::ResetShortcuts},
      {NTP_CUSTOMIZATIONS, &ProfileResetter::ResetNtpCustomizations},
      {LANGUAGES, &ProfileResetter::ResetLanguages},
  };

  ResettableFlags reset_triggered_for_flags = 0;
  for (size_t i = 0; i < std::size(flagToMethod); ++i) {
    if (resettable_flags & flagToMethod[i].flag) {
      reset_triggered_for_flags |= flagToMethod[i].flag;
      (this->*flagToMethod[i].method)();
    }
  }

  DCHECK_EQ(resettable_flags, reset_triggered_for_flags);
}

bool ProfileResetter::IsActive() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_reset_flags_ != 0;
}

void ProfileResetter::MarkAsDone(Resettable resettable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check that we are never called twice or unexpectedly.
  CHECK(pending_reset_flags_ & resettable);

  pending_reset_flags_ &= ~resettable;

  if (!pending_reset_flags_) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(callback_));
    master_settings_.reset();
    template_url_service_subscription_ = {};
  }
}

void ProfileResetter::ResetDefaultSearchEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(template_url_service_);
  // If TemplateURLServiceFactory is ready we can clean it right now.
  // Otherwise, load it and continue from ProfileResetter::Observe.
  if (template_url_service_->loaded()) {
    PrefService* prefs = profile_->GetPrefs();
    DCHECK(prefs);
    TemplateURLPrepopulateData::ClearPrepopulatedEnginesInPrefs(
        profile_->GetPrefs());
    absl::optional<base::Value::List> search_engines(
        master_settings_->GetSearchProviderOverrides());
    if (search_engines.has_value()) {
      // This Chrome distribution channel provides a custom search engine. We
      // must reset to it.
      prefs->SetList(prefs::kSearchProviderOverrides,
                     std::move(search_engines).value());
    }

    template_url_service_->RepairPrepopulatedSearchEngines();
    template_url_service_->RepairStarterPackEngines();

    MarkAsDone(DEFAULT_SEARCH_ENGINE);
  } else {
    template_url_service_subscription_ =
        template_url_service_->RegisterOnLoadedCallback(
            base::BindOnce(&ProfileResetter::OnTemplateURLServiceLoaded,
                           weak_ptr_factory_.GetWeakPtr()));
    template_url_service_->Load();
  }
}

void ProfileResetter::ResetHomepage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  std::string homepage;

  if (master_settings_->GetHomepage(&homepage))
    prefs->SetString(prefs::kHomePage, homepage);

  absl::optional<bool> homepage_is_ntp =
      master_settings_->GetHomepageIsNewTab();
  if (homepage_is_ntp.has_value())
    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, *homepage_is_ntp);
  else
    prefs->ClearPref(prefs::kHomePageIsNewTabPage);

  absl::optional<bool> show_home_button = master_settings_->GetShowHomeButton();
  if (show_home_button.has_value())
    prefs->SetBoolean(prefs::kShowHomeButton, *show_home_button);
  else
    prefs->ClearPref(prefs::kShowHomeButton);
  MarkAsDone(HOMEPAGE);
}

void ProfileResetter::ResetContentSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const content_settings::WebsiteSettingsInfo* info :
       *content_settings::WebsiteSettingsRegistry::GetInstance()) {
    map->ClearSettingsForOneType(info->type());
  }

  // TODO(raymes): The default value isn't really used for website settings
  // right now, but if it were we should probably reset that here too.
  for (const content_settings::ContentSettingsInfo* info :
       *content_settings::ContentSettingsRegistry::GetInstance()) {
    map->SetDefaultContentSetting(info->website_settings_info()->type(),
                                  CONTENT_SETTING_DEFAULT);
  }
  MarkAsDone(CONTENT_SETTINGS);
}

void ProfileResetter::ResetCookiesAndSiteData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cookies_remover_);

  cookies_remover_ = profile_->GetBrowsingDataRemover();
  cookies_remover_->AddObserver(this);
  uint64_t remove_mask = chrome_browsing_data_remover::DATA_TYPE_SITE_DATA |
                         content::BrowsingDataRemover::DATA_TYPE_CACHE;
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  cookies_remover_->RemoveAndReply(
      base::Time(), base::Time::Max(), remove_mask,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, this);
}

void ProfileResetter::ResetExtensions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::string> brandcode_extensions;
  master_settings_->GetExtensions(&brandcode_extensions);

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(extension_service);
  extension_service->DisableUserExtensionsExcept(brandcode_extensions);

  // Reenable all disabled external component extensions.
  // BrandcodedDefaultSettings does not contain information about component
  // extensions, so fetch them from the existing registry. This may be not very
  // robust, as the profile resetter may be invoked when the registry is in some
  // iffy state. However, we can't enable an extension which is not in the
  // registry anyway.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);
  DCHECK(extension_registry);
  std::vector<extensions::ExtensionId> extension_ids_to_reenable;
  for (const auto& extension : extension_registry->disabled_extensions()) {
    if (extension->location() ==
        extensions::mojom::ManifestLocation::kExternalComponent)
      extension_ids_to_reenable.push_back(extension->id());
  }
  for (const auto& extension_id : extension_ids_to_reenable) {
    extension_service->EnableExtension(extension_id);
  }

  MarkAsDone(EXTENSIONS);
}

void ProfileResetter::ResetStartupPages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  absl::optional<base::Value::List> url_list(
      master_settings_->GetUrlsToRestoreOnStartup());
  if (url_list.has_value()) {
    prefs->SetList(prefs::kURLsToRestoreOnStartup, std::move(url_list).value());
  }

  int restore_on_startup;
  if (master_settings_->GetRestoreOnStartup(&restore_on_startup))
    prefs->SetInteger(prefs::kRestoreOnStartup, restore_on_startup);
  else
    prefs->ClearPref(prefs::kRestoreOnStartup);

  MarkAsDone(STARTUP_PAGES);
}

void ProfileResetter::ResetPinnedTabs() {
  // Unpin all the tabs.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->is_type_normal() && browser->profile() == profile_) {
      TabStripModel* tab_model = browser->tab_strip_model();
      // Here we assume that indexof(any mini tab) < indexof(any normal tab).
      // If we unpin the tab, it can be moved to the right. Thus traversing in
      // reverse direction is correct.
      for (int i = tab_model->count() - 1; i >= 0; --i) {
        if (tab_model->IsTabPinned(i))
          tab_model->SetTabPinned(i, false);
      }
    }
  }
  MarkAsDone(PINNED_TABS);
}

void ProfileResetter::ResetShortcuts() {
#if BUILDFLAG(IS_WIN)
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
      ->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&ResetShortcutsOnBlockingThread),
          base::BindOnce(&ProfileResetter::MarkAsDone,
                         weak_ptr_factory_.GetWeakPtr(), SHORTCUTS));
#else
  MarkAsDone(SHORTCUTS);
#endif
}

void ProfileResetter::ResetNtpCustomizations() {
  NtpCustomBackgroundService::ResetProfilePrefs(profile_);
  NewTabPageUI::ResetProfilePrefs(profile_->GetPrefs());
  MarkAsDone(NTP_CUSTOMIZATIONS);
}

void ProfileResetter::ResetLanguages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  auto translate_prefs = ChromeTranslateClient::CreateTranslatePrefs(prefs);
  translate_prefs->ResetToDefaults();

  language::ResetLanguagePrefs(prefs);

  MarkAsDone(LANGUAGES);
}

void ProfileResetter::OnTemplateURLServiceLoaded() {
  // TemplateURLService has loaded. If we need to clean search engines, it's
  // time to go on.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  template_url_service_subscription_ = {};
  if (pending_reset_flags_ & DEFAULT_SEARCH_ENGINE)
    ResetDefaultSearchEngine();
}

void ProfileResetter::OnBrowsingDataRemoverDone(uint64_t failed_data_types) {
  cookies_remover_->RemoveObserver(this);
  cookies_remover_ = nullptr;
  MarkAsDone(COOKIES_AND_SITE_DATA);
}

#if BUILDFLAG(IS_WIN)
std::vector<ShortcutCommand> GetChromeLaunchShortcuts(
    const scoped_refptr<SharedCancellationFlag>& cancel) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Get full path of chrome.
  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe))
    return std::vector<ShortcutCommand>();
  std::vector<ShortcutCommand> shortcuts;
  for (int location = ShellUtil::SHORTCUT_LOCATION_FIRST;
       location <= ShellUtil::SHORTCUT_LOCATION_LAST; ++location) {
    if (cancel.get() && cancel->data.IsSet())
      break;
    ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
        static_cast<ShellUtil::ShortcutLocation>(location),
        ShellUtil::CURRENT_USER,
        chrome_exe,
        false,
        cancel,
        &shortcuts);
  }
  return shortcuts;
}
#endif  // BUILDFLAG(IS_WIN)
