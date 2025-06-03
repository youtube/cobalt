// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_client_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/system/federated/federated_service_controller_impl.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_notifier_impl.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/app_sync_ui_state_watcher.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_controller_factory.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/url_handler_ash.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace {

AppListClientImpl* g_app_list_client_instance = nullptr;

// Parameters used by the time duration metrics.
constexpr base::TimeDelta kTimeMetricsMin = base::Seconds(1);
constexpr base::TimeDelta kTimeMetricsMax = base::Days(7);
constexpr int kTimeMetricsBucketCount = 100;

bool IsTabletMode() {
  return ash::TabletMode::IsInTabletMode();
}

// Returns whether the session is active.
bool IsSessionActive() {
  return session_manager::SessionManager::Get()->session_state() ==
         session_manager::SessionState::ACTIVE;
}

// IDs passed to ActivateItem are always of the form "<app id>". But app search
// results can have IDs either like "<app id>" or "chrome-extension://<app
// id>/". Since we cannot tell from the ID alone which is correct, try both and
// return a result if either succeeds.
ChromeSearchResult* FindAppResultByAppId(
    app_list::SearchController* search_controller,
    const std::string& app_id) {
  auto* result = search_controller->FindSearchResult(app_id);
  if (!result) {
    // Convert <app id> to chrome-extension://<app id>.
    result = search_controller->FindSearchResult(
        base::StrCat({extensions::kExtensionScheme, "://", app_id, "/"}));
  }
  return result;
}

ash::NewWindowDelegate::Disposition ConvertDisposition(
    WindowOpenDisposition disposition) {
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return ash::NewWindowDelegate::Disposition::kNewForegroundTab;
    case WindowOpenDisposition::UNKNOWN:
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD:
    case WindowOpenDisposition::IGNORE_ACTION:
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
      return ash::NewWindowDelegate::Disposition::kNewWindow;
    case WindowOpenDisposition::CURRENT_TAB:
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::SWITCH_TO_TAB:
      return ash::NewWindowDelegate::Disposition::kSwitchToTab;
  }
}

class ScopedIphSessionImpl : public ash::ScopedIphSession {
 public:
  explicit ScopedIphSessionImpl(feature_engagement::Tracker* tracker,
                                const base::Feature& iph_feature)
      : tracker_(tracker), iph_feature_(iph_feature) {
    CHECK(tracker_);
  }

  ~ScopedIphSessionImpl() override { tracker_->Dismissed(*iph_feature_); }

  void NotifyEvent(const std::string& event) override {
    tracker_->NotifyEvent(event);
  }

 private:
  raw_ptr<feature_engagement::Tracker> tracker_;
  const raw_ref<const base::Feature, ExperimentalAsh> iph_feature_;
};

}  // namespace

AppListClientImpl::AppListClientImpl()
    : app_list_controller_(ash::AppListController::Get()) {
  app_list_controller_->SetClient(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);

  DCHECK(!g_app_list_client_instance);
  g_app_list_client_instance = this;

  app_list_notifier_ =
      std::make_unique<AppListNotifierImpl>(app_list_controller_);
}

AppListClientImpl::~AppListClientImpl() {
  SetProfile(nullptr);

  auto* user_manager = user_manager::UserManager::Get();
  user_manager->RemoveSessionStateObserver(this);

  // We assume that the current user is new if `state_for_new_user_` has value.
  if (state_for_new_user_.has_value() &&
      !state_for_new_user_->showing_recorded) {
    DCHECK(user_manager->IsCurrentUserNew());

    // Prefer the function to the macro because the usage data is recorded no
    // more than once per second.
    if (IsTabletMode()) {
      base::UmaHistogramEnumeration(
          "Apps.AppListUsageByNewUsers.TabletMode",
          AppListUsageStateByNewUsers::kNotUsedBeforeDestruction);
    } else {
      base::UmaHistogramEnumeration(
          "Apps.AppListUsageByNewUsers.ClamshellMode",
          AppListUsageStateByNewUsers::kNotUsedBeforeDestruction);
    }
  }

  session_manager::SessionManager::Get()->RemoveObserver(this);

  DCHECK_EQ(this, g_app_list_client_instance);
  g_app_list_client_instance = nullptr;

  if (app_list_controller_) {
    app_list_controller_->SetClient(nullptr);
  }
}

// static
AppListClientImpl* AppListClientImpl::GetInstance() {
  return g_app_list_client_instance;
}

void AppListClientImpl::OnAppListControllerDestroyed() {
  // |app_list_controller_| could be released earlier, e.g. starting a kiosk
  // next session.
  app_list_controller_ = nullptr;
  if (current_model_updater_) {
    current_model_updater_->SetActive(false);
  }
}

std::vector<ash::AppListSearchControlCategory>
AppListClientImpl::GetToggleableCategories() const {
  return search_controller_->GetToggleableCategories();
}

void AppListClientImpl::StartSearch(const std::u16string& trimmed_query) {
  if (search_controller_) {
    if (trimmed_query.empty()) {
      search_controller_->ClearSearch();
    } else {
      search_controller_->StartSearch(trimmed_query);
    }
    OnSearchStarted();

    if (state_for_new_user_) {
      if (!state_for_new_user_->first_search_result_recorded &&
          state_for_new_user_->started_search && trimmed_query.empty()) {
        state_for_new_user_->first_search_result_recorded = true;
        RecordFirstSearchResult(ash::NO_RESULT, IsTabletMode());
      } else if (!trimmed_query.empty()) {
        state_for_new_user_->started_search = true;
      }
    }
  }

  app_list_notifier_->NotifySearchQueryChanged(trimmed_query);
}

void AppListClientImpl::StartZeroStateSearch(base::OnceClosure on_done,
                                             base::TimeDelta timeout) {
  if (search_controller_) {
    search_controller_->StartZeroState(std::move(on_done), timeout);
    OnSearchStarted();
  } else {
    std::move(on_done).Run();
  }
}

void AppListClientImpl::OpenSearchResult(int profile_id,
                                         const std::string& result_id,
                                         int event_flags,
                                         ash::AppListLaunchedFrom launched_from,
                                         ash::AppListLaunchType launch_type,
                                         int suggestion_index,
                                         bool launch_as_default) {
  if (!search_controller_) {
    return;
  }

  auto requested_model_updater_iter = profile_model_mappings_.find(profile_id);
  DCHECK(requested_model_updater_iter != profile_model_mappings_.end());
  DCHECK_EQ(current_model_updater_, requested_model_updater_iter->second);

  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result) {
    return;
  }

  app_list::LaunchData launch_data;
  launch_data.id = result_id;
  launch_data.result_type = result->result_type();
  launch_data.category = result->category();
  launch_data.launch_type = launch_type;
  launch_data.launched_from = launched_from;
  launch_data.suggestion_index = suggestion_index;
  launch_data.score = result->relevance();

  const size_t last_query_length = search_controller_->get_query().size();
  if (launch_type == ash::AppListLaunchType::kAppSearchResult &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox &&
      ash::IsAppListSearchResultAnApp(launch_data.result_type) &&
      last_query_length != 0) {
    ash::RecordSuccessfulAppLaunchUsingSearch(launched_from, last_query_length);
  }

  if (launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    if (IsTabletMode()) {
      base::UmaHistogramCounts100("Apps.AppListSearchQueryLengthV2.TabletMode",
                                  last_query_length);
    } else {
      base::UmaHistogramCounts100(
          "Apps.AppListSearchQueryLengthV2.ClamshellMode", last_query_length);
    }
  }

  // Send training signal to search controller.
  search_controller_->Train(std::move(launch_data));

  app_list_notifier_->NotifyLaunched(
      result->display_type(),
      ash::AppListNotifier::Result(result_id, result->metrics_type()));

  RecordSearchResultOpenTypeHistogram(launched_from, result->metrics_type(),
                                      IsTabletMode());

  if (launch_as_default) {
    RecordDefaultSearchResultOpenTypeHistogram(result->metrics_type());
  }

  if (!last_query_length &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    RecordZeroStateSuggestionOpenTypeHistogram(result->metrics_type());
  }

  if (launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    RecordOpenedResultFromSearchBox(result->metrics_type());
  }

  MaybeRecordLauncherAction(launched_from);

  if (state_for_new_user_ && state_for_new_user_->started_search &&
      !state_for_new_user_->first_search_result_recorded) {
    state_for_new_user_->first_search_result_recorded = true;
    RecordFirstSearchResult(result->metrics_type(), IsTabletMode());
  }

  // OpenResult may cause |result| to be deleted.
  search_controller_->OpenResult(result, event_flags);
}

void AppListClientImpl::InvokeSearchResultAction(
    const std::string& result_id,
    ash::SearchResultActionType action) {
  if (!search_controller_) {
    return;
  }
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (result) {
    search_controller_->InvokeResultAction(result, action);
  }
}

void AppListClientImpl::ActivateItem(int profile_id,
                                     const std::string& id,
                                     int event_flags,
                                     ash::AppListLaunchedFrom launched_from) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];

  // Pointless to notify the AppListModelUpdater of the activated item if the
  // |requested_model_updater| is not the current one, which means that the
  // active profile is changed. The same rule applies to the GetContextMenuModel
  // and ContextMenuItemSelected.
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }

  if (launched_from == ash::AppListLaunchedFrom::kLaunchedFromRecentApps) {
    auto* result = FindAppResultByAppId(search_controller_.get(), id);
    if (result) {
      app_list_notifier_->NotifyLaunched(
          result->display_type(),
          ash::AppListNotifier::Result(result->id(), result->metrics_type()));
    }
  }

  // Send a training signal to the search controller.
  const auto* item = current_model_updater_->FindItem(id);
  if (item) {
    app_list::LaunchData launch_data;
    launch_data.id = id;
    // We don't have easy access to the search result type here, so
    // launch_data.result_type isn't set. However we have no need to distinguish
    // the type of apps launched from the grid in SearchController::Train.
    launch_data.launched_from = launched_from;
    search_controller_->Train(std::move(launch_data));
  }

  CHECK_EQ(requested_model_updater, current_model_updater_);
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(profile_);
  if (scalable_iph) {
    // `ScalableIph` is not available for some profiles.
    scalable_iph->MaybeRecordAppListItemActivation(id);
  }

  MaybeRecordLauncherAction(launched_from);
  requested_model_updater->ActivateChromeItem(id, event_flags);
}

void AppListClientImpl::GetContextMenuModel(
    int profile_id,
    const std::string& id,
    ash::AppListItemContext item_context,
    GetContextMenuModelCallback callback) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    std::move(callback).Run(nullptr);
    return;
  }
  requested_model_updater->GetContextMenuModel(
      id, item_context,
      base::BindOnce(
          [](GetContextMenuModelCallback callback,
             std::unique_ptr<ui::SimpleMenuModel> menu_model) {
            std::move(callback).Run(std::move(menu_model));
          },
          std::move(callback)));
}

void AppListClientImpl::OnAppListVisibilityWillChange(bool visible) {
  if (search_controller_) {
    search_controller_->AppListViewChanging(visible);
  }
}

void AppListClientImpl::OnAppListVisibilityChanged(bool visible) {
  app_list_visible_ = visible;
  if (visible) {
    RecordViewShown();
  } else if (current_model_updater_) {
    current_model_updater_->OnAppListHidden();

    // Record whether user took action first time they opened the launcher.
    // Note that this is recorded only on first user session (otherwise
    // `state_for_new_user_` will not be set).
    if (state_for_new_user_ && state_for_new_user_->showing_recorded &&
        !state_for_new_user_->first_open_success_recorded) {
      state_for_new_user_->first_open_success_recorded = true;

      if (state_for_new_user_->shown_in_tablet_mode) {
        base::UmaHistogramBoolean(
            "Apps.AppList.SuccessfulFirstUsageByNewUsers.TabletMode",
            state_for_new_user_->action_recorded);
      } else {
        base::UmaHistogramBoolean(
            "Apps.AppList.SuccessfulFirstUsageByNewUsers.ClamshellMode",
            state_for_new_user_->action_recorded);
      }
    }
    // If the user started search, record no action if a result open event has
    // not been yet recorded.
    if (state_for_new_user_ && state_for_new_user_->started_search &&
        !state_for_new_user_->first_search_result_recorded) {
      state_for_new_user_->first_search_result_recorded = true;
      RecordFirstSearchResult(ash::NO_RESULT, IsTabletMode());
    }
  }
}

void AppListClientImpl::OnSearchResultVisibilityChanged(const std::string& id,
                                                        bool visibility) {
  if (!search_controller_) {
    return;
  }

  ChromeSearchResult* result = search_controller_->FindSearchResult(id);
  if (result == nullptr) {
    return;
  }
  result->OnVisibilityChanged(visibility);
}

void AppListClientImpl::OnQuickSettingsChanged(
    const std::string& setting_name,
    const std::map<std::string, int>& values) {}

void AppListClientImpl::ActiveUserChanged(user_manager::User* active_user) {
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    // In tests, the user before switching and the one after switching may
    // be both new. It should not happen in the real world.
    state_for_new_user_ = StateForNewUser();
  } else if (state_for_new_user_) {
    if (!state_for_new_user_->showing_recorded) {
      // We assume that the previous user before switching was new if
      // `state_for_new_user_` is not null.
      if (IsTabletMode()) {
        base::UmaHistogramEnumeration(
            "Apps.AppListUsageByNewUsers.TabletMode",
            AppListUsageStateByNewUsers::kNotUsedBeforeSwitchingAccounts);
      } else {
        base::UmaHistogramEnumeration(
            "Apps.AppListUsageByNewUsers.ClamshellMode",
            AppListUsageStateByNewUsers::kNotUsedBeforeSwitchingAccounts);
      }
    }
    state_for_new_user_.reset();
  }

  if (!active_user->is_profile_created()) {
    return;
  }

  UpdateProfile();
}

void AppListClientImpl::UpdateProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  // AppListSyncableService is null in tests.
  if (syncable_service) {
    SetProfile(profile);
  }
}

void AppListClientImpl::SetProfile(Profile* new_profile) {
  if (profile_ == new_profile) {
    return;
  }

  if (profile_) {
    DCHECK(current_model_updater_);
    current_model_updater_->SetActive(false);

    search_controller_.reset();
    app_sync_ui_state_watcher_.reset();
    current_model_updater_ = nullptr;
  }

  template_url_service_observation_.Reset();

  profile_ = new_profile;
  if (!profile_) {
    GetAppListController()->ClearActiveModel();
    return;
  }

  // If we are in guest mode, the new profile should be an OffTheRecord profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening links in a browser window (see
  // http://crbug.com/460437).
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Guest mode must use OffTheRecord profile";

  template_url_service_observation_.Observe(
      TemplateURLServiceFactory::GetForProfile(profile_));

  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  current_model_updater_ = syncable_service->GetModelUpdater();
  current_model_updater_->SetActive(true);

  // On ChromeOS, there is no way to sign-off just one user. When signing off
  // all users, AppListClientImpl instance is destructed before profiles are
  // unloaded. So we don't need to remove elements from
  // |profile_model_mappings_| explicitly.
  profile_model_mappings_[current_model_updater_->model_id()] =
      current_model_updater_;

  app_sync_ui_state_watcher_ =
      std::make_unique<AppSyncUIStateWatcher>(profile_, current_model_updater_);

  SetUpSearchUI();
  OnTemplateURLServiceChanged();
  RecalculateWouldTriggerLauncherSearchIph();
}

void AppListClientImpl::SetUpSearchUI() {
  search_controller_ = app_list::CreateSearchController(
      profile_, current_model_updater_, this, GetNotifier(),
      ash::Shell::Get()->federated_service_controller());

  // Refresh the results used for the suggestion chips with empty query.
  // This fixes crbug.com/999287.
  StartSearch(std::u16string());
}

app_list::SearchController* AppListClientImpl::search_controller() {
  return search_controller_.get();
}

void AppListClientImpl::SetSearchControllerForTest(
    std::unique_ptr<app_list::SearchController> test_controller) {
  search_controller_ = std::move(test_controller);
}

AppListModelUpdater* AppListClientImpl::GetModelUpdaterForTest() {
  return current_model_updater_;
}

void AppListClientImpl::InitializeAsIfNewUserLoginForTest() {
  new_user_session_activation_time_ = base::Time::Now();
  state_for_new_user_ = StateForNewUser();
}

void AppListClientImpl::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "AppListClientImpl::OnSessionStateChanged");
  // Return early if the current user is not new or the session is not active.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew() ||
      !IsSessionActive()) {
    return;
  }

  new_user_session_activation_time_ = base::Time::Now();
}

void AppListClientImpl::OnTemplateURLServiceChanged() {
  DCHECK(current_model_updater_);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  const bool is_google =
      default_provider &&
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  current_model_updater_->SetSearchEngineIsGoogle(is_google);
  search_controller_->OnDefaultSearchIsGoogleSet(is_google);
}

void AppListClientImpl::ShowAppList(ash::AppListShowSource source) {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  if (!app_list_controller_) {
    return;
  }
  app_list_controller_->ShowAppList(source);
}

Profile* AppListClientImpl::GetCurrentAppListProfile() const {
  return ChromeShelfController::instance()->profile();
}

ash::AppListController* AppListClientImpl::GetAppListController() const {
  return app_list_controller_;
}

void AppListClientImpl::DismissView() {
  if (!app_list_controller_) {
    return;
  }
  app_list_controller_->DismissAppList();
}

aura::Window* AppListClientImpl::GetAppListWindow() {
  return app_list_controller_->GetWindow();
}

int64_t AppListClientImpl::GetAppListDisplayId() {
  aura::Window* const app_list_window = GetAppListWindow();
  if (!app_list_window) {
    return display::kInvalidDisplayId;
  }
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(app_list_window)
      .id();
}

bool AppListClientImpl::IsAppPinned(const std::string& app_id) {
  return ChromeShelfController::instance()->IsAppPinned(app_id);
}

bool AppListClientImpl::IsAppOpen(const std::string& app_id) const {
  return ChromeShelfController::instance()->IsOpen(ash::ShelfID(app_id));
}

void AppListClientImpl::PinApp(const std::string& app_id) {
  PinAppWithIDToShelf(app_id);
}

void AppListClientImpl::UnpinApp(const std::string& app_id) {
  UnpinAppWithIDFromShelf(app_id);
}

AppListControllerDelegate::Pinnable AppListClientImpl::GetPinnable(
    const std::string& app_id) {
  return GetPinnableForAppID(app_id,
                             ChromeShelfController::instance()->profile());
}

void AppListClientImpl::CreateNewWindow(bool incognito,
                                        bool should_trigger_session_restore) {
  ash::NewWindowDelegate::GetInstance()->NewWindow(
      incognito, should_trigger_session_restore);
}

void AppListClientImpl::OpenURL(Profile* profile,
                                const GURL& url,
                                ui::PageTransition transition,
                                WindowOpenDisposition disposition) {
  if (crosapi::browser_util::IsLacrosEnabled()) {
    // Handle os:// URLs directly, without involving Lacros.
    // See comment in OmniboxLacrosProvider::StartWithoutSearchProvider.
    if (crosapi::gurl_os_handler_utils::HasOsScheme(url)) {
      const GURL ash_url =
          crosapi::gurl_os_handler_utils::GetAshUrlFromLacrosUrl(url);
      if (ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(ash_url)) {
        crosapi::UrlHandlerAsh().OpenUrl(ash_url);
      } else {
        LOG(WARNING) << "URL not supported: " << url << " (" << ash_url << ")";
      }
    } else {
      ash::NewWindowDelegate::GetPrimary()->OpenUrl(
          url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
          ConvertDisposition(disposition));
    }
  } else {
    NavigateParams params(profile, url, transition);
    params.disposition = disposition;
    Navigate(&params);
  }
}

ash::AppListNotifier* AppListClientImpl::GetNotifier() {
  return app_list_notifier_.get();
}

void AppListClientImpl::RecalculateWouldTriggerLauncherSearchIph() {
  // This can be called before a `Profile` is set to `AppListClientImpl`. If a
  // `Profile` is not set yet, return here. `AppListClientImpl::SetProfile` will
  // call this method once a `Profile` is set.
  if (!current_model_updater_) {
    return;
  }

  current_model_updater_->RecalculateWouldTriggerLauncherSearchIph();
}

std::unique_ptr<ash::ScopedIphSession>
AppListClientImpl::CreateLauncherSearchIphSession() {
  if (profile_ == nullptr) {
    return nullptr;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  if (!tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHLauncherSearchHelpUiFeature)) {
    return nullptr;
  }

  // If we call `ShouldTriggerHelpUI` above, we must show an IPH, i.e. we must
  // return `ScopedIphSessionImpl`.
  return std::make_unique<ScopedIphSessionImpl>(
      tracker, feature_engagement::kIPHLauncherSearchHelpUiFeature);
}

void AppListClientImpl::LoadIcon(int profile_id, const std::string& app_id) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }
  requested_model_updater->LoadAppIcon(app_id);
}

ash::AppListSortOrder AppListClientImpl::GetPermanentSortingOrder() const {
  // `profile_` could be set after a user session gets added to the existing
  // session in tests, which does not happen on real devices.
  if (!profile_) {
    return ash::AppListSortOrder::kCustom;
  }

  return app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
      ->GetPermanentSortingOrder();
}

void AppListClientImpl::RecordViewShown() {
  base::RecordAction(base::UserMetricsAction("Launcher_Show"));

  // Record the time duration between session activation and the first launcher
  // showing if the current user is new.

  // We do not need to worry about the scenario below:
  // log in to a new account -> switch to another account -> switch back to the
  // initial account-> show the launcher
  // In this case, when showing the launcher, the current user is not
  // new anymore.
  // TODO(https://crbug.com/1211620): If this bug is fixed, we might need to
  // do some changes here.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew()) {
    DCHECK(!state_for_new_user_);
    return;
  }

  // Record launcher usage only when the session is active.
  // TODO(https://crbug.com/1248250): handle ui events during OOBE in a more
  // elegant way. For example, do not bother showing the app list when handling
  // the app list toggling event because the app list is not visible in OOBE.
  if (!IsSessionActive()) {
    return;
  }

  // Return early if `state_for_new_user_` is null.
  // TODO(https://crbug.com/1278947): Theoretically, `state_for_new_user_`
  // should be meaningful when the current user is new. However, it is not hold
  // under some edge cases. When the root issue gets fixed, replace it with a
  // check statement.
  if (!state_for_new_user_) {
    return;
  }

  if (state_for_new_user_->showing_recorded) {
    // Showing launcher was recorded before so return early.
    return;
  }

  state_for_new_user_->showing_recorded = true;
  state_for_new_user_->shown_in_tablet_mode = IsTabletMode();

  CHECK(new_user_session_activation_time_.has_value());
  const base::TimeDelta opening_duration =
      base::Time::Now() - *new_user_session_activation_time_;
  // `base::Time` may skew. Therefore only record when the time duration is
  // non-negative.
  if (opening_duration >= base::TimeDelta()) {
    if (state_for_new_user_->shown_in_tablet_mode) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps."
          "TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
          "TabletMode",
          /*sample=*/opening_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);

      base::UmaHistogramEnumeration("Apps.AppListUsageByNewUsers.TabletMode",
                                    AppListUsageStateByNewUsers::kUsed);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps."
          "TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
          "ClamshellMode",
          /*sample=*/opening_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);

      base::UmaHistogramEnumeration("Apps.AppListUsageByNewUsers.ClamshellMode",
                                    AppListUsageStateByNewUsers::kUsed);
    }
  }
}

void AppListClientImpl::RecordOpenedResultFromSearchBox(
    ash::SearchResultType result_type) {
  // Check whether there is any Chrome non-app browser window open and not
  // minimized.
  bool non_app_browser_open_and_not_minimzed = false;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->type() != Browser::TYPE_NORMAL ||
        browser->window()->IsMinimized()) {
      // Skip if `browser` is not a normal browser or `browser` is minimized.
      continue;
    }

    non_app_browser_open_and_not_minimzed = true;
    break;
  }

  if (non_app_browser_open_and_not_minimzed) {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.OpenedAppListSearchResultFromSearchBoxV2."
        "ExistNonAppBrowserWindowOpenAndNotMinimized",
        result_type, ash::SEARCH_RESULT_TYPE_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.OpenedAppListSearchResultFromSearchBoxV2."
        "NonAppBrowserWindowsEitherClosedOrMinimized",
        result_type, ash::SEARCH_RESULT_TYPE_BOUNDARY);
  }
}

void AppListClientImpl::MaybeRecordLauncherAction(
    ash::AppListLaunchedFrom launched_from) {
  DCHECK(launched_from == ash::AppListLaunchedFrom::kLaunchedFromGrid ||
         launched_from == ash::AppListLaunchedFrom::kLaunchedFromRecentApps ||
         launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox ||
         launched_from == ash::AppListLaunchedFrom::kLaunchedFromContinueTask ||
         launched_from ==
             ash::AppListLaunchedFrom::kLaunchedFromQuickAppAccess);

  // Return early if the current user is not new.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew()) {
    DCHECK(!state_for_new_user_);
    return;
  }

  // The launcher action has been recorded so return early.
  if (state_for_new_user_->action_recorded) {
    return;
  }

  state_for_new_user_->action_recorded = true;
  if (IsTabletMode()) {
    base::UmaHistogramEnumeration("Apps.NewUserFirstLauncherAction.TabletMode",
                                  launched_from);
  } else {
    base::UmaHistogramEnumeration(
        "Apps.NewUserFirstLauncherAction.ClamshellMode", launched_from);
  }

  DCHECK(new_user_session_activation_time_.has_value());
  const base::TimeDelta launcher_action_duration =
      base::Time::Now() - *new_user_session_activation_time_;
  if (launcher_action_duration >= base::TimeDelta()) {
    // `base::Time` may skew. Therefore only record when the time duration is
    // non-negative.
    if (IsTabletMode()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction."
          "TabletMode",
          /*sample=*/launcher_action_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction."
          "ClamshellMode",
          /*sample=*/launcher_action_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);
    }
  }
}
