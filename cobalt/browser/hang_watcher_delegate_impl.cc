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

#include "cobalt/browser/hang_watcher_delegate_impl.h"

#include <cstdint>
#include <optional>

#include "base/feature_list.h"
#include "cobalt/browser/core_metric_components_manager.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"

namespace cobalt {
namespace browser {

// static
void CobaltHangWatcherDelegate::Initialize() {
  static base::NoDestructor<CobaltHangWatcherDelegate> instance;
  base::HangWatcher::SetDelegate(instance.get());
}

bool CobaltHangWatcherDelegate::IsHangReportingEnabled() {
  // First, check 'GlobalFeatures', which is a process-wide settings dictionary
  // typically populated by the embedder via the JS H5VCC API.
  // We prioritize this to allow embedders to enable hang reporting
  // until the proper Finch bridge is fully functional. If the setting isn't
  // explicitly provided, we fallback to the standard Chromium 'FeatureList'
  // which is backed by Finch.

  if (auto* global_features = GlobalFeatures::GetInstance()) {
    const auto& settings = global_features->GetSettings();
    auto it = settings.find("EnableHangReporting");
    if (it != settings.end()) {
      if (const auto* val = std::get_if<int64_t>(&it->second)) {
        bool enabled = (*val != 0);
        DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangReporting: "
                   << enabled;
        return enabled;
      }
    }
  }
  DLOG(INFO) << "CobaltHangWatcherDelegate: EnableHangReporting: using Finch";

  return base::FeatureList::IsEnabled(cobalt::features::kHangReporting);
}

void CobaltHangWatcherDelegate::OnHangStarted(const std::string& hang_uuid) {
  CoreMetricComponentsManager::GetInstance()->RecordHangStarted(hang_uuid);
}

}  // namespace browser
}  // namespace cobalt
