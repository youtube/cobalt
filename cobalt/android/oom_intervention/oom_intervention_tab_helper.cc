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

#include "cobalt/android/oom_intervention/oom_intervention_tab_helper.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "cobalt/android/oom_intervention/oom_intervention_config.h"
#include "cobalt/android/oom_intervention/oom_intervention_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"

namespace {

content::WebContents* g_last_visible_web_contents = nullptr;

bool IsLastVisibleWebContents(content::WebContents* web_contents) {
  return web_contents == g_last_visible_web_contents;
}

void SetLastVisibleWebContents(content::WebContents* web_contents) {
  g_last_visible_web_contents = web_contents;
}

}  // namespace

// static
bool OomInterventionTabHelper::IsEnabled() {
  return OomInterventionConfig::GetInstance()->is_intervention_enabled();
}

OomInterventionTabHelper::OomInterventionTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OomInterventionTabHelper>(*web_contents) {}

OomInterventionTabHelper::~OomInterventionTabHelper() = default;

void OomInterventionTabHelper::OnHighMemoryUsage() {
  near_oom_detected_time_ = base::TimeTicks::Now();
  renderer_detection_timer_.Stop();
}

void OomInterventionTabHelper::WebContentsDestroyed() {
  StopMonitoring();
}

void OomInterventionTabHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  ResetInterfaces();

  // Skip background process termination.
  if (!IsLastVisibleWebContents(web_contents())) {
    ResetInterventionState();
    return;
  }

  if (near_oom_detected_time_) {
    ResetInterventionState();
  }
}

void OomInterventionTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Filter out sub-frame's navigation, non-primary page's navigation, or if the
  // navigation happens without changing document.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  last_navigation_timestamp_ = base::TimeTicks::Now();

  // Filter out the first navigation.
  if (!navigation_started_) {
    navigation_started_ = true;
    return;
  }

  ResetInterfaces();

  // Filter out background navigation.
  if (!IsLastVisibleWebContents(navigation_handle->GetWebContents())) {
    ResetInterventionState();
    return;
  }

  if (near_oom_detected_time_) {
    ResetInterventionState();
  }
}

void OomInterventionTabHelper::PrimaryPageChanged(content::Page& page) {
  if (!page.GetMainDocument().IsDocumentOnLoadCompletedInMainFrame()) {
    return;
  }
  if (IsLastVisibleWebContents(web_contents())) {
    StartMonitoringIfNeeded();
  }
}

void OomInterventionTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    StartMonitoringIfNeeded();
    SetLastVisibleWebContents(web_contents());
  } else {
    StopMonitoring();
  }
}

void OomInterventionTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (IsLastVisibleWebContents(web_contents())) {
    StartMonitoringIfNeeded();
  }
}

void OomInterventionTabHelper::StartMonitoringIfNeeded() {
  if (intervention_) {
    return;
  }

  if (near_oom_detected_time_) {
    return;
  }

  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    return;
  }

  auto* config = OomInterventionConfig::GetInstance();
  if (config->should_detect_in_renderer()) {
    if (receiver_.is_bound()) {
      return;
    }
    StartDetectionInRenderer();
  }
}

void OomInterventionTabHelper::StopMonitoring() {
  if (OomInterventionConfig::GetInstance()->should_detect_in_renderer()) {
    ResetInterfaces();
  }
}

void OomInterventionTabHelper::StartDetectionInRenderer() {
  auto* config = OomInterventionConfig::GetInstance();
  bool renderer_pause_enabled = config->is_renderer_pause_enabled();
  bool navigate_ads_enabled = config->is_navigate_ads_enabled();
  bool purge_v8_memory_enabled = config->is_purge_v8_memory_enabled();

  start_monitor_timestamp_ = base::TimeTicks::Now();

  content::RenderFrameHost& main_frame =
      web_contents()->GetPrimaryPage().GetMainDocument();

  content::RenderProcessHost* render_process_host = main_frame.GetProcess();
  CHECK(render_process_host);
  render_process_host->BindReceiver(intervention_.BindNewPipeAndPassReceiver());
  CHECK(!receiver_.is_bound());
  blink::mojom::DetectionArgsPtr detection_args =
      config->GetRendererOomDetectionArgs();
  intervention_->StartDetection(
      receiver_.BindNewPipeAndPassRemote(), std::move(detection_args),
      renderer_pause_enabled, navigate_ads_enabled, purge_v8_memory_enabled);
}

void OomInterventionTabHelper::OnNearOomDetected() {
  CHECK(!OomInterventionConfig::GetInstance()->should_detect_in_renderer());
  CHECK_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  CHECK(!near_oom_detected_time_);

  StartDetectionInRenderer();
  CHECK(!renderer_detection_timer_.IsRunning());
  base::TimeDelta detection_window = base::Seconds(60);
  if (base::FeatureList::IsEnabled(features::kOomIntervention)) {
    detection_window = base::Seconds(features::kDetectionWindowSeconds.Get());
  }
  renderer_detection_timer_.Start(
      FROM_HERE, detection_window,
      base::BindOnce(&OomInterventionTabHelper::
                         OnDetectionWindowElapsedWithoutHighMemoryUsage,
                     weak_factory_.GetWeakPtr()));
}

void OomInterventionTabHelper::
    OnDetectionWindowElapsedWithoutHighMemoryUsage() {
  ResetInterventionState();
  ResetInterfaces();
  StartMonitoringIfNeeded();
}

void OomInterventionTabHelper::ResetInterventionState() {
  near_oom_detected_time_.reset();
  renderer_detection_timer_.Stop();
}

void OomInterventionTabHelper::ResetInterfaces() {
  intervention_.reset();
  receiver_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OomInterventionTabHelper);
