// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/file_descriptor_shuffle.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "cobalt/browser/metrics/cobalt_metrics_log_uploader.h"
#include "cobalt/browser/switches.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "url/gurl.h"

namespace cobalt {

namespace {

void OnMemoryDumpDone(
    base::OnceClosure done_callback,
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> global_dump) {
  if (success && global_dump) {
    CobaltMetricsServiceClient::RecordMemoryMetrics(global_dump.get());
  }

  if (done_callback) {
    std::move(done_callback).Run();
  }
}

}  // namespace

struct CobaltMetricsServiceClient::State
    : public base::RefCountedThreadSafe<CobaltMetricsServiceClient::State> {
  State() = default;

  // Task runner for background memory metrics collection.
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  // Flag to stop logging.
  bool stop_logging = false;

  void RecordMemoryMetricsAfterDelay() {
    if (stop_logging) {
      return;
    }

    base::TimeDelta delay = memory_instrumentation::GetDelayForNextMemoryLog();
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kMemoryMetricsInterval)) {
      std::string interval_str =
          command_line->GetSwitchValueASCII(switches::kMemoryMetricsInterval);
      int interval_int;
      if (base::StringToInt(interval_str, &interval_int) && interval_int > 0) {
        delay = base::Seconds(interval_int);
      } else {
        LOG(ERROR) << "Invalid memory metrics interval: " << interval_str;
      }
    }

    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&State::RequestMemoryMetrics, base::RetainedRef(this)),
        delay);
  }

  void RequestMemoryMetrics() {
    if (stop_logging) {
      return;
    }

    auto* instrumentation =
        memory_instrumentation::MemoryInstrumentation::GetInstance();
    if (instrumentation) {
      instrumentation->RequestGlobalDump(
          {}, base::BindOnce(&OnMemoryDumpDone, base::OnceClosure()));
    }
    RecordMemoryMetricsAfterDelay();
  }

 private:
  friend class base::RefCountedThreadSafe<State>;
  ~State() = default;
};

CobaltMetricsServiceClient::CobaltMetricsServiceClient(
    metrics::MetricsStateManager* state_manager,
    std::unique_ptr<variations::SyntheticTrialRegistry>
        synthetic_trial_registry,
    PrefService* local_state)
    : synthetic_trial_registry_(std::move(synthetic_trial_registry)),
      local_state_(local_state),
      metrics_state_manager_(state_manager) {
  DETACH_FROM_THREAD(thread_checker_);
}

void CobaltMetricsServiceClient::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  metrics_service_ = CreateMetricsServiceInternal(metrics_state_manager_.get(),
                                                  this, local_state_.get());
  log_uploader_ = CreateLogUploaderInternal();
  log_uploader_weak_ptr_ = log_uploader_->GetWeakPtr();
  StartIdleRefreshTimer();
  StartMemoryMetricsLogger();
}

void CobaltMetricsServiceClient::StartMemoryMetricsLogger() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = base::MakeRefCounted<State>();
  state_->task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  state_->task_runner->PostTask(
      FROM_HERE, base::BindOnce(&State::RecordMemoryMetricsAfterDelay,
                                base::RetainedRef(state_)));
}

void CobaltMetricsServiceClient::StartIdleRefreshTimer() {
  if (idle_refresh_timer_.IsRunning()) {
    idle_refresh_timer_.Stop();
  }

  // At a rate of half the upload interval, we force UMA to consider the app
  // as non-idle. This guarantees metrics are uploaded regularly. This is done
  // for two reasons:
  //
  //   1) The nature of the YouTube application is such that the user can be
  //      "idle" for long periods (e.g., watching a movie). We still want to
  //      send metrics in these cases.
  //
  //   2) The typical way Chromium handles non-idle is page loads and user
  //      actions. User actions are currently sparse and/or not working due to
  //      the nature of Kabuki's implementation (see b/417477183).
  auto timer_interval = GetStandardUploadInterval() / 2;
  timer_interval = timer_interval > min_idle_refresh_interval_
                       ? timer_interval
                       : min_idle_refresh_interval_;
  idle_refresh_timer_.Start(
      FROM_HERE, timer_interval, this,
      &CobaltMetricsServiceClient::OnApplicationNotIdleInternal);
  DLOG(INFO) << "Starting refresh timer for: "
             << idle_refresh_timer_.GetCurrentDelay().InSeconds() << " seconds";
}

std::unique_ptr<metrics::MetricsService>
CobaltMetricsServiceClient::CreateMetricsServiceInternal(
    metrics::MetricsStateManager* state_manager,
    metrics::MetricsServiceClient* client,
    PrefService* local_state) {
  return std::make_unique<metrics::MetricsService>(state_manager, client,
                                                   local_state);
}

// static
std::unique_ptr<CobaltMetricsServiceClient> CobaltMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager,
    std::unique_ptr<variations::SyntheticTrialRegistry>
        synthetic_trial_registry,
    PrefService* local_state) {
  // Perform two-phase initialization so that `client->metrics_service_` only
  // receives pointers to fully constructed objects.
  std::unique_ptr<CobaltMetricsServiceClient> client(
      new CobaltMetricsServiceClient(
          state_manager, std::move(synthetic_trial_registry), local_state));
  client->Initialize();

  return client;
}

variations::SyntheticTrialRegistry*
CobaltMetricsServiceClient::GetSyntheticTrialRegistry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* CobaltMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return metrics_service_.get();
}

void CobaltMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // ClientId is unnecessary within Cobalt. We expect the web client responsible
  // for uploading these to have its own concept of device/client identifiers.
}

// TODO(b/286884542): Audit all stub implementations in this class and reaffirm
// they're not needed and/or add a reasonable implementation.
int32_t CobaltMetricsServiceClient::GetProduct() {
  // Note, Product is a Chrome concept and similar dimensions will get logged
  // elsewhere downstream. This value doesn't matter.
  return 0;
}

std::string CobaltMetricsServiceClient::GetApplicationLocale() {
  // The locale will be populated by the web client, so return value is
  // inconsequential.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return "en-US";
}

const network_time::NetworkTimeTracker*
CobaltMetricsServiceClient::GetNetworkTimeTracker() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // TODO(b/372559349): Figure out whether we need to return a real object.
  // The NetworkTimeTracker used to provide higher-quality wall clock times than
  // |clock_| (when available). Can be overridden for tests.
  NOTIMPLEMENTED();
  return nullptr;
}

bool CobaltMetricsServiceClient::GetBrand(std::string* brand_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // "false" means no brand code available. We set the brand when uploading
  // via GEL.
  return false;
}

metrics::SystemProfileProto::Channel CobaltMetricsServiceClient::GetChannel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // Return value here is unused in downstream logging.
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

bool CobaltMetricsServiceClient::IsExtendedStableChannel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return false;  // Not supported on Cobalt.
}

std::string CobaltMetricsServiceClient::GetVersionString() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // E.g. 134.0.6998.19.
  return base::Version().GetString();
}

void CobaltMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());

  std::move(done_callback).Run();

  // Reset the idle state but don't call OnApplicationNotIdleInternal to avoid
  // potential confusion/recursion if it were to ever call this again.
  GetMetricsService()->OnApplicationNotIdle();
}
void CobaltMetricsServiceClient::OnApplicationNotIdleInternal() {
  // MetricsService will shut itself down if the app doesn't periodically tell
  // it it's not idle. In Cobalt's case, we don't want this behavior. Watch
  // sessions for LR can happen for extended periods of time with no action by
  // the user. So, we always just set the app as "non-idle" immediately after
  // each metric log is finalized.
  GetMetricsService()->OnApplicationNotIdle();
}

GURL CobaltMetricsServiceClient::GetMetricsServerUrl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // Chrome keeps the actual URL in an internal file, likely to avoid abuse.
  // This below is made up, and in any case likely not to be used (it ends up in
  // CreateUploader()'s `server_url`. Return empty and use instead logic in
  // CobaltMetricsLogUploader.
  return GURL("https://youtube.com/tv/uma");
}

GURL CobaltMetricsServiceClient::GetInsecureMetricsServerUrl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // This is made up and not used. See GetMetricsServerUrl() for more details.
  return GURL("https://youtube.com/tv/uma");
}

std::unique_ptr<metrics::MetricsLogUploader>
CobaltMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // Uploader should already be initialized early in construction.
  CHECK(log_uploader_);
  log_uploader_->setOnUploadComplete(on_upload_complete);
  return std::move(log_uploader_);
}

std::unique_ptr<CobaltMetricsLogUploader>
CobaltMetricsServiceClient::CreateLogUploaderInternal() {
  return std::make_unique<CobaltMetricsLogUploader>();
}

base::TimeDelta CobaltMetricsServiceClient::GetStandardUploadInterval() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return upload_interval_;
}

void CobaltMetricsServiceClient::SetUploadInterval(base::TimeDelta interval) {
  upload_interval_ = interval;
  // If upload interval changes, update idle refresh timer accordingly.
  StartIdleRefreshTimer();
}

CobaltMetricsServiceClient::~CobaltMetricsServiceClient() {
  if (state_) {
    state_->stop_logging = true;
  }
}

void CobaltMetricsServiceClient::SetMetricsListener(
    ::mojo::PendingRemote<::h5vcc_metrics::mojom::MetricsListener> listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  log_uploader_weak_ptr_->SetMetricsListener(std::move(listener));
}

void CobaltMetricsServiceClient::ScheduleRecordForTesting(
    base::OnceClosure done_callback) {
  state_->task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<State> state, base::OnceClosure done_callback) {
            if (state->stop_logging) {
              std::move(done_callback).Run();
              return;
            }
            auto* instrumentation =
                memory_instrumentation::MemoryInstrumentation::GetInstance();
            if (instrumentation) {
              instrumentation->RequestGlobalDump(
                  {},
                  base::BindOnce(&OnMemoryDumpDone, std::move(done_callback)));
            } else {
              std::move(done_callback).Run();
            }
          },
          state_, std::move(done_callback)));
}

// static
void CobaltMetricsServiceClient::RecordMemoryMetrics(
    memory_instrumentation::GlobalMemoryDump* global_dump) {
  uint64_t total_private_footprint_kb = 0;
  for (const auto& process_dump : global_dump->process_dumps()) {
    total_private_footprint_kb += process_dump.os_dump().private_footprint_kb;
  }

  if (total_private_footprint_kb > 0) {
    uint64_t total_private_footprint_mb = total_private_footprint_kb / 1024;
    MEMORY_METRICS_HISTOGRAM_MB("Memory.Total.PrivateMemoryFootprint",
                                total_private_footprint_mb);
  }
}

}  // namespace cobalt
