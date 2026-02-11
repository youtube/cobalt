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
#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
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

struct CobaltMetricsServiceClient::State
    : public base::RefCountedThreadSafe<CobaltMetricsServiceClient::State> {
  State() = default;

  // Task runner for background memory metrics collection.
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  // Flag to stop logging.
  bool stop_logging = false;

  uint64_t last_private_footprint_kb = 0;
  base::TimeTicks last_dump_time;

  void RecordMemoryMetrics(
      memory_instrumentation::GlobalMemoryDump* global_dump) {
    CobaltMetricsServiceClient::RecordMemoryMetrics(
        global_dump, &last_private_footprint_kb, &last_dump_time);
  }

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
          {"v8", "blink_gc", "malloc", "partition_alloc", "skia", "gpu",
           "media", "blink_objects"},
          base::BindOnce(
              [](scoped_refptr<State> state, base::OnceClosure done_callback,
                 bool success,
                 std::unique_ptr<memory_instrumentation::GlobalMemoryDump>
                     global_dump) {
                if (success && global_dump) {
                  state->RecordMemoryMetrics(global_dump.get());
                }
                if (done_callback) {
                  std::move(done_callback).Run();
                }
              },
              base::RetainedRef(this), base::OnceClosure()));
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
      metrics_state_manager_(state_manager),
      upload_interval_(kStandardUploadIntervalMinutes) {
  COBALT_DETACH_FROM_THREAD(thread_checker_);
}

void CobaltMetricsServiceClient::Initialize() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  metrics_service_ = CreateMetricsServiceInternal(metrics_state_manager_.get(),
                                                  this, local_state_.get());
  log_uploader_ = CreateLogUploaderInternal();
  log_uploader_weak_ptr_ = log_uploader_->GetWeakPtr();
  StartIdleRefreshTimer();
  StartMemoryMetricsLogger();
}

void CobaltMetricsServiceClient::StartMemoryMetricsLogger() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* CobaltMetricsServiceClient::GetMetricsService() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return metrics_service_.get();
}

void CobaltMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return "en-US";
}

const network_time::NetworkTimeTracker*
CobaltMetricsServiceClient::GetNetworkTimeTracker() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // TODO(b/372559349): Figure out whether we need to return a real object.
  // The NetworkTimeTracker used to provide higher-quality wall clock times than
  // |clock_| (when available). Can be overridden for tests.
  NOTIMPLEMENTED();
  return nullptr;
}

bool CobaltMetricsServiceClient::GetBrand(std::string* brand_code) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // "false" means no brand code available. We set the brand when uploading
  // via GEL.
  return false;
}

metrics::SystemProfileProto::Channel CobaltMetricsServiceClient::GetChannel() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // Return value here is unused in downstream logging.
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

bool CobaltMetricsServiceClient::IsExtendedStableChannel() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return false;  // Not supported on Cobalt.
}

std::string CobaltMetricsServiceClient::GetVersionString() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // E.g. 134.0.6998.19.
  return base::Version().GetString();
}

void CobaltMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // Chrome keeps the actual URL in an internal file, likely to avoid abuse.
  // This below is made up, and in any case likely not to be used (it ends up in
  // CreateUploader()'s `server_url`. Return empty and use instead logic in
  // CobaltMetricsLogUploader.
  return GURL("https://youtube.com/tv/uma");
}

GURL CobaltMetricsServiceClient::GetInsecureMetricsServerUrl() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // This is made up and not used. See GetMetricsServerUrl() for more details.
  return GURL("https://youtube.com/tv/uma");
}

std::unique_ptr<metrics::MetricsLogUploader>
CobaltMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    std::string_view mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
                  {"v8", "blink_gc", "malloc", "partition_alloc", "skia", "gpu",
                   "media", "blink_objects"},
                  base::BindOnce(
                      [](scoped_refptr<State> state,
                         base::OnceClosure done_callback, bool success,
                         std::unique_ptr<
                             memory_instrumentation::GlobalMemoryDump>
                             global_dump) {
                        if (success && global_dump) {
                          state->RecordMemoryMetrics(global_dump.get());
                        }
                        if (done_callback) {
                          std::move(done_callback).Run();
                        }
                      },
                      state, std::move(done_callback)));
            } else {
              std::move(done_callback).Run();
            }
          },
          state_, std::move(done_callback)));
}

// static
void CobaltMetricsServiceClient::RecordMemoryMetrics(
    memory_instrumentation::GlobalMemoryDump* global_dump,
    uint64_t* last_private_footprint_kb,
    base::TimeTicks* last_dump_time) {
  uint64_t total_private_footprint_kb = 0;
  uint64_t total_resident_kb = 0;
  uint64_t total_js_heap_kb = 0;
  uint64_t total_dom_kb = 0;
  uint64_t total_layout_kb = 0;
  uint64_t total_graphics_kb = 0;
  uint64_t total_media_kb = 0;
  uint64_t total_native_kb = 0;

  uint64_t total_documents = 0;
  uint64_t total_nodes = 0;
  uint64_t total_js_event_listeners = 0;
  uint64_t total_layout_objects = 0;

  for (const auto& process_dump : global_dump->process_dumps()) {
    total_private_footprint_kb += process_dump.os_dump().private_footprint_kb;

    uint32_t resident_kb = process_dump.os_dump().resident_set_kb;
    total_resident_kb += resident_kb;

    std::optional<uint64_t> js_heap =
        process_dump.GetMetric("v8", "effective_size");
    if (js_heap) {
      total_js_heap_kb += (*js_heap / 1024);
    }

    std::optional<uint64_t> dom =
        process_dump.GetMetric("blink_gc", "effective_size");
    if (dom) {
      total_dom_kb += (*dom / 1024);
    }

    std::optional<uint64_t> layout =
        process_dump.GetMetric("partition_alloc/partitions/layout", "size");
    if (layout) {
      total_layout_kb += (*layout / 1024);
    }

    std::optional<uint64_t> skia =
        process_dump.GetMetric("skia", "effective_size");
    if (skia) {
      total_graphics_kb += (*skia / 1024);
    }
    std::optional<uint64_t> gpu =
        process_dump.GetMetric("gpu", "effective_size");
    if (gpu) {
      total_graphics_kb += (*gpu / 1024);
    }

    std::optional<uint64_t> media =
        process_dump.GetMetric("media", "effective_size");
    if (media) {
      total_media_kb += (*media / 1024);
    }

    std::optional<uint64_t> native =
        process_dump.GetMetric("malloc", "effective_size");
    if (native) {
      total_native_kb += (*native / 1024);
    }

    total_documents +=
        process_dump.GetMetric("blink_objects/Document", "object_count")
            .value_or(0);
    total_nodes += process_dump.GetMetric("blink_objects/Node", "object_count")
                       .value_or(0);
    total_js_event_listeners +=
        process_dump.GetMetric("blink_objects/JSEventListener", "object_count")
            .value_or(0);
    total_layout_objects +=
        process_dump.GetMetric("blink_objects/LayoutObject", "object_count")
            .value_or(0);
  }

  if (total_private_footprint_kb > 0) {
    uint64_t total_private_footprint_mb = total_private_footprint_kb / 1024;
    MEMORY_METRICS_HISTOGRAM_MB("Memory.Total.PrivateMemoryFootprint",
                                total_private_footprint_mb);

    if (last_private_footprint_kb && last_dump_time) {
      base::TimeTicks now = base::TimeTicks::Now();
      if (!last_dump_time->is_null()) {
        base::TimeDelta delta_t = now - *last_dump_time;
        // Only record growth rate if the samples are within a reasonable window
        // (e.g., 30 minutes) to avoid misleading averages after sleep/wake.
        if (delta_t >= base::Seconds(1) && delta_t <= base::Minutes(30)) {
          int64_t delta_mem_kb =
              static_cast<int64_t>(total_private_footprint_kb) -
              static_cast<int64_t>(*last_private_footprint_kb);
          double minutes = delta_t.InSecondsF() / 60.0;
          double growth_rate_kb_per_min = delta_mem_kb / minutes;

          if (growth_rate_kb_per_min >= 0) {
            int rate_int = static_cast<int>(growth_rate_kb_per_min);
            // Slow leak metric: 0 to 10 MB/min (10240 KB/min), high resolution.
            base::UmaHistogramCounts10000(
                "Cobalt.Memory.PrivateMemoryFootprint.GrowthRate.Slow",
                rate_int);
            // Fast leak metric: 0 to 1 GB/min.
            base::UmaHistogramCounts1M(
                "Cobalt.Memory.PrivateMemoryFootprint.GrowthRate.Fast",
                rate_int);
          }
        }
      }
      *last_private_footprint_kb = total_private_footprint_kb;
      *last_dump_time = now;
    }
  }

  if (total_resident_kb > 0) {
    MEMORY_METRICS_HISTOGRAM_MB("Memory.Total.Resident",
                                total_resident_kb / 1024);
  }
  if (total_js_heap_kb > 0) {
    MEMORY_METRICS_HISTOGRAM_MB("Cobalt.Memory.JavaScript",
                                total_js_heap_kb / 1024);
  }
  if (total_dom_kb > 0) {
    MEMORY_METRICS_HISTOGRAM_MB("Cobalt.Memory.DOM", total_dom_kb / 1024);
  }
  if (total_layout_kb > 0) {
    MEMORY_METRICS_HISTOGRAM_MB("Cobalt.Memory.Layout", total_layout_kb / 1024);
  }
  if (total_graphics_kb > 0) {
    MEMORY_METRICS_HISTOGRAM_MB("Cobalt.Memory.Graphics",
                                total_graphics_kb / 1024);
  }
  if (total_media_kb > 0) {
    MEMORY_METRICS_HISTOGRAM_MB("Cobalt.Memory.Media", total_media_kb / 1024);
  }
  if (total_native_kb > 0) {
    MEMORY_METRICS_HISTOGRAM_MB("Cobalt.Memory.Native", total_native_kb / 1024);
  }

  if (total_documents > 0) {
    base::UmaHistogramCounts1000("Cobalt.Memory.ObjectCounts.Document",
                                 total_documents);
  }
  if (total_nodes > 0) {
    base::UmaHistogramCounts1M("Cobalt.Memory.ObjectCounts.Node", total_nodes);
  }
  if (total_js_event_listeners > 0) {
    base::UmaHistogramCounts100000("Cobalt.Memory.ObjectCounts.JSEventListener",
                                   total_js_event_listeners);
  }
  if (total_layout_objects > 0) {
    base::UmaHistogramCounts100000("Cobalt.Memory.ObjectCounts.LayoutObject",
                                   total_layout_objects);
  }
}

}  // namespace cobalt
