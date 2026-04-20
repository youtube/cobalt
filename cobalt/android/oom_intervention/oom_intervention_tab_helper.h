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

#ifndef COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
#define COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/oom_intervention/oom_intervention.mojom.h"

namespace content {
class WebContents;
}

// A tab helper for near-OOM intervention.
class OomInterventionTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OomInterventionTabHelper>,
      public blink::mojom::OomInterventionHost {
 public:
  static bool IsEnabled();

  ~OomInterventionTabHelper() override;

  // blink::mojom::OomInterventionHost:
  void OnHighMemoryUsage() override;

 private:
  explicit OomInterventionTabHelper(content::WebContents* web_contents);

  friend class content::WebContentsUserData<OomInterventionTabHelper>;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Starts observing near-OOM situation if it's not started.
  void StartMonitoringIfNeeded();
  // Stops observing near-OOM situation.
  void StopMonitoring();
  // Starts detecting near-OOM situation in renderer.
  void StartDetectionInRenderer();

  // Called when NearOomMonitor detects near-OOM situation.
  void OnNearOomDetected();

  // Called when we stop monitoring high memory usage in the foreground
  // renderer.
  void OnDetectionWindowElapsedWithoutHighMemoryUsage();

  void ResetInterventionState();

  void ResetInterfaces();

  bool navigation_started_ = false;
  absl::optional<base::TimeTicks> near_oom_detected_time_;
  base::OneShotTimer renderer_detection_timer_;

  mojo::Remote<blink::mojom::OomIntervention> intervention_;

  mojo::Receiver<blink::mojom::OomInterventionHost> receiver_{this};

  base::TimeTicks last_navigation_timestamp_;
  base::TimeTicks start_monitor_timestamp_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // NOTE: Do not add member variables after weak_factory_
  // It should be the first one destroyed among all members.
  // See base/memory/weak_ptr.h.
  base::WeakPtrFactory<OomInterventionTabHelper> weak_factory_{this};
};

#endif  // COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
