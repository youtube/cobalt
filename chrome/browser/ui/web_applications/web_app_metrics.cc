// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics.h"

#include <stdint.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/one_shot_event.h"
#include "base/power_monitor/power_monitor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/value_iterators.h"
#include "base/values.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/sync/base/model_type.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/preinstalled_web_app_window_experiment_utils.h"
#endif

using DisplayMode = blink::mojom::DisplayMode;
using content::WebContents;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace web_app {

namespace {

bool g_disable_automatic_icon_health_checks_for_testing = false;

// Max amount of time to record as a session. If a session exceeds this length,
// treat it as invalid (0 time).
// TODO (crbug.com/1081187): Use an idle timeout instead.
constexpr base::TimeDelta max_valid_session_delta_ = base::Hours(12);

void RecordEngagementHistogram(
    const std::string& histogram_name,
    site_engagement::EngagementType engagement_type) {
  base::UmaHistogramEnumeration(histogram_name, engagement_type,
                                site_engagement::EngagementType::kLast);
}

void RecordTabOrWindowHistogram(
    const std::string& histogram_prefix,
    bool in_window,
    site_engagement::EngagementType engagement_type) {
  RecordEngagementHistogram(
      histogram_prefix + (in_window ? ".InWindow" : ".InTab"), engagement_type);
}

void RecordUserInstalledHistogram(
    bool in_window,
    site_engagement::EngagementType engagement_type) {
  const std::string histogram_prefix = "WebApp.Engagement.UserInstalled";
  RecordTabOrWindowHistogram(histogram_prefix, in_window, engagement_type);
}

bool IsPreferredAppForSupportedLinks(const AppId& app_id, Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  return proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id);
}

}  // namespace

// static
WebAppMetrics* WebAppMetrics::Get(Profile* profile) {
  return WebAppMetricsFactory::GetForProfile(profile);
}

// static
void WebAppMetrics::DisableAutomaticIconHealthChecksForTesting() {
  g_disable_automatic_icon_health_checks_for_testing = true;
}

WebAppMetrics::WebAppMetrics(Profile* profile)
    : SiteEngagementObserver(
          site_engagement::SiteEngagementService::Get(profile)),
      profile_(profile),
      icon_health_checks_(profile),
      browser_tab_strip_tracker_(this, nullptr) {
  browser_tab_strip_tracker_.Init();
  base::PowerMonitor::AddPowerSuspendObserver(this);
  BrowserList::AddObserver(this);

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsIconHealthChecks) &&
      !g_disable_automatic_icon_health_checks_for_testing) {
    AfterStartupTaskUtils::PostTask(
        FROM_HERE, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindOnce(&WebAppIconHealthChecks::Start,
                       icon_health_checks_.GetWeakPtr(), base::DoNothing()));
  }

  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);
  DCHECK(provider);
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppMetrics::CountUserInstalledApps,
                                weak_ptr_factory_.GetWeakPtr()));
}

WebAppMetrics::~WebAppMetrics() {
  BrowserList::RemoveObserver(this);
  base::PowerMonitor::RemovePowerSuspendObserver(this);
}

void WebAppMetrics::OnEngagementEvent(
    WebContents* web_contents,
    const GURL& url,
    double score,
    site_engagement::EngagementType engagement_type) {
  if (!web_contents)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  // Number of apps is not yet counted.
  if (num_user_installed_apps_ == kNumUserInstalledAppsNotCounted)
    return;

  // The engagement broken down by the number of apps installed must be recorded
  // for all engagement events, not just web apps.
  if (num_user_installed_apps_ > 3) {
    RecordEngagementHistogram(
        "WebApp.Engagement.MoreThanThreeUserInstalledApps", engagement_type);
  } else if (num_user_installed_apps_ > 0) {
    RecordEngagementHistogram("WebApp.Engagement.UpToThreeUserInstalledApps",
                              engagement_type);
  } else {
    RecordEngagementHistogram("WebApp.Engagement.NoUserInstalledApps",
                              engagement_type);
  }

  // A presence of WebAppTabHelper with valid app_id indicates an installed
  // web app.
  const AppId* app_id = WebAppTabHelper::GetAppId(web_contents);
  if (!app_id)
    return;

  // No HostedAppBrowserController if app is running as a tab in common browser.
  const bool in_window = !!browser->app_controller();
  const bool user_installed = WebAppProvider::GetForLocalAppsUnchecked(profile_)
                                  ->registrar_unsafe()
                                  .WasInstalledByUser(*app_id);

  // Record all web apps:
  RecordTabOrWindowHistogram("WebApp.Engagement", in_window, engagement_type);

  if (user_installed) {
    RecordUserInstalledHistogram(in_window, engagement_type);
  } else {
    // Record this app into more specific bucket if was installed by default:
    RecordTabOrWindowHistogram("WebApp.Engagement.DefaultInstalled", in_window,
                               engagement_type);
  }
}

void WebAppMetrics::OnBrowserNoLongerActive(Browser* browser) {
  // OnBrowserNoLongerActive is called before OnBrowserSetLastActive for any
  // focus switch.
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // It is possible for the browser's active web contents to be different
  // from foreground_web_contents_ eg. when reparenting tabs. Just make both
  // inactive.
  if (web_contents != foreground_web_contents_)
    UpdateUkmData(web_contents, TabSwitching::kFrom);
  UpdateUkmData(foreground_web_contents_, TabSwitching::kFrom);
  foreground_web_contents_ = nullptr;
}

void WebAppMetrics::OnBrowserSetLastActive(Browser* browser) {
  // OnBrowserNoLongerActive is called before OnBrowserSetLastActive for any
  // focus switch.
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  foreground_web_contents_ = web_contents;
  UpdateUkmData(web_contents, TabSwitching::kTo);
}

void WebAppMetrics::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Process deselection, removal, then selection in-order so we have a
  // consistent view of selected and last-interacted tabs.
  TabSwitching initial_mode = TabSwitching::kFrom;
  // Foreground usage duration should be counted when the web app is being
  // closed, despite IsInAppWindow returning false at this point.
  if (change.type() == TabStripModelChange::kRemoved &&
      tab_strip_model->empty()) {
    auto iter = base::ranges::find(*BrowserList::GetInstance(), tab_strip_model,
                                   &Browser::tab_strip_model);
    if (iter != BrowserList::GetInstance()->end() &&
        (*iter)->type() == Browser::TYPE_APP)
      initial_mode = TabSwitching::kForegroundClosing;
  }
  UpdateUkmData(selection.old_contents, initial_mode);

  foreground_web_contents_ = selection.new_contents;
  // Newly-selected foreground contents should not be going away.
  if (foreground_web_contents_ &&
      foreground_web_contents_->IsBeingDestroyed()) {
    base::debug::DumpWithoutCrashing();
    foreground_web_contents_ = nullptr;
  }

  // Contents being replaced should never be the new selection.
  if (change.type() == TabStripModelChange::kReplaced &&
      change.GetReplace()->old_contents == foreground_web_contents_) {
    base::debug::DumpWithoutCrashing();
    foreground_web_contents_ = nullptr;
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    for (const TabStripModelChange::RemovedTab& contents :
         change.GetRemove()->contents) {
      if (contents.remove_reason ==
          TabStripModelChange::RemoveReason::kDeleted) {
        const AppId* app_id = WebAppTabHelper::GetAppId(contents.contents);
        if (app_id)
          app_last_interacted_time_.erase(*app_id);
        // Newly-selected foreground contents should not be going away.
        if (contents.contents == foreground_web_contents_) {
          base::debug::DumpWithoutCrashing();
          foreground_web_contents_ = nullptr;
        }
      }
    }
  }

  UpdateUkmData(foreground_web_contents_, TabSwitching::kTo);
}

void WebAppMetrics::OnSuspend() {
  // Update current tab as foreground time.
  if (foreground_web_contents_) {
    const AppId* app_id = WebAppTabHelper::GetAppId(foreground_web_contents_);
    if (app_id && app_last_interacted_time_.contains(*app_id)) {
      UpdateUkmData(foreground_web_contents_, TabSwitching::kFrom);
      app_last_interacted_time_.erase(*app_id);
    }
  }
  // Update all other tabs as background time.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    int tab_count = browser->tab_strip_model()->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(i);
      DCHECK(contents);
      const AppId* app_id = WebAppTabHelper::GetAppId(contents);
      if (app_id && app_last_interacted_time_.contains(*app_id)) {
        UpdateUkmData(contents, TabSwitching::kBackgroundClosing);
      }
    }
  }
  app_last_interacted_time_.clear();
}

void WebAppMetrics::NotifyOnAssociatedAppChanged(
    content::WebContents* web_contents,
    const absl::optional<AppId>& previous_app_id,
    const absl::optional<AppId>& new_app_id) {
  // Ensure we aren't counting closed app as still open.
  // TODO (crbug.com/1081187): If there were multiple app instances open, this
  // will prevent background time being counted until the app is next active.
  if (previous_app_id.has_value())
    app_last_interacted_time_.erase(previous_app_id.value());
  // Don't record any UKM data here. It will be recorded in
  // |NotifyInstallableWebAppStatusUpdated| once fully fetched.
}

void WebAppMetrics::NotifyInstallableWebAppStatusUpdated(
    WebContents* web_contents) {
  DCHECK(web_contents);
  // Skip recording if app isn't in the foreground.
  if (web_contents != foreground_web_contents_)
    return;
  // Skip recording if we just recorded this App ID (when switching tabs/windows
  // or on a previous navigation that triggered this event). Otherwise we would
  // count navigations within a web app as sessions.
  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(foreground_web_contents_);
  DCHECK(app_banner_manager);
  if (!app_banner_manager->GetManifestStartUrl().is_valid())
    return;
  if (app_banner_manager->GetManifestStartUrl() ==
      last_recorded_web_app_start_url_) {
    return;
  }

  UpdateUkmData(foreground_web_contents_, TabSwitching::kTo);
}

void WebAppMetrics::NotifyWebContentsDestroyed(WebContents* web_contents) {
  // Won't save us if we set a destroyed WebContents as foreground after this
  // point. Maybe we should keep a set of destroyed WebContents pointers?
  if (foreground_web_contents_ == web_contents)
    foreground_web_contents_ = nullptr;
}

void WebAppMetrics::RemoveBrowserListObserverForTesting() {
  BrowserList::RemoveObserver(this);
}

void WebAppMetrics::CountUserInstalledAppsForTesting() {
  // Reset and re-count.
  num_user_installed_apps_ = kNumUserInstalledAppsNotCounted;
  CountUserInstalledApps();
}

void WebAppMetrics::CountUserInstalledApps() {
  DCHECK_EQ(kNumUserInstalledAppsNotCounted, num_user_installed_apps_);

  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);

  num_user_installed_apps_ =
      provider->registrar_unsafe().CountUserInstalledApps();
  DCHECK_NE(kNumUserInstalledAppsNotCounted, num_user_installed_apps_);
  DCHECK_GE(num_user_installed_apps_, 0);
}

void WebAppMetrics::UpdateUkmData(WebContents* web_contents,
                                  TabSwitching mode) {
  if (!web_contents)
    return;
  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(web_contents);
  // May be null in unit tests.
  if (!app_banner_manager)
    return;
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);
  // WebAppProvider may be removed after WebAppMetrics construction in tests.
  if (!provider)
    return;
  DailyInteraction features;

  const AppId* app_id = WebAppTabHelper::GetAppId(web_contents);
  if (app_id && provider->registrar_unsafe().IsLocallyInstalled(*app_id)) {
    // App is installed
    features.start_url = provider->registrar_unsafe().GetAppStartUrl(*app_id);
    features.installed = true;
    auto install_source =
        provider->registrar_unsafe().GetLatestAppInstallSource(*app_id);
    if (install_source)
      features.install_source = static_cast<int>(*install_source);
    DisplayMode display_mode =
        provider->registrar_unsafe().GetAppEffectiveDisplayMode(*app_id);
    features.effective_display_mode = static_cast<int>(display_mode);
    features.captures_links =
        IsPreferredAppForSupportedLinks(*app_id, profile_);
    // AppBannerManager treats already-installed web-apps as non-promotable, so
    // include already-installed findings as promotable.
    features.promotable = app_banner_manager->IsProbablyPromotableWebApp(
        /*ignore_existing_installations=*/true);
    bool preinstalled_app =
        provider->registrar_unsafe().IsInstalledByDefaultManagement(*app_id);
    // Record usage duration and session counts only for installed web apps that
    // are currently open in a window, and all preinstalled apps.
    if (provider->ui_manager().IsInAppWindow(web_contents) ||
        preinstalled_app || mode == TabSwitching::kForegroundClosing) {
      base::Time now = base::Time::Now();
      if (app_last_interacted_time_.contains(*app_id)) {
        base::TimeDelta delta = now - app_last_interacted_time_[*app_id];
        if (delta < max_valid_session_delta_) {
          switch (mode) {
            case TabSwitching::kFrom:
            case TabSwitching::kForegroundClosing:
              features.foreground_duration = delta;
              break;
            case TabSwitching::kTo:
            case TabSwitching::kBackgroundClosing:
              features.background_duration = delta;
              break;
          }
        }
      }
      app_last_interacted_time_[*app_id] = now;

      // Note: real web app launch counts 2 sessions immediately, as app window
      // is actually activated twice in the launch process.
      if (mode == TabSwitching::kTo)
        features.num_sessions = 1;
    }
#if BUILDFLAG(IS_CHROMEOS)
    auto user_group =
        preinstalled_web_app_window_experiment_utils::GetUserGroupPref(
            profile_->GetPrefs());
    if (user_group !=
        features::PreinstalledWebAppWindowExperimentUserGroup::kUnknown) {
      features.preinstalled_web_app_window_experiment_user_group =
          static_cast<int>(user_group);
      features.preinstalled_web_app_window_experiment_has_launched_before =
          preinstalled_web_app_window_experiment_utils::
              HasLaunchedAppBeforeExperiment(*app_id, profile_->GetPrefs());
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else if (app_banner_manager->IsPromotableWebApp()) {
    // App is not installed, but is promotable. Record a subset of features.
    features.start_url = app_banner_manager->GetManifestStartUrl();
    DCHECK(features.start_url.is_valid());
    features.installed = false;
    DisplayMode display_mode = app_banner_manager->GetManifestDisplayMode();
    features.effective_display_mode = static_cast<int>(display_mode);
    features.promotable = true;
  } else {
    last_recorded_web_app_start_url_ = GURL();
    return;
  }
  last_recorded_web_app_start_url_ = features.start_url;

  FlushOldRecordsAndUpdate(features, profile_,
                           SyncServiceFactory::GetForProfile(profile_));
}

}  // namespace web_app
