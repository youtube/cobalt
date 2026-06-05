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

#include "cobalt/browser/cobalt_web_contents_observer.h"

#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"
#include "cobalt/browser/lifecycle/public/mojom/cobalt_lifecycle.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if BUILDFLAG(IS_STARBOARD)
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "starboard/system.h"
#endif  // BUILDFLAG(IS_STARBOARD)

namespace cobalt {

#if BUILDFLAG(IS_STARBOARD)
namespace {
constexpr int kNavigationTimeoutSeconds = 30;
}  // namespace

class CobaltWebContentsObserver::PlatformErrorBridge {
 public:
  PlatformErrorBridge(base::WeakPtr<CobaltWebContentsObserver> observer,
                      int64_t navigation_id)
      : observer_(observer),
        navigation_id_(navigation_id),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
  ~PlatformErrorBridge() = default;

  void Run(SbSystemPlatformErrorResponse response) {
    if (task_runner_->RunsTasksInCurrentSequence()) {
      RunOnSequence(response);
    } else {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&PlatformErrorBridge::RunOnSequence,
                                            base::Unretained(this), response));
    }
  }

  void Invalidate() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    observer_.reset();
  }

 private:
  void RunOnSequence(SbSystemPlatformErrorResponse response) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (observer_) {
      observer_->OnPlatformErrorResponse(response, navigation_id_);
    }
    delete this;
  }

  base::WeakPtr<CobaltWebContentsObserver> observer_;
  int64_t navigation_id_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};
#endif  // BUILDFLAG(IS_STARBOARD)

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // Check if main frame is already created.
  if (web_contents) {
    content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    if (main_frame && main_frame->IsRenderFrameLive()) {
      mojo::Remote<cobalt::mojom::CobaltLifecycleController> controller;
      main_frame->GetRemoteInterfaces()->GetInterface(
          controller.BindNewPipeAndPassReceiver());

      // Create observer and pass to renderer.
      mojo::PendingRemote<cobalt::mojom::CobaltLifecycleObserver>
          observer_remote;
      CobaltLifecycleManager::GetInstance()->BindReceiver(
          main_frame, observer_remote.InitWithNewPipeAndPassReceiver());
      controller->SetObserver(std::move(observer_remote));

      controllers_[main_frame] = std::move(controller);
    }
  }
}

CobaltWebContentsObserver::~CobaltWebContentsObserver() {
#if BUILDFLAG(IS_STARBOARD)
  if (pending_platform_error_bridge_) {
    pending_platform_error_bridge_->Invalidate();
  }
#endif
}

#if BUILDFLAG(IS_STARBOARD)
void CobaltWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* handle) {
  LOG(INFO) << "DidStartNavigation to: " << handle->GetURL();
  if (!handle->IsInPrimaryMainFrame()) {
    LOG(INFO) << "DidStartNavigation: navigation to " << handle->GetURL()
              << " not in primary mainframe, returning";
    return;
  }
  LOG(INFO) << "DidStartNavigation: navigation to " << handle->GetURL()
            << " in primary mainframe";

  latest_navigation_id_ = handle->GetNavigationId();

  // Start a navigation timer with a timeout callback to raise a
  // network error dialog
  timeout_timer_.Stop();
  timeout_timer_.Start(
      FROM_HERE, base::Seconds(kNavigationTimeoutSeconds),
      base::BindOnce(&CobaltWebContentsObserver::RaisePlatformError,
                     weak_factory_.GetWeakPtr(), latest_navigation_id_));
}

void CobaltWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  timeout_timer_.Stop();
  const auto net_error_code = navigation_handle->GetNetErrorCode();
  if (net_error_code != net::OK && net_error_code != net::ERR_ABORTED) {
    LOG(INFO) << "DidFinishNavigation: Raising platform error with code: "
              << net::ErrorToString(net_error_code);
    RaisePlatformError(navigation_handle->GetNavigationId());
  } else if (net_error_code == net::OK) {
    platform_error_raised_count_ = 0;
  }
}

void CobaltWebContentsObserver::RaisePlatformError(int64_t navigation_id) {
  if (is_platform_error_showing_) {
    return;
  }
  if (navigation_id != latest_navigation_id_) {
    LOG(INFO) << "Ignoring stale platform error request for navigation "
              << navigation_id;
    return;
  }
  is_platform_error_showing_ = true;
  platform_error_raised_count_++;
  base::UmaHistogramCounts100("Cobalt.Network.PlatformErrorCount",
                              platform_error_raised_count_);
  auto* bridge =
      new PlatformErrorBridge(weak_factory_.GetWeakPtr(), navigation_id);
  pending_platform_error_bridge_ = bridge;
  if (!SbSystemRaisePlatformError(
          kSbSystemPlatformErrorTypeConnectionError,
          &CobaltWebContentsObserver::HandlePlatformErrorResponse, bridge)) {
    LOG(WARNING) << "Did not handle platform error";
    is_platform_error_showing_ = false;
    if (pending_platform_error_bridge_ == bridge) {
      delete bridge;
      pending_platform_error_bridge_ = nullptr;
    }
  }
}

// static
void CobaltWebContentsObserver::HandlePlatformErrorResponse(
    SbSystemPlatformErrorResponse response,
    void* user_data) {
  auto* bridge = static_cast<PlatformErrorBridge*>(user_data);
  bridge->Run(response);
}

void CobaltWebContentsObserver::OnPlatformErrorResponse(
    SbSystemPlatformErrorResponse response,
    int64_t navigation_id) {
  pending_platform_error_bridge_ = nullptr;
  is_platform_error_showing_ = false;
  if (navigation_id != latest_navigation_id_) {
    LOG(INFO) << "Ignoring stale platform error response for navigation "
              << navigation_id;
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (response == kSbSystemPlatformErrorResponsePositive) {
    LOG(INFO) << "Platform error response is POSITIVE. Reloading...";
    if (web_contents()) {
      web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                             /*check_for_repost=*/false);
    }
  } else {
    LOG(INFO) << "Platform error response is NEGATIVE/CANCEL. Stopping app...";
    SbSystemRequestStop(0);
  }
#endif
}
#endif  // BUILDFLAG(IS_STARBOARD)

}  // namespace cobalt
