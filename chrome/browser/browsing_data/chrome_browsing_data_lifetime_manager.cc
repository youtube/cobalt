// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/browsing_data/core/browsing_data_policies_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#else
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

namespace {

constexpr int kInitialCleanupDelayInSeconds = 15;
constexpr int kDefaultCleanupPeriodInHours = 1;

using ScheduledRemovalSettings =
    ChromeBrowsingDataLifetimeManager::ScheduledRemovalSettings;

// An observer of all the browsing data removal tasks that are started by the
// ChromeBrowsingDataLifetimeManager that records the the tasks starts and
// completed states as well as their durations.
class BrowsingDataRemoverObserver
    : public content::BrowsingDataRemover::Observer {
 public:
  ~BrowsingDataRemoverObserver() override = default;

  // Creates an instance of BrowsingDataRemoverObserver that
  // manages its own lifetime. The instance will be deleted after
  // |OnBrowsingDataRemoverDone| is called. |keep_alive| is an optional
  // parameter to pass to ensure that the browser does not initiates a shutdown
  // before the browsing data clearing is complete.
  static content::BrowsingDataRemover::Observer* Create(
      content::BrowsingDataRemover* remover,
      bool filterable_deletion,
      Profile* profile,
      bool keep_browser_alive = false) {
    return new BrowsingDataRemoverObserver(remover, filterable_deletion,
                                           profile, keep_browser_alive);
  }

  // content::BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    base::UmaHistogramMediumTimes(duration_histogram(),
                                  base::TimeTicks::Now() - start_time_);
    // Having |keep_browser_alive_| being true means that the deletion that just
    // finished was happening at the browser exit, therefore
    // |kClearBrowsingDataOnExitDeletionPending| is no more necessary;
    if (keep_browser_alive_) {
      profile_->GetPrefs()->ClearPref(
          browsing_data::prefs::kClearBrowsingDataOnExitDeletionPending);
    }
    base::UmaHistogramBoolean(state_histogram(),
                              /*BooleanStartedCompleted.Completed*/ true);
    // The profile and browser should not be shutting down yet.
    DCHECK(!keep_browser_alive_ || !profile_->ShutdownStarted());
    delete this;
  }

 private:
  BrowsingDataRemoverObserver(content::BrowsingDataRemover* remover,
                              bool filterable_deletion,
                              Profile* profile,
                              bool keep_browser_alive)
      : start_time_(base::TimeTicks::Now()),
        filterable_deletion_(filterable_deletion),
        profile_(profile),
        keep_browser_alive_(keep_browser_alive) {
#if !BUILDFLAG(IS_ANDROID)
    if (keep_browser_alive) {
      keep_alive_ = std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::BROWSING_DATA_LIFETIME_MANAGER,
          KeepAliveRestartOption::DISABLED);
    }
#endif
    browsing_data_remover_observer_.Observe(remover);
    base::UmaHistogramBoolean(state_histogram(),
                              /*BooleanStartedCompleted.Started*/ false);
  }

  const char* duration_histogram() const {
    static constexpr char kDurationScheduledFilterableDeletion[] =
        "History.BrowsingDataLifetime.Duration.ScheduledFilterableDeletion";
    static constexpr char kDurationScheduledUnfilterableDeletion[] =
        "History.BrowsingDataLifetime.Duration.ScheduledUnfilterableDeletion";
    static constexpr char kDurationBrowserShutdownDeletion[] =
        "History.BrowsingDataLifetime.Duration.BrowserShutdownDeletion";
    return keep_browser_alive_
               ? kDurationBrowserShutdownDeletion
               : filterable_deletion_ ? kDurationScheduledFilterableDeletion
                                      : kDurationScheduledUnfilterableDeletion;
  }

  const char* state_histogram() const {
    static constexpr char kStateScheduledFilterableDeletion[] =
        "History.BrowsingDataLifetime.State.ScheduledFilterableDeletion";
    static constexpr char kStateScheduledUnfilterableDeletion[] =
        "History.BrowsingDataLifetime.State.ScheduledUnfilterableDeletion";
    static constexpr char kStateBrowserShutdownDeletion[] =
        "History.BrowsingDataLifetime.State.BrowserShutdownDeletion";
    return keep_browser_alive_
               ? kStateBrowserShutdownDeletion
               : filterable_deletion_ ? kStateScheduledFilterableDeletion
                                      : kStateScheduledUnfilterableDeletion;
  }

  base::ScopedObservation<content::BrowsingDataRemover,
                          content::BrowsingDataRemover::Observer>
      browsing_data_remover_observer_{this};
  const base::TimeTicks start_time_;
  const bool filterable_deletion_;

  const raw_ptr<Profile> profile_;
  bool keep_browser_alive_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
#endif
};

uint64_t GetOriginTypeMask(const base::Value::List& data_types) {
  uint64_t result = 0;
  for (const auto& data_type : data_types) {
    std::string data_type_str = data_type.GetString();
    if (data_type_str ==
        browsing_data::policy_data_types::kCookiesAndOtherSiteData) {
      result |= content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kHostedAppData) {
      result |= content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    }
  }
  return result;
}

uint64_t GetRemoveMask(const base::Value::List& data_types) {
  uint64_t result = 0;
  for (const auto& data_type : data_types) {
    std::string data_type_str = data_type.GetString();
    if (data_type_str == browsing_data::policy_data_types::kBrowsingHistory) {
      result |= chrome_browsing_data_remover::DATA_TYPE_HISTORY;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kDownloadHistory) {
      result |= content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kCookiesAndOtherSiteData) {
      result |= chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kCachedImagesAndFiles) {
      result |= content::BrowsingDataRemover::DATA_TYPE_CACHE;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kPasswordSignin) {
      result |= chrome_browsing_data_remover::DATA_TYPE_PASSWORDS;
    } else if (data_type_str == browsing_data::policy_data_types::kAutofill) {
      result |= chrome_browsing_data_remover::DATA_TYPE_FORM_DATA;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kSiteSettings) {
      result |= chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS;
    } else if (data_type_str ==
               browsing_data::policy_data_types::kHostedAppData) {
      result |= chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
    }
  }
  return result;
}

std::vector<ScheduledRemovalSettings> ConvertToScheduledRemovalSettings(
    const base::Value::List& browsing_data_settings) {
  std::vector<ScheduledRemovalSettings> scheduled_removals_settings;
  for (const auto& setting : browsing_data_settings) {
    const auto* data_types =
        setting.GetDict().FindList(browsing_data::policy_fields::kDataTypes);
    const auto time_to_live_in_hours = setting.GetDict().FindInt(
        browsing_data::policy_fields::kTimeToLiveInHours);
    DCHECK(data_types);
    scheduled_removals_settings.push_back({GetRemoveMask(*data_types),
                                           GetOriginTypeMask(*data_types),
                                           *time_to_live_in_hours});
  }
  return scheduled_removals_settings;
}

base::flat_set<GURL> GetOpenedUrls(Profile* profile) {
  base::flat_set<GURL> result;
  // TODO (crbug/1288416): Enable this for android.
#if !BUILDFLAG(IS_ANDROID)
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile) {
      continue;
    }
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      result.insert(browser->tab_strip_model()->GetWebContentsAt(i)->GetURL());
    }
  }
#else
  for (const TabModel* model : TabModelList::models()) {
    for (int index = 0; index < model->GetTabCount(); ++index) {
      TabAndroid* tab = model->GetTabAt(index);
      if (tab)
        result.insert(tab->GetURL());
    }
  }
#endif
  return result;
}

}  // namespace

namespace browsing_data {

namespace policy_fields {

const char kTimeToLiveInHours[] = "time_to_live_in_hours";
const char kDataTypes[] = "data_types";

}  // namespace policy_fields
}  // namespace browsing_data

ChromeBrowsingDataLifetimeManager::ChromeBrowsingDataLifetimeManager(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)) {
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord());
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      browsing_data::prefs::kBrowsingDataLifetime,
      base::BindRepeating(
          &ChromeBrowsingDataLifetimeManager::UpdateScheduledRemovalSettings,
          base::Unretained(this)));

  // When the service is instantiated, wait a few minutes after Chrome startup
  // to start deleting data.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(&ChromeBrowsingDataLifetimeManager::
                                           UpdateScheduledRemovalSettings,
                                       weak_ptr_factory_.GetWeakPtr()),
                        base::Seconds(kInitialCleanupDelayInSeconds));
}

ChromeBrowsingDataLifetimeManager::~ChromeBrowsingDataLifetimeManager() =
    default;

void ChromeBrowsingDataLifetimeManager::Shutdown() {
  pref_change_registrar_.RemoveAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ChromeBrowsingDataLifetimeManager::ClearBrowsingDataForOnExitPolicy(
    bool keep_browser_alive) {
  const base::Value::List& data_types = profile_->GetPrefs()->GetList(
      browsing_data::prefs::kClearBrowsingDataOnExitList);
  if (!data_types.empty() && !SyncServiceFactory::IsSyncAllowed(profile_)) {
    profile_->GetPrefs()->SetBoolean(
        browsing_data::prefs::kClearBrowsingDataOnExitDeletionPending, true);
    auto* remover = profile_->GetBrowsingDataRemover();
    // Add a ScopedKeepAlive to hold the browser shutdown until the browsing
    // data is deleted and the profile is destroyed.
#if DCHECK_IS_ON()
    if (browser_shutdown::HasShutdownStarted())
      DCHECK(keep_browser_alive);
#endif
    remover->RemoveAndReply(base::Time(), base::Time::Max(),
                            GetRemoveMask(data_types),
                            GetOriginTypeMask(data_types),
                            BrowsingDataRemoverObserver::Create(
                                remover, /*filterable_deletion=*/true, profile_,
                                keep_browser_alive));
  } else {
    profile_->GetPrefs()->ClearPref(
        browsing_data::prefs::kClearBrowsingDataOnExitDeletionPending);
  }
}

void ChromeBrowsingDataLifetimeManager::UpdateScheduledRemovalSettings() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  scheduled_removals_settings_ =
      ConvertToScheduledRemovalSettings(profile_->GetPrefs()->GetList(
          browsing_data::prefs::kBrowsingDataLifetime));

  if (!scheduled_removals_settings_.empty())
    StartScheduledBrowsingDataRemoval();
}

void ChromeBrowsingDataLifetimeManager::StartScheduledBrowsingDataRemoval() {
  content::BrowsingDataRemover* remover = profile_->GetBrowsingDataRemover();

  for (auto& removal_settings : scheduled_removals_settings_) {
    if (removal_settings.time_to_live_in_hours <= 0)
      continue;

    if (SyncServiceFactory::IsSyncAllowed(profile_))
      continue;

    auto deletion_end_time = end_time_for_testing_.value_or(
        base::Time::Now() -
        base::Hours(removal_settings.time_to_live_in_hours));
    auto filterable_remove_mask =
        removal_settings.remove_mask &
        chrome_browsing_data_remover::FILTERABLE_DATA_TYPES;
    if (filterable_remove_mask) {
      auto filter_builder = content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kPreserve);
      for (const auto& url : GetOpenedUrls(profile_)) {
        std::string domain = GetDomainAndRegistry(
            url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
        if (domain.empty())
          domain = url.host();  // IP address or internal hostname.
        filter_builder->AddRegisterableDomain(domain);
      }
      remover->RemoveWithFilterAndReply(
          base::Time::Min(), deletion_end_time, filterable_remove_mask,
          removal_settings.origin_type_mask, std::move(filter_builder),
          testing_data_remover_observer_
              ? testing_data_remover_observer_.get()
              : BrowsingDataRemoverObserver::Create(
                    remover, /*filterable_deletion=*/true, profile_));
    }

    auto unfilterable_remove_mask =
        removal_settings.remove_mask &
        ~chrome_browsing_data_remover::FILTERABLE_DATA_TYPES;
    if (unfilterable_remove_mask) {
      remover->RemoveAndReply(
          base::Time::Min(), deletion_end_time, unfilterable_remove_mask,
          removal_settings.origin_type_mask,
          testing_data_remover_observer_
              ? testing_data_remover_observer_.get()
              : BrowsingDataRemoverObserver::Create(
                    remover, /*filterable_deletion=*/false, profile_));
    }
  }
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(&ChromeBrowsingDataLifetimeManager::
                                           StartScheduledBrowsingDataRemoval,
                                       weak_ptr_factory_.GetWeakPtr()),
                        base::Hours(kDefaultCleanupPeriodInHours));
}
