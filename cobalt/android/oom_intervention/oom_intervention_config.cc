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

#include "cobalt/android/oom_intervention/oom_intervention_config.h"

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "cobalt/android/oom_intervention/oom_intervention_features.h"

namespace {

bool GetSwapFreeThreshold(uint64_t* threshold) {
  base::SystemMemoryInfoKB memory_info;
  if (!base::GetSystemMemoryInfo(&memory_info)) {
    return false;
  }

  // If there is no swap (zram) the monitor doesn't work because we use
  // SwapFree as the tracking metric.
  if (memory_info.swap_total == 0) {
    return false;
  }

  double swap_memory_ratio = 0.45;
  if (base::FeatureList::IsEnabled(features::kOomIntervention)) {
    swap_memory_ratio = features::kSwapMemoryThresholdRatio.Get();
  }

  *threshold =
      static_cast<uint64_t>(memory_info.swap_total * swap_memory_ratio);
  return true;
}

}  // namespace

OomInterventionConfig::OomInterventionConfig()
    : is_intervention_enabled_(true),
      renderer_detection_args_(blink::mojom::DetectionArgs::New()) {
  if (!is_intervention_enabled_) {
    return;
  }

  // Cobalt does not have UI to unpause the page.
  is_renderer_pause_enabled_ = false;
  is_navigate_ads_enabled_ = false;
  // Purging the entire V8 would cause the page to freeze too.
  is_purge_v8_memory_enabled_ = false;
  should_detect_in_renderer_ = true;
  use_components_callback_ = true;

  if (!GetSwapFreeThreshold(&swapfree_threshold_)) {
    is_swap_monitor_enabled_ = false;
  }

  double physical_memory_ratio = 0.45;
  if (base::FeatureList::IsEnabled(features::kOomIntervention)) {
    physical_memory_ratio = features::kPhysicalMemoryThresholdRatio.Get();
  }
  renderer_detection_args_->private_footprint_threshold =
      base::SysInfo::AmountOfPhysicalMemory() * physical_memory_ratio;
}

// static
const OomInterventionConfig* OomInterventionConfig::GetInstance() {
  return base::Singleton<OomInterventionConfig>::get();
}

blink::mojom::DetectionArgsPtr
OomInterventionConfig::GetRendererOomDetectionArgs() const {
  return renderer_detection_args_.Clone();
}
