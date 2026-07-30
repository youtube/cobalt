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

#ifndef COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_CONFIG_H_
#define COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_CONFIG_H_

#include "base/memory/singleton.h"
#include "third_party/blink/public/mojom/oom_intervention/oom_intervention.mojom.h"

class OomInterventionConfig {
 public:
  static const OomInterventionConfig* GetInstance();

  // True when intervention and config is valid.
  bool is_intervention_enabled() const { return is_intervention_enabled_; }

  // True when browser swap monitor is enabled.
  bool is_swap_monitor_enabled() const { return is_swap_monitor_enabled_; }

  // True if Android memory pressure signals should be monitored.
  bool use_components_callback() const { return use_components_callback_; }

  // True if on detection of near OOM condition the renderer JS should be
  // paused.
  bool is_renderer_pause_enabled() const { return is_renderer_pause_enabled_; }

  // True if on detection of near OOM condition the ad iframes should be
  // navigated.
  bool is_navigate_ads_enabled() const { return is_navigate_ads_enabled_; }

  // True if on detection of near OOM condition V8 memory should be purged.
  bool is_purge_v8_memory_enabled() const {
    return is_purge_v8_memory_enabled_;
  }

  // True if detection should be enabled on renderers.
  bool should_detect_in_renderer() const { return should_detect_in_renderer_; }

  // The threshold for swap size in the system to start monitoring.
  uint64_t swapfree_threshold() const { return swapfree_threshold_; }

  // The arguments for detecting near OOM situation in renderer.
  blink::mojom::DetectionArgsPtr GetRendererOomDetectionArgs() const;

 private:
  friend struct base::DefaultSingletonTraits<OomInterventionConfig>;

  OomInterventionConfig();

  bool is_intervention_enabled_ = false;

  bool is_swap_monitor_enabled_ = false;
  bool use_components_callback_ = false;

  bool is_renderer_pause_enabled_ = false;
  bool is_navigate_ads_enabled_ = false;
  bool is_purge_v8_memory_enabled_ = false;
  bool should_detect_in_renderer_ = false;

  uint64_t swapfree_threshold_ = 0;

  blink::mojom::DetectionArgsPtr renderer_detection_args_;
};

#endif  // COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_CONFIG_H_
