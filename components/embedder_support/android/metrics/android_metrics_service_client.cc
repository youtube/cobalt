// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/android_metrics_service_client.h"

#include <jni.h>
#include <cstdint>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_paths_android.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/embedder_support/android/metrics/android_metrics_log_uploader.h"
#include "components/embedder_support/android/metrics/jni/AndroidMetricsServiceClient_jni.h"
#include "components/metrics/android_metrics_provider.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/content/accessibility_metrics_provider.h"
#include "components/metrics/content/content_stability_metrics_provider.h"
#include "components/metrics/content/extensions_helper.h"
#include "components/metrics/content/gpu_metrics_provider.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/entropy_state_provider.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/persistent_histograms.h"
#include "components/metrics/sampling_metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/form_factor_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/version_utils.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/field_trials_provider_helper.h"
#include "components/ukm/ukm_service.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace metrics {

using InstallerPackageType = AndroidMetricsServiceClient::InstallerPackageType;

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

const int kMaxHistogramStorageKiB = 100 << 10;  // 100 MiB

// Divides the spectrum of uint32_t values into 1000 ~equal-sized buckets (range
// [0, 999] inclusive), and returns which bucket |value| falls into. Ex. given
// 2^30, this would return 250, because 25% of uint32_t values fall below the
// given value.
int UintToPerMille(uint32_t value) {
  // We need to divide by UINT32_MAX+1 (2^32), otherwise the fraction could
  // evaluate to 1000.
  uint64_t divisor = 1llu << 32;
  uint64_t value_per_mille = static_cast<uint64_t>(value) * 1000llu / divisor;
  DCHECK_GE(value_per_mille, 0llu);
  DCHECK_LE(value_per_mille, 999llu);
  return static_cast<int>(value_per_mille);
}

bool IsProcessRunning(base::ProcessId pid) {
  // Sending a signal value of 0 will cause error checking to be performed
  // with no signal being sent.
  return (kill(pid, 0) == 0 || errno != ESRCH);
}

metrics::FileMetricsProvider::FilterAction FilterBrowserMetricsFiles(
    const base::FilePath& path) {
  base::ProcessId pid;
  if (!base::GlobalHistogramAllocator::ParseFilePath(path, nullptr, nullptr,
                                                     &pid)) {
    return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
  }

  if (pid == base::GetCurrentProcId())
    return metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID;

  if (IsProcessRunning(pid))
    return metrics::FileMetricsProvider::FILTER_TRY_LATER;

  return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
}

// Constructs the name of a persistent metrics file from a directory and metrics
// name, and either registers that file as associated with a previous run if
// metrics reporting is enabled, or deletes it if not.
void RegisterOrRemovePreviousRunMetricsFile(
    bool metrics_reporting_enabled,
    const base::FilePath& dir,
    base::StringPiece metrics_name,
    metrics::FileMetricsProvider::SourceAssociation association,
    metrics::FileMetricsProvider* file_metrics_provider) {
  base::FilePath metrics_file =
      base::GlobalHistogramAllocator::ConstructFilePath(dir, metrics_name);

  if (metrics_reporting_enabled) {
    // Enable reading any existing saved metrics.
    file_metrics_provider->RegisterSource(metrics::FileMetricsProvider::Params(
        metrics_file,
        metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
        association, metrics_name));
  } else {
    // When metrics reporting is not enabled, any existing file should be
    // deleted in order to preserve user privacy.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::GetDeleteFileCallback(metrics_file));
  }
}

bool IsSamplesCounterEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kPersistentHistogramsFeature, "prev_run_metrics_count_only", false);
}

// TODO(crbug.com/1152072): Unify this implementation with the one in
// ChromeMetricsServiceClient.
std::unique_ptr<metrics::FileMetricsProvider> CreateFileMetricsProvider(
    PrefService* pref_service,
    bool metrics_reporting_enabled) {
  // Create an object to monitor files of metrics and include them in reports.
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics_provider =
      std::make_unique<metrics::FileMetricsProvider>(pref_service);

  base::FilePath user_data_dir;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir);
  // Register the Crashpad metrics files.
  // Register the data from the previous run if crashpad_handler didn't exit
  // cleanly.
  RegisterOrRemovePreviousRunMetricsFile(
      metrics_reporting_enabled, user_data_dir, kCrashpadHistogramAllocatorName,
      metrics::FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,
      file_metrics_provider.get());

  base::FilePath browser_metrics_upload_dir =
      user_data_dir.AppendASCII(kBrowserMetricsName);
  if (metrics_reporting_enabled) {
    metrics::FileMetricsProvider::Params browser_metrics_params(
        browser_metrics_upload_dir,
        metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
        IsSamplesCounterEnabled()
            ? metrics::FileMetricsProvider::
                  ASSOCIATE_INTERNAL_PROFILE_SAMPLES_COUNTER
            : metrics::FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE,
        kBrowserMetricsName);

    browser_metrics_params.max_dir_kib = kMaxHistogramStorageKiB;
    browser_metrics_params.filter =
        base::BindRepeating(FilterBrowserMetricsFiles);
    file_metrics_provider->RegisterSource(browser_metrics_params);

    base::FilePath crashpad_active_path =
        base::GlobalHistogramAllocator::ConstructFilePathForActiveFile(
            user_data_dir, kCrashpadHistogramAllocatorName);
    // Register data that will be populated for the current run. "Active"
    // files need an empty "prefs_key" because they update the file itself.
    file_metrics_provider->RegisterSource(metrics::FileMetricsProvider::Params(
        crashpad_active_path,
        metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
        metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN));
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

  return file_metrics_provider;
}

base::OnceClosure CreateChainedClosure(base::OnceClosure cb1,
                                       base::OnceClosure cb2) {
  return base::BindOnce(
      [](base::OnceClosure cb1, base::OnceClosure cb2) {
        if (cb1) {
          std::move(cb1).Run();
        }
        if (cb2) {
          std::move(cb2).Run();
        }
      },
      std::move(cb1), std::move(cb2));
}

}  // namespace

// Needs to be kept in sync with the writer in
// third_party/crashpad/crashpad/handler/handler_main.cc.
const char kCrashpadHistogramAllocatorName[] = "CrashpadMetrics";

AndroidMetricsServiceClient::AndroidMetricsServiceClient() = default;
AndroidMetricsServiceClient::~AndroidMetricsServiceClient() = default;

// static
void AndroidMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::FileMetricsProvider::RegisterSourcePrefs(registry,
                                                    kBrowserMetricsName);
  metrics::FileMetricsProvider::RegisterSourcePrefs(
      registry, kCrashpadHistogramAllocatorName);
  metrics::FileMetricsProvider::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
  ukm::UkmService::RegisterPrefs(registry);
}

void AndroidMetricsServiceClient::Initialize(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_finished_);

  pref_service_ = pref_service;

  metrics_state_manager_ = MetricsStateManager::Create(
      pref_service_, this,
      // Pass an empty file path since the path is for Extended Variations Safe
      // Mode, which is N/A to Android embedders.
      std::wstring(), base::FilePath(), StartupVisibility::kUnknown,
      {
          // The low entropy provider is used instead of the default provider
          // because the default provider needs to know if UMA is enabled and
          // querying GMS to determine whether UMA is enabled is slow.
          // The low entropy provider has fewer unique experiment combinations,
          // which is better for privacy, but can have crosstalk issues between
          // experiments.
          .default_entropy_provider_type = metrics::EntropyProviderType::kLow,
          .force_benchmarking_mode =
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  cc::switches::kEnableGpuBenchmarking),
      });

  metrics_state_manager_->InstantiateFieldTrialList();

  init_finished_ = true;

  synthetic_trial_registry_ =
      std::make_unique<variations::SyntheticTrialRegistry>(
          IsExternalExperimentAllowlistEnabled());

  // Create the MetricsService immediately so that other code can make use of
  // it. Chrome always creates the MetricsService as well.
  metrics_service_ = std::make_unique<MetricsService>(
      metrics_state_manager_.get(), this, pref_service_);

  // Registration of providers has to wait until consent is determined. To
  // do otherwise means the providers would always be configured with reporting
  // disabled (because when this is called in production consent hasn't been
  // determined). If consent has not been determined, this does nothing.
  MaybeStartMetrics();
}

// TODO:(crbug.com/1148351) Make the initialization consistent with Chrome.
void AndroidMetricsServiceClient::MaybeStartMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsConsentDetermined())
    return;

#if DCHECK_IS_ON()
  // This function should be called only once after consent has been determined.
  DCHECK(!did_start_metrics_with_consent_);
  did_start_metrics_with_consent_ = true;
#endif

  // Treat the debugging flag the same as user consent because the user set it,
  // but keep app_consent_ separate so we never persist data from an opted-out
  // app.
  bool user_consent_or_flag = user_consent_ || IsMetricsReportingForceEnabled();
  if (app_consent_ && user_consent_or_flag) {
    did_start_metrics_ = true;
    // Make GetSampleBucketValue() work properly.
    metrics_state_manager_->ForceClientIdCreation();
    is_client_id_forced_ = true;
    RegisterMetricsProvidersAndInitState();
    // Register for notifications so we can detect when the user or app are
    // interacting with the embedder. We use these as signals to wake up the
    // MetricsService.
    RegisterForNotifications();
    OnMetricsStart();

    if (IsReportingEnabled()) {
      // We assume the embedder has no shutdown sequence, so there's no need
      // for a matching Stop() call.
      metrics_service_->Start();
    }

    CreateUkmService();
  } else {
    // Even though reporting is not enabled, CreateFileMetricsProvider() is
    // called. This ensures on disk state is removed.
    metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
        pref_service_, /* metrics_reporting_enabled */ false));
    OnMetricsNotStarted();
    pref_service_->ClearPref(prefs::kMetricsClientID);
    pref_service_->ClearPref(prefs::kMetricsProvisionalClientID);
    pref_service_->ClearPref(prefs::kMetricsLogRecordId);
  }
}

void AndroidMetricsServiceClient::RegisterMetricsProvidersAndInitState() {
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::SubprocessMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter()));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<CPUMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<EntropyStateProvider>(pref_service_));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ScreenInfoMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      pref_service_, metrics_state_manager_->IsMetricsReportingEnabled()));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<CallStackProfileMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::AndroidMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(
          base::DIR_ANDROID_APP_DATA));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::SamplingMetricsProvider>(
          GetSampleRatePerMille()));
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<AccessibilityMetricsProvider>());
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<ContentStabilityMetricsProvider>(
          pref_service(), /*extensions_helper=*/nullptr));
  RegisterAdditionalMetricsProviders(metrics_service_.get());

  // The file metrics provider performs IO.
  base::ScopedAllowBlocking allow_io;
  metrics_service_->InitializeMetricsRecordingState();
}

void AndroidMetricsServiceClient::CreateUkmService() {
  ukm_service_ = std::make_unique<ukm::UkmService>(
      pref_service_, this, /*demographics_provider=*/nullptr);

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter()));

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      std::make_unique<metrics::FormFactorMetricsProvider>());

  ukm_service_->RegisterMetricsProvider(
      ukm::CreateFieldTrialsProviderForUkm(synthetic_trial_registry_.get()));

  UpdateUkmService();
}

void AndroidMetricsServiceClient::RegisterForNotifications() {
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
}

void AndroidMetricsServiceClient::SetHaveMetricsConsent(bool user_consent,
                                                        bool app_consent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_consent_finished_ = true;
  user_consent_ = user_consent;
  app_consent_ = app_consent;
  MaybeStartMetrics();
}

void AndroidMetricsServiceClient::SetFastStartupForTesting(
    bool fast_startup_for_testing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fast_startup_for_testing_ = fast_startup_for_testing;
}

void AndroidMetricsServiceClient::SetUploadIntervalForTesting(
    const base::TimeDelta& upload_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overridden_upload_interval_ = upload_interval;
}

void AndroidMetricsServiceClient::UpdateUkm(bool must_purge) {
  if (!ukm_service_)
    return;
  if (must_purge) {
    ukm_service_->Purge();
    ukm_service_->ResetClientState(ukm::ResetReason::kOnUkmAllowedStateChanged);
  }

  UpdateUkmService();
}

void AndroidMetricsServiceClient::UpdateUkmService() {
  if (!ukm_service_)
    return;

  bool consent_or_flag = IsConsentGiven() || IsMetricsReportingForceEnabled();
  bool allowed = IsUkmAllowedForAllProfiles();
  bool is_incognito = IsOffTheRecordSessionActive();

  if (consent_or_flag && allowed && !is_incognito) {
    ukm_service_->EnableRecording();
    ukm_service_->EnableReporting();
  } else {
    ukm_service_->DisableRecording();
    ukm_service_->DisableReporting();
  }
}

bool AndroidMetricsServiceClient::IsConsentDetermined() const {
  return init_finished_ && set_consent_finished_;
}

bool AndroidMetricsServiceClient::IsConsentGiven() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_consent_ && app_consent_;
}

bool AndroidMetricsServiceClient::IsReportingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!app_consent_)
    return false;
  return IsMetricsReportingForceEnabled() ||
         (EnabledStateProvider::IsReportingEnabled() && IsInSample());
}

MetricsService* AndroidMetricsServiceClient::GetMetricsServiceIfStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return did_start_metrics_ ? metrics_service_.get() : nullptr;
}

variations::SyntheticTrialRegistry*
AndroidMetricsServiceClient::GetSyntheticTrialRegistry() {
  return synthetic_trial_registry_.get();
}

MetricsService* AndroidMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will be null if initialization hasn't finished.
  return metrics_service_.get();
}

ukm::UkmService* AndroidMetricsServiceClient::GetUkmService() {
  return ukm_service_.get();
}

// In Chrome, UMA and Crashpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In AndroidMetricsServiceClients
// UMA and Crashpad are independent, so this is a no-op.
void AndroidMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {}

std::string AndroidMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

const network_time::NetworkTimeTracker*
AndroidMetricsServiceClient::GetNetworkTimeTracker() {
  return nullptr;
}

bool AndroidMetricsServiceClient::GetBrand(std::string* brand_code) {
  // AndroidMetricsServiceClients don't use brand codes.
  return false;
}

SystemProfileProto::Channel AndroidMetricsServiceClient::GetChannel() {
  return AsProtobufChannel(version_info::android::GetChannel());
}

bool AndroidMetricsServiceClient::IsExtendedStableChannel() {
  return false;  // Not supported on AndroidMetricsServiceClients.
}

std::string AndroidMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void AndroidMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  // Merge histograms from metrics providers into StatisticsRecorder.
  base::StatisticsRecorder::ImportProvidedHistograms();

  base::TimeDelta timeout =
      base::Milliseconds(kMaxHistogramGatheringWaitDuration);

  // Set up the callback task to call after we receive histograms from all
  // child processes. |timeout| specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      CreateChainedClosure(std::move(done_callback),
                           on_final_metrics_collected_listener_),
      timeout);

  if (collect_final_metrics_for_log_closure_)
    std::move(collect_final_metrics_for_log_closure_).Run();
}

std::unique_ptr<MetricsLogUploader> AndroidMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete) {
  if (service_type == metrics::MetricsLogUploader::UKM) {
    // Clearcut doesn't handle UKMs.
    auto url_loader_factory = GetURLLoaderFactory();
    DCHECK(url_loader_factory);
    return std::make_unique<metrics::NetMetricsLogUploader>(
        url_loader_factory, server_url, insecure_server_url, mime_type,
        service_type, on_upload_complete);
  }

  // |server_url|, |insecure_server_url|, and |mime_type| are unused because
  // AndroidMetricsServiceClients send metrics to the platform logging mechanism
  // rather than to Chrome's metrics server.
  return std::make_unique<AndroidMetricsLogUploader>(on_upload_complete);
}

base::TimeDelta AndroidMetricsServiceClient::GetStandardUploadInterval() {
  // In AndroidMetricsServiceClients, metrics collection (when we batch up all
  // logged histograms into a ChromeUserMetricsExtension proto) and metrics
  // uploading (when the proto goes to the server) happen separately.
  //
  // This interval controls the metrics collection rate, so we choose the
  // standard upload interval to make sure we're collecting metrics consistently
  // with Chrome for Android. The metrics uploading rate for
  // AndroidMetricsServiceClients is controlled by the platform logging
  // mechanism. Since this mechanism has its own logic for rate-limiting on
  // cellular connections, we disable the component-layer logic.
  if (!overridden_upload_interval_.is_zero()) {
    return overridden_upload_interval_;
  }
  return metrics::GetUploadInterval(false /* use_cellular_upload_interval */);
}

bool AndroidMetricsServiceClient::IsUkmAllowedForAllProfiles() {
  return false;
}

bool AndroidMetricsServiceClient::ShouldStartUpFastForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fast_startup_for_testing_;
}

void AndroidMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case content::NOTIFICATION_LOAD_STOP:
    case content::NOTIFICATION_LOAD_START:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      metrics_service_->OnApplicationNotIdle();
      break;
    default:
      NOTREACHED();
  }
}

void AndroidMetricsServiceClient::SetCollectFinalMetricsForLogClosureForTesting(
    base::OnceClosure closure) {
  collect_final_metrics_for_log_closure_ = std::move(closure);
}

void AndroidMetricsServiceClient::SetOnFinalMetricsCollectedListenerForTesting(
    base::RepeatingClosure listener) {
  on_final_metrics_collected_listener_ = std::move(listener);
}

int AndroidMetricsServiceClient::GetSampleBucketValue() const {
  DCHECK(is_client_id_forced_);
  return UintToPerMille(base::PersistentHash(metrics_service_->GetClientId()));
}

bool AndroidMetricsServiceClient::IsInSample() const {
  // Called in MaybeStartMetrics(), after |metrics_service_| is created.
  // NOTE IsInSample and ShouldRecordPackageName deliberately use the same hash
  // to guarantee we never exceed 10% of total, opted-in clients for
  // PackageNames.
  return GetSampleBucketValue() < GetSampleRatePerMille();
}

InstallerPackageType AndroidMetricsServiceClient::GetInstallerPackageType() {
  // Check with Java side, to see if it's OK to log the package name for this
  // type of app (see Java side for the specific requirements).
  JNIEnv* env = base::android::AttachCurrentThread();
  int type = Java_AndroidMetricsServiceClient_getInstallerPackageType(env);
  return static_cast<InstallerPackageType>(type);
}

bool AndroidMetricsServiceClient::CanRecordPackageNameForAppType() {
  return GetInstallerPackageType() != InstallerPackageType::OTHER;
}

bool AndroidMetricsServiceClient::ShouldRecordPackageName() {
  return GetSampleBucketValue() < GetPackageNameLimitRatePerMille();
}

void AndroidMetricsServiceClient::RegisterAdditionalMetricsProviders(
    MetricsService* service) {}

std::string AndroidMetricsServiceClient::GetAppPackageNameIfLoggable() {
  if (ShouldRecordPackageName() && CanRecordPackageNameForAppType())
    return GetAppPackageName();
  return std::string();
}

std::string AndroidMetricsServiceClient::GetAppPackageName() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_app_name =
      Java_AndroidMetricsServiceClient_getAppPackageName(env);
  if (j_app_name)
    return ConvertJavaStringToUTF8(env, j_app_name);
  return std::string();
}

bool AndroidMetricsServiceClient::IsOffTheRecordSessionActive() {
  return false;
}

scoped_refptr<network::SharedURLLoaderFactory>
AndroidMetricsServiceClient::GetURLLoaderFactory() {
  return nullptr;
}

}  // namespace metrics
