// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/help_app_provider.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr size_t kMinQueryLength = 3u;
constexpr double kMinScore = 0.4;
constexpr size_t kNumRequestedResults = 5u;

// The end result of a list search. Logged once per time a list search finishes.
// Not logged if the search is canceled by a new search starting. Not logged for
// suggestion chips. These values persist to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class ListSearchResultState {
  // Search finished with no problems.
  kOk = 0,
  // Search canceled because no help app icon.
  kNoHelpAppIcon = 1,
  // Search canceled because the search backend isn't available.
  kSearchHandlerUnavailable = 2,
  kMaxValue = kSearchHandlerUnavailable,
};

// Use this when a list search finishes.
void LogListSearchResultState(ListSearchResultState state) {
  base::UmaHistogramEnumeration(
      "Apps.AppList.HelpAppProvider.ListSearchResultState", state);
}

}  // namespace

HelpAppResult::HelpAppResult(
    const float& relevance,
    Profile* profile,
    const ash::help_app::mojom::SearchResultPtr& result,
    const gfx::ImageSkia& icon,
    const std::u16string& query)
    : profile_(profile),
      url_path_(result->url_path_with_parameters),
      help_app_content_id_(result->id) {
  DCHECK(profile_);
  set_id(ash::kChromeUIHelpAppURL + url_path_);
  set_relevance(relevance);
  SetTitle(result->title);
  SetCategory(Category::kHelp);
  SetResultType(ResultType::kHelpApp);
  SetDisplayType(DisplayType::kList);
  SetMetricsType(ash::HELP_APP_DEFAULT);
  SetIcon(IconInfo(icon, kAppIconDimension));
  SetDetails(result->main_category);
}

HelpAppResult::~HelpAppResult() = default;

void HelpAppResult::Open(int event_flags) {
  // This is a google-internal histogram. If changing this, also change the
  // corresponding histograms file.
  base::UmaHistogramSparse("Discover.LauncherSearch.ContentLaunched",
                           base::PersistentHash(help_app_content_id_));

  // Note: event_flags is ignored, LaunchSWA doesn't need it.
  // Launch list result.
  ash::SystemAppLaunchParams params;
  params.url = GURL(ash::kChromeUIHelpAppURL + url_path_);
  params.launch_source = apps::LaunchSource::kFromAppListQuery;
  ash::LaunchSystemWebAppAsync(
      profile_, ash::SystemWebAppType::HELP, params,
      std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
}

HelpAppProvider::HelpAppProvider(Profile* profile,
                                 ash::help_app::SearchHandler* search_handler)
    : profile_(profile), search_handler_(search_handler) {
  DCHECK(profile_);

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  Observe(&app_service_proxy_->AppRegistryCache());
  LoadIcon();

  // TODO(b/261867385): We manually load the icon from the local codebase as
  // the icon load from proxy is flaky. When the flakiness if solved, we can
  // safely remove this.
  icon_ = gfx::CreateVectorIcon(app_list::kHelpAppIcon, kAppIconDimension,
                                SK_ColorTRANSPARENT);

  if (!search_handler_) {
    return;
  }
  search_handler_->OnProfileDirAvailable(profile->GetPath());
  search_handler_->Observe(
      search_results_observer_receiver_.BindNewPipeAndPassRemote());
}

HelpAppProvider::~HelpAppProvider() = default;

void HelpAppProvider::Start(const std::u16string& query) {
  if (query.size() < kMinQueryLength) {
    // Do not do a list search for queries that are too short because the
    // results generally aren't meaningful. This isn't worth logging as a list
    // search result case because it happens frequently when entering a new
    // search query.
    return;
  }

  // Start a search for list results.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  last_query_ = query;

  // Stop the search if:
  //  - the search backend isn't available (or the feature is disabled)
  //  - we don't have an icon to display with results.
  if (!search_handler_) {
    LogListSearchResultState(ListSearchResultState::kSearchHandlerUnavailable);
    return;
  } else if (icon_.isNull()) {
    LogListSearchResultState(ListSearchResultState::kNoHelpAppIcon);
    // This prevents a timeout in the test, but it does not change the user
    // experience because the results were already cleared at the start.
    SearchProvider::Results search_results;
    SwapResults(&search_results);
    return;
  }

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  search_handler_->Search(
      query, kNumRequestedResults,
      base::BindOnce(&HelpAppProvider::OnSearchReturned,
                     weak_factory_.GetWeakPtr(), query, start_time));
}

void HelpAppProvider::StopQuery() {
  last_query_.clear();
  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
}

void HelpAppProvider::OnSearchReturned(
    const std::u16string& query,
    const base::TimeTicks& start_time,
    std::vector<ash::help_app::mojom::SearchResultPtr> sorted_results) {
  DCHECK_LE(sorted_results.size(), kNumRequestedResults);

  SearchProvider::Results search_results;
  for (const auto& result : sorted_results) {
    if (result->relevance_score < kMinScore) {
      continue;
    }

    search_results.emplace_back(std::make_unique<HelpAppResult>(
        result->relevance_score, profile_, result, icon_, last_query_));
  }

  base::UmaHistogramTimes("Apps.AppList.HelpAppProvider.QueryTime",
                          base::TimeTicks::Now() - start_time);
  LogListSearchResultState(ListSearchResultState::kOk);
  SwapResults(&search_results);
}

ash::AppListSearchResultType HelpAppProvider::ResultType() const {
  return ash::AppListSearchResultType::kHelpApp;
}

void HelpAppProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == web_app::kHelpAppId && update.ReadinessChanged() &&
      update.Readiness() == apps::Readiness::kReady) {
    LoadIcon();
  }
}

void HelpAppProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

// If the availability of search results changed, start a new search.
void HelpAppProvider::OnSearchResultAvailabilityChanged() {
  if (last_query_.empty()) {
    return;
  }
  Start(last_query_);
}

void HelpAppProvider::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = icon_value->uncompressed;
  }
}

void HelpAppProvider::LoadIcon() {
  app_service_proxy_->LoadIcon(
      app_service_proxy_->AppRegistryCache().GetAppType(web_app::kHelpAppId),
      web_app::kHelpAppId, apps::IconType::kStandard,
      ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&HelpAppProvider::OnLoadIcon, weak_factory_.GetWeakPtr()));
}

}  // namespace app_list
