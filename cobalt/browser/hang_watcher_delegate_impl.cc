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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
namespace cobalt {
namespace browser {

// static
void CobaltHangWatcherDelegate::Initialize() {
  static base::NoDestructor<CobaltHangWatcherDelegate> instance;
  base::HangWatcher::SetDelegate(instance.get());
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
  return global_features_ ? global_features_ : GlobalFeatures::GetInstance();
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
  // First, check 'GlobalFeatures', which is a process-wide settings dictionary
  // typically populated by the embedder via the JS H5VCC API.
  // We prioritize this to allow embedders to enable hang reporting
  // until the proper Finch bridge is fully functional. If the setting isn't
  // explicitly provided, we fallback to the standard Chromium 'FeatureList'
  // which is backed by Finch.
  auto val = GetIntSetting("EnableHangReporting");
  if (val.has_value()) {
    bool enabled = *val != 0;
    DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangReporting: " << enabled;
    return enabled;
  }

  DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangReporting: using Finch";
  return base::FeatureList::IsEnabled(cobalt::features::kHangReporting);
}

std::optional<base::TimeDelta> CobaltHangWatcherDelegate::GetHangWatchTime() {
  auto val = GetIntSetting("HangWatchTimeSeconds");
  if (!val.has_value() || *val <= 0) {
    return std::nullopt;
  }
  return base::Seconds(*val);
}

std::optional<base::TimeDelta>
CobaltHangWatcherDelegate::GetHangWatchMonitoringPeriod() {
  auto val = GetIntSetting("HangWatchMonitoringPeriodSeconds");
  if (!val.has_value() || *val <= 0) {
    return std::nullopt;
  }
  return base::Seconds(*val);
}

std::optional<bool> CobaltHangWatcherDelegate::IsThreadDumpingEnabled(
    base::HangWatcher::ThreadType thread_type) {
  std::string_view key;
  switch (thread_type) {
    case base::HangWatcher::ThreadType::kMainThread:
      key = "EnableHangWatchMainThreadDump";
      break;
    case base::HangWatcher::ThreadType::kIOThread:
      key = "EnableHangWatchIOThreadDump";
      break;
    case base::HangWatcher::ThreadType::kThreadPoolThread:
      key = "EnableHangWatchThreadPoolDump";
      break;
    case base::HangWatcher::ThreadType::kRendererThread:
      key = "EnableHangWatchRendererThreadDump";
      break;
    default:
      return std::nullopt;
  }

  if (key.empty()) {
    return std::nullopt;
  }

  auto val = GetIntSetting(key);
  if (val.has_value()) {
    return *val != 0;
  }

  return std::nullopt;
}

}  // namespace browser
}  // namespace cobalt
