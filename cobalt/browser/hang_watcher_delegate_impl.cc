// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/hang_watcher_delegate_impl.h"

#include <optional>
#include <string>
#include <variant>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/h5vcc_native_stability/native_stability_manager.h"
#include "cobalt/build/configs/buildflags.h"

namespace cobalt {
namespace browser {

// static
void CobaltHangWatcherDelegate::Initialize() {
  static base::NoDestructor<CobaltHangWatcherDelegate> instance;
  base::HangWatcher::SetDelegate(instance.get());
  base::HangWatcher::UpdateConfiguration();
}

CobaltHangWatcherDelegate::CobaltHangWatcherDelegate()
    : global_features_(nullptr) {}

CobaltHangWatcherDelegate::CobaltHangWatcherDelegate(
    GlobalFeatures* global_features)
    : global_features_(global_features) {}

// We evaluate GlobalFeatures::GetInstance() lazily here rather than in the
// default constructor. CobaltHangWatcherDelegate is instantiated extremely
// early during browser startup. Calling GetInstance() at that time would
// prematurely initialize background threads (Metrics, Experiments) before
// the Chromium TaskEnvironment and thread pool are ready, causing a FATAL
// crash.
GlobalFeatures* CobaltHangWatcherDelegate::GetGlobalFeatures() {
  if (global_features_) {
    return global_features_;
  }

  // GlobalFeatures::GetInstance() may require ThreadPool to initialize or
  // access settings. We must ensure it's available to avoid crashes during
  // early startup or shutdown.
  if (!base::ThreadPoolInstance::Get()) {
    return nullptr;
  }
  return GlobalFeatures::GetInstance();
}

std::optional<int64_t> CobaltHangWatcherDelegate::GetIntSetting(
    std::string_view key) {
  auto* features = GetGlobalFeatures();
  if (!features) {
    return std::nullopt;
  }

  auto val_opt = features->GetSetting(key);
  if (!val_opt) {
    return std::nullopt;
  }

  const auto* val_int = std::get_if<int64_t>(&val_opt.value());
  if (!val_int) {
    return std::nullopt;
  }

  return *val_int;
}

bool CobaltHangWatcherDelegate::IsHangReportingEnabled() {
  // Check 'GlobalFeatures', which is populated by the embedder via the JS H5VCC
  // API. We prioritize this to allow embedders runtime override control until
  // the Finch bridge is fully functional. If omitted, we fallback to Finch
  // ('base::FeatureList').
  // Note: GlobalFeatures and Finch are treated as mutually exclusive. If an
  // embedder explicitly sets a key, that intent is honored: invalid or
  // non-positive values fall back directly to compiled defaults rather than
  // silently activating an unintended Finch experiment group.
  auto val = GetIntSetting("EnableHangReporting");
  if (val.has_value()) {
    bool enabled = *val != 0;
    DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangReporting: " << enabled;
    return enabled;
  }

  DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangReporting: using Finch";
  return base::FeatureList::IsEnabled(cobalt::features::kHangReporting);
}

base::TimeDelta CobaltHangWatcherDelegate::GetHangWatchTime() {
  auto val = GetIntSetting("HangWatchTimeSeconds");
  if (val.has_value()) {
    if (*val <= 0) {
      DLOG(INFO) << "CobaltHangWatcherDelegate: HangWatchTimeSeconds: " << *val
                 << " is non-positive, using default";
      return base::WatchHangsInScope::kDefaultHangWatchTime;
    }
    DLOG(INFO) << "CobaltHangWatcherDelegate: HangWatchTimeSeconds: " << *val;
    return base::Seconds(*val);
  }

  DLOG(INFO) << "CobaltHangWatcherDelegate: HangWatchTimeSeconds: using Finch";
  int timeout_seconds = cobalt::features::kHangWatchTimeSeconds.Get();
  if (timeout_seconds <= 0) {
    return base::WatchHangsInScope::kDefaultHangWatchTime;
  }
  return base::Seconds(timeout_seconds);
}

base::TimeDelta CobaltHangWatcherDelegate::GetHangWatchMonitoringPeriod() {
  auto val = GetIntSetting("HangWatchMonitoringPeriodSeconds");
  if (val.has_value()) {
    if (*val <= 0) {
      DLOG(INFO) << "CobaltHangWatcherDelegate: "
                    "HangWatchMonitoringPeriodSeconds: "
                 << *val << " is non-positive, using default";
      return base::Seconds(
          cobalt::features::kHangWatchMonitoringPeriodSeconds.default_value);
    }
    DLOG(INFO) << "CobaltHangWatcherDelegate: "
                  "HangWatchMonitoringPeriodSeconds: "
               << *val;
    return base::Seconds(*val);
  }

  DLOG(INFO) << "CobaltHangWatcherDelegate: "
                "HangWatchMonitoringPeriodSeconds: using Finch";
  int period_seconds =
      cobalt::features::kHangWatchMonitoringPeriodSeconds.Get();
  if (period_seconds <= 0) {
    return base::Seconds(
        cobalt::features::kHangWatchMonitoringPeriodSeconds.default_value);
  }
  return base::Seconds(period_seconds);
}

bool CobaltHangWatcherDelegate::IsThreadDumpingEnabled(
    base::HangWatcher::ThreadType thread_type) {
  std::string_view key;
  const base::Feature* feature = nullptr;
  switch (thread_type) {
    case base::HangWatcher::ThreadType::kMainThread:
      key = "EnableHangWatchMainThreadDump";
      feature = &cobalt::features::kHangWatchMainThreadDump;
      break;
    case base::HangWatcher::ThreadType::kIOThread:
      key = "EnableHangWatchIOThreadDump";
      feature = &cobalt::features::kHangWatchIOThreadDump;
      break;
    case base::HangWatcher::ThreadType::kThreadPoolThread:
      key = "EnableHangWatchThreadPoolDump";
      feature = &cobalt::features::kHangWatchThreadPoolDump;
      break;
    case base::HangWatcher::ThreadType::kRendererThread:
      key = "EnableHangWatchRendererThreadDump";
      feature = &cobalt::features::kHangWatchRendererThreadDump;
      break;
    default:
      return false;
  }

  auto val = GetIntSetting(key);
  if (val.has_value()) {
    bool enabled = *val != 0;
    DLOG(INFO) << "CobaltHangWatcherDelegate: " << key << ": " << enabled;
    return enabled;
  }

  DLOG(INFO) << "CobaltHangWatcherDelegate: " << key << ": using Finch";
  return base::FeatureList::IsEnabled(*feature);
}

bool CobaltHangWatcherDelegate::IsLongHangDetectionEnabled() {
  auto val = GetIntSetting("EnableHangWatcherLongHangDetection");
  if (val.has_value()) {
    DLOG(INFO)
        << "CobaltHangWatcherDelegate: EnableHangWatcherLongHangDetection: "
        << (*val != 0);
    return *val != 0;
  }
  DLOG(INFO) << "CobaltHangWatcherDelegate: "
                "EnableHangWatcherLongHangDetection: using Finch";
  return base::FeatureList::IsEnabled(
      cobalt::features::kHangWatcherLongHangDetection);
}

bool CobaltHangWatcherDelegate::IsLongHangKillEnabled() {
  auto val = GetIntSetting("EnableHangWatcherLongHangKill");
  if (val.has_value()) {
    DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangWatcherLongHangKill: "
               << (*val != 0);
    return *val != 0;
  }
  DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangWatcherLongHangKill: "
                "using Finch";
  return base::FeatureList::IsEnabled(
      cobalt::features::kHangWatcherLongHangKill);
}

base::TimeDelta CobaltHangWatcherDelegate::GetLongHangTimeout() {
  auto val = GetIntSetting("LongHangTimeoutSeconds");
  if (val.has_value()) {
    if (*val <= 0) {
      DLOG(INFO) << "CobaltHangWatcherDelegate: LongHangTimeoutSeconds: "
                 << *val << " is non-positive, using default";
      return base::Seconds(
          cobalt::features::kLongHangTimeoutSeconds.default_value);
    }
    DLOG(INFO) << "CobaltHangWatcherDelegate: LongHangTimeoutSeconds: " << *val;
    return base::Seconds(*val);
  }

  DLOG(INFO)
      << "CobaltHangWatcherDelegate: LongHangTimeoutSeconds: using Finch";
  int timeout_seconds = cobalt::features::kLongHangTimeoutSeconds.Get();
  if (timeout_seconds <= 0) {
    return base::Seconds(
        cobalt::features::kLongHangTimeoutSeconds.default_value);
  }
  return base::Seconds(timeout_seconds);
}

void CobaltHangWatcherDelegate::RecordHangStarted(
    const std::string& hang_uuid) {
#if BUILDFLAG(USE_EVERGREEN)
  auto* nsm = h5vcc_native_stability::NativeStabilityManager::GetInstance();
  if (nsm) {
    nsm->RecordHangStarted(hang_uuid);
  }
#endif
}

void CobaltHangWatcherDelegate::RecordHangRecovered(
    const std::string& hang_uuid) {
#if BUILDFLAG(USE_EVERGREEN)
  auto* nsm = h5vcc_native_stability::NativeStabilityManager::GetInstance();
  if (nsm) {
    nsm->RecordHangRecovered(hang_uuid);
  }
#endif
}

}  // namespace browser
}  // namespace cobalt
