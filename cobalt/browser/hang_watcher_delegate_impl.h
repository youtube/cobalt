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

#ifndef COBALT_BROWSER_HANG_WATCHER_DELEGATE_IMPL_H_
#define COBALT_BROWSER_HANG_WATCHER_DELEGATE_IMPL_H_

#include <optional>
#include <string>

#include "base/threading/hang_watcher.h"

namespace cobalt {
class GlobalFeatures;
namespace browser {

class CobaltHangWatcherDelegate : public base::HangWatcher::Delegate {
 public:
  CobaltHangWatcherDelegate();
  explicit CobaltHangWatcherDelegate(GlobalFeatures* global_features);
  ~CobaltHangWatcherDelegate() override = default;

  static void Initialize();

  // Configuration is resolved across three tiers:
  //   1. Dynamic JavaScript runtime settings (GlobalFeatures / H5VCC)
  //   2. Server-side Finch feature flags and parameters (cobalt::features)
  //   3. Hardcoded compiled defaults (base::WatchHangsInScope,
  //   cobalt::features)
  //
  // GlobalFeatures (H5VCC) and Finch are treated as mutually exclusive
  // configuration paths. If an embedder explicitly provides a setting via
  // GlobalFeatures, that configuration is authoritative. If the provided
  // setting is invalid, the system logs a message and falls back directly to
  // the safe compiled default rather than silently switching to a Finch value
  // (which the embedder may not have configured or intended to use). Fallback
  // to Finch is reserved strictly for when GlobalFeatures settings are omitted
  // entirely.
  bool IsHangReportingEnabled() override;
  base::TimeDelta GetHangWatchTime() override;
  base::TimeDelta GetHangWatchMonitoringPeriod() override;
  bool IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType thread_type) override;
  bool IsLongHangDetectionEnabled() override;
  bool IsLongHangKillEnabled() override;
  base::TimeDelta GetLongHangTimeout() override;
  void RecordHangStarted(const std::string& hang_uuid) override;
  void RecordHangRecovered(const std::string& hang_uuid) override;

 private:
  GlobalFeatures* GetGlobalFeatures();
  std::optional<int64_t> GetIntSetting(std::string_view key);

  GlobalFeatures* global_features_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_HANG_WATCHER_DELEGATE_IMPL_H_
