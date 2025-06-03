// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"

#import <UIKit/UIKit.h>

#import <stdint.h>
#import <string>
#import <utility>
#import <vector>

#import "base/base64.h"
#import "base/check.h"
#import "base/command_line.h"
#import "base/debug/dump_without_crashing.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/persistent_histogram_allocator.h"
#import "base/path_service.h"
#import "base/process/process_metrics.h"
#import "base/rand_util.h"
#import "base/strings/safe_sprintf.h"
#import "base/task/thread_pool.h"
#import "base/threading/platform_thread.h"
#import "components/crash/core/common/crash_key.h"
#import "components/crash/core/common/crash_keys.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/call_stack_profile_metrics_provider.h"
#import "components/metrics/cpu_metrics_provider.h"
#import "components/metrics/demographics/demographic_metrics_provider.h"
#import "components/metrics/drive_metrics_provider.h"
#import "components/metrics/entropy_state_provider.h"
#import "components/metrics/field_trials_provider.h"
#import "components/metrics/metrics_data_validation.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_reporting_default_state.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/net/cellular_logic_helper.h"
#import "components/metrics/net/net_metrics_log_uploader.h"
#import "components/metrics/net/network_metrics_provider.h"
#import "components/metrics/persistent_histograms.h"
#import "components/metrics/stability_metrics_helper.h"
#import "components/metrics/ui/form_factor_metrics_provider.h"
#import "components/metrics/ui/screen_info_metrics_provider.h"
#import "components/metrics/url_constants.h"
#import "components/metrics/version_utils.h"
#import "components/omnibox/browser/omnibox_metrics_provider.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_device_info/device_count_metrics_provider.h"
#import "components/ukm/ukm_service.h"
#import "components/variations/variations_associated_data.h"
#import "components/version_info/version_info.h"
#import "google_apis/google_api_keys.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/metrics/chrome_browser_state_client.h"
#import "ios/chrome/browser/metrics/ios_chrome_default_browser_metrics_provider.h"
#import "ios/chrome/browser/metrics/ios_chrome_signin_and_sync_status_metrics_provider.h"
#import "ios/chrome/browser/metrics/ios_chrome_stability_metrics_provider.h"
#import "ios/chrome/browser/metrics/ios_family_link_user_metrics_provider.h"
#import "ios/chrome/browser/metrics/ios_feed_activity_metrics_provider.h"
#import "ios/chrome/browser/metrics/ios_feed_enabled_metrics_provider.h"
#import "ios/chrome/browser/metrics/ios_profile_session_metrics_provider.h"
#import "ios/chrome/browser/metrics/mobile_session_shutdown_metrics_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/tab_parenting_global_observer.h"
#import "ios/chrome/browser/translate/model/translate_ranker_metrics_provider.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Maximum amount of local storage for storing persistent histograms.
const int kMaxHistogramStorageKiB = 50 << 10;  // 50 MiB

void GetNetworkConnectionTrackerAsync(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback) {
  std::move(callback).Run(
      GetApplicationContext()->GetNetworkConnectionTracker());
}

std::unique_ptr<metrics::FileMetricsProvider> CreateFileMetricsProvider(
    bool metrics_reporting_enabled) {
  // Create an object to monitor files of metrics and include them in reports.
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics_provider(
      new metrics::FileMetricsProvider(
          GetApplicationContext()->GetLocalState()));

  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    base::FilePath browser_metrics_upload_dir =
        user_data_dir.AppendASCII(kBrowserMetricsName);
    if (metrics_reporting_enabled) {
      metrics::FileMetricsProvider::Params browser_metrics_params(
          browser_metrics_upload_dir,
          metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          metrics::FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE,
          kBrowserMetricsName);
      browser_metrics_params.max_dir_kib = kMaxHistogramStorageKiB;
      browser_metrics_params.filter = base::BindRepeating(
          &IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles);
      file_metrics_provider->RegisterSource(browser_metrics_params);
    } else {
      // When metrics reporting is not enabled, any existing files should be
      // deleted in order to preserve user privacy.
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::GetDeletePathRecursivelyCallback(
              std::move(browser_metrics_upload_dir)));
    }
  }
  return file_metrics_provider;
}

}  // namespace

// UKM suffix for field trial recording.
const char kUKMFieldTrialSuffix[] = "UKM";

IOSChromeMetricsServiceClient::IOSChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
      stability_metrics_provider_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  notification_listeners_active_ = RegisterForNotifications();
}

IOSChromeMetricsServiceClient::~IOSChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
std::unique_ptr<IOSChromeMetricsServiceClient>
IOSChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager) {
  // Perform two-phase initialization so that `client->metrics_service_` only
  // receives pointers to fully constructed objects.
  std::unique_ptr<IOSChromeMetricsServiceClient> client(
      new IOSChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void IOSChromeMetricsServiceClient::RegisterPrefs(
    PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
  metrics::FileMetricsProvider::RegisterSourcePrefs(registry,
                                                    kBrowserMetricsName);
  metrics::RegisterMetricsReportingStatePrefs(registry);
  ukm::UkmService::RegisterPrefs(registry);
}

variations::SyntheticTrialRegistry*
IOSChromeMetricsServiceClient::GetSyntheticTrialRegistry() {
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* IOSChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

ukm::UkmService* IOSChromeMetricsServiceClient::GetUkmService() {
  return ukm_service_.get();
}

void IOSChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

int32_t IOSChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string IOSChromeMetricsServiceClient::GetApplicationLocale() {
  return GetApplicationContext()->GetApplicationLocale();
}

const network_time::NetworkTimeTracker*
IOSChromeMetricsServiceClient::GetNetworkTimeTracker() {
  return GetApplicationContext()->GetNetworkTimeTracker();
}

bool IOSChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  brand_code->assign(ios::provider::GetBrandCode());
  return true;
}

metrics::SystemProfileProto::Channel
IOSChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(::GetChannel());
}

bool IOSChromeMetricsServiceClient::IsExtendedStableChannel() {
  return false;  // Not supported on iOS.
}

std::string IOSChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void IOSChromeMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = std::move(done_callback);
  CollectFinalHistograms();
}

std::unique_ptr<metrics::MetricsLogUploader>
IOSChromeMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  return std::make_unique<metrics::NetMetricsLogUploader>(
      GetApplicationContext()->GetSharedURLLoaderFactory(), server_url,
      insecure_server_url, mime_type, service_type, on_upload_complete);
}

base::TimeDelta IOSChromeMetricsServiceClient::GetStandardUploadInterval() {
  return metrics::GetUploadInterval(metrics::ShouldUseCellularUploadInterval());
}

void IOSChromeMetricsServiceClient::WebStateDidStartLoading(
    web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::WebStateDidStopLoading(
    web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::Initialize() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  synthetic_trial_registry_ =
      std::make_unique<variations::SyntheticTrialRegistry>(
          IsExternalExperimentAllowlistEnabled());

  synthetic_trial_observation_.Observe(synthetic_trial_registry_.get());

  metrics_service_ = std::make_unique<metrics::MetricsService>(
      metrics_state_manager_, this, local_state);
  RegisterMetricsServiceProviders();

  if (IsMetricsReportingForceEnabled() ||
      base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    ukm_service_ = std::make_unique<ukm::UkmService>(
        local_state, this,
        std::make_unique<metrics::DemographicMetricsProvider>(
            std::make_unique<metrics::ChromeBrowserStateClient>(),
            metrics::MetricsLogUploader::MetricServiceType::UKM));
    // As this is startup, there the UKM previous state is the same as the
    // present state.
    OnUkmAllowedStateChanged(/* must_purge */ false, GetUkmConsentState());
    RegisterUKMProviders();
  }
}

void IOSChromeMetricsServiceClient::RegisterMetricsServiceProviders() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          base::BindRepeating(&GetNetworkConnectionTrackerAsync)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<OmniboxMetricsProvider>());

  auto stability_metrics_provider =
      std::make_unique<IOSChromeStabilityMetricsProvider>(local_state);
  stability_metrics_provider_ = stability_metrics_provider.get();
  metrics_service_->RegisterMetricsProvider(
      std::move(stability_metrics_provider));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSChromeDefaultBrowserMetricsProvider>(
          metrics::MetricsLogUploader::MetricServiceType::UMA));

  // NOTE: metrics_state_manager_->IsMetricsReportingEnabled() returns false
  // during local testing. To test locally, modify
  // MetricsServiceAccessor::IsMetricsReportingEnabled() to return true.
  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      metrics_state_manager_->IsMetricsReportingEnabled()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::EntropyStateProvider>(local_state));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(ios::FILE_LOCAL_STATE));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSChromeSigninAndSyncStatusMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSFamilyLinkUserMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<MobileSessionShutdownMetricsProvider>(
          metrics_service_.get()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::DeviceCountMetricsProvider>(base::BindRepeating(
          &DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<translate::TranslateRankerMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DemographicMetricsProvider>(
          std::make_unique<metrics::ChromeBrowserStateClient>(),
          metrics::MetricsLogUploader::MetricServiceType::UMA));

  // TODO(crbug.com/1491396): Avoid using GetLastUsedBrowserState().
  ChromeBrowserState* browser_state = GetApplicationContext()
                                          ->GetChromeBrowserStateManager()
                                          ->GetLastUsedBrowserState();

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSFeedActivityMetricsProvider>(
          browser_state->GetPrefs()));

  metrics_service_->RegisterMetricsProvider(
      CreateIOSProfileSessionMetricsProvider());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSFeedEnabledMetricsProvider>(
          browser_state->GetPrefs()));
}

void IOSChromeMetricsServiceClient::RegisterUKMProviders() {
  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<variations::FieldTrialsProvider>(
          synthetic_trial_registry_.get(), kUKMFieldTrialSuffix));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<IOSChromeDefaultBrowserMetricsProvider>(
          metrics::MetricsLogUploader::MetricServiceType::UKM));
}

void IOSChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  task_vm_info task_info_data;
  mach_msg_type_number_t count = sizeof(task_vm_info) / sizeof(natural_t);
  kern_return_t kr =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_info_data), &count);
  if (kr == KERN_SUCCESS) {
    mach_vm_size_t footprint_mb = task_info_data.phys_footprint / 1024 / 1024;
    base::UmaHistogramMemoryLargeMB("Memory.Browser.MemoryFootprint",
                                    footprint_mb);
    // The pseudo metric of Memory.Browser.MemoryFootprint. Only used to
    // assess field trial data quality.
    base::UmaHistogramMemoryLargeMB(
        "UMA.Pseudo.Memory.Browser.MemoryFootprint",
        metrics::GetPseudoMetricsSample(
            static_cast<double>(task_info_data.phys_footprint) / 1024 / 1024));

    switch (UIApplication.sharedApplication.applicationState) {
      case UIApplicationStateActive:
        base::UmaHistogramMemoryLargeMB("Memory.Browser.MemoryFootprint.Active",
                                        footprint_mb);
        // According to Apple, apps on iPhone 6 and older devices get terminated
        // by the OS if memory usage crosses 200MB watermark. Obviously this
        // metric will not be recorded with true on iPhone 6 and older devices.
        UMA_HISTOGRAM_BOOLEAN(
            "Memory.Browser.MemoryFootprint.Active.Over200MBWatermark",
            footprint_mb >= 200);
        break;
      case UIApplicationStateInactive:
        base::UmaHistogramMemoryLargeMB(
            "Memory.Browser.MemoryFootprint.Inactive", footprint_mb);
        break;
      case UIApplicationStateBackground:
        base::UmaHistogramMemoryLargeMB(
            "Memory.Browser.MemoryFootprint.Background", footprint_mb);
        break;
    }
  } else {
    // Max kern_return_t is 0x100 = 256, plus trailing null.
    // (https://opensource.apple.com/source/xnu/xnu-792.25.20/osfmk/mach/kern_return.h)
    // TODO(crbug.com/1365392): Remove this when done debugging the uncaught
    // memory regression.
    static crash_reporter::CrashKeyString<4> task_info_kern_return(
        "task-info-kern-return");
    char kr_buf[4];
    base::strings::SafeSPrintf(kr_buf, "%d", kr);
    task_info_kern_return.Set(kr_buf);
    base::debug::DumpWithoutCrashing();
  }

  std::vector<ChromeBrowserState*> loaded_browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  int open_tabs_count = 0;
  for (ChromeBrowserState* browser_state : loaded_browser_states) {
    // Iterate through regular Browser and OTR Browser to find the corresponding
    // tab.
    BrowserList* browser_list =
        BrowserListFactory::GetForBrowserState(browser_state);
    std::set<Browser*> regular_browsers = browser_list->AllRegularBrowsers();
    std::set<Browser*> otr_browsers = browser_list->AllIncognitoBrowsers();

    for (Browser* browser : regular_browsers) {
      open_tabs_count += browser->GetWebStateList()->count();
    }
    for (Browser* browser : otr_browsers) {
      open_tabs_count += browser->GetWebStateList()->count();
    }
  }
  base::UmaHistogramCounts10000("Memory.Browser.MemoryFootprint.NumOpenTabs",
                                open_tabs_count);
  base::UmaHistogramCounts10000(
      "Memory.Browser.MemoryFootprint.NumLiveOverscroll",
      [OverscrollActionsController instanceCount]);

  std::move(collect_final_metrics_done_callback_).Run();
}

bool IOSChromeMetricsServiceClient::RegisterForNotifications() {
  tab_parented_subscription_ =
      TabParentingGlobalObserver::GetInstance()->RegisterCallback(
          base::BindRepeating(&IOSChromeMetricsServiceClient::OnTabParented,
                              base::Unretained(this)));
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::BindRepeating(
              &IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
              base::Unretained(this)));

  std::vector<ChromeBrowserState*> loaded_browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  bool all_profiles_succeeded = true;

  // If this function is called too early (before browser_state is fully
  // initialized), the event listener will not be registered correctly.
  DCHECK_GT(loaded_browser_states.size(), 0u);
  for (ChromeBrowserState* browser_state : loaded_browser_states) {
    if (!RegisterForBrowserStateEvents(browser_state)) {
      all_profiles_succeeded = false;
    }
  }
  return all_profiles_succeeded;
}

bool IOSChromeMetricsServiceClient::RegisterForBrowserStateEvents(
    ChromeBrowserState* browser_state) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS);
  ObserveServiceForDeletions(history_service);
  syncer::SyncService* sync =
      SyncServiceFactory::GetInstance()->GetForBrowserState(browser_state);
  StartObserving(sync, browser_state->GetPrefs());
  return (history_service != nullptr && sync != nullptr);
}

void IOSChromeMetricsServiceClient::OnTabParented(web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

// static
metrics::FileMetricsProvider::FilterAction
IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles(
    const base::FilePath& path) {
  // Do not process the file if it corresponds to the current process id.
  base::ProcessId pid;
  bool parse_success = base::GlobalHistogramAllocator::ParseFilePath(
      path, nullptr, nullptr, &pid);
  if (!parse_success)
    return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
  if (pid == base::GetCurrentProcId())
    return metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID;
  // No need to test whether `pid` is a different active process. This isn't
  // applicable to iOS because there cannot be two copies of Chrome running.
  return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
}

metrics::EnableMetricsDefault
IOSChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      GetApplicationContext()->GetLocalState());
}

void IOSChromeMetricsServiceClient::OnHistoryDeleted() {
  if (ukm_service_)
    ukm_service_->Purge();
}

void IOSChromeMetricsServiceClient::OnUkmAllowedStateChanged(
    bool must_purge,
    ukm::UkmConsentState previous_consent_state) {
  const ukm::UkmConsentState consent_state = GetUkmConsentState();
  if (!ukm_service_)
    return;
  if (must_purge) {
    ukm_service_->Purge();
    ukm_service_->ResetClientState(ukm::ResetReason::kOnUkmAllowedStateChanged);
  } else {
    // Purge recording if required consent has been revoked.
    if (!consent_state.Has(ukm::MSBB)) {
      ukm_service_->PurgeMsbbData();
    }
    // No need to test for ukm::APPS and ukm::EXTENSIONS as they are not
    // supported on iOS.
  }

  // Notify the recording service of changed metrics consent.
  ukm_service_->UpdateRecording(consent_state);

  // Broadcast UKM consent state change.
  ukm_service_->OnUkmAllowedStateChanged(consent_state);

  // Signal service manager to enable/disable UKM based on new state.
  UpdateRunningServices();
}

void IOSChromeMetricsServiceClient::OnIncognitoWebStateAdded() {
  // Signal service manager to enable/disable UKM based on new state.
  UpdateRunningServices();
}

void IOSChromeMetricsServiceClient::OnIncognitoWebStateRemoved() {
  // Signal service manager to enable/disable UKM based on new state.
  UpdateRunningServices();
}

bool IOSChromeMetricsServiceClient::IsUkmAllowedForAllProfiles() {
  return UkmConsentStateObserver::IsUkmAllowedForAllProfiles();
}

bool IOSChromeMetricsServiceClient::
    AreNotificationListenersEnabledOnAllProfiles() {
  return notification_listeners_active_;
}

std::string IOSChromeMetricsServiceClient::GetUploadSigningKey() {
  std::string decoded_key;
  base::Base64Decode(google_apis::GetMetricsKey(), &decoded_key);
  return decoded_key;
}
