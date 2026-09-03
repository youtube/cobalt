#include "base/strings/string_util.h"
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

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/browser/resource_scheduler/cobalt_adaptive_resource_scheduler.h"
#include "cobalt/browser/resource_scheduler/cobalt_resource_throttle.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/resource_request.h"

namespace cobalt {

// static
bool CobaltResourceThrottle::IsRequestDeferrable(
    const network::ResourceRequest& request) {
  // 1. Critical documents, scripts, styles, and web workers: NEVER defer.
  if (request.destination == network::mojom::RequestDestination::kDocument ||
      request.destination == network::mojom::RequestDestination::kScript ||
      request.destination == network::mojom::RequestDestination::kStyle ||
      request.destination == network::mojom::RequestDestination::kWorker ||
      request.destination ==
          network::mojom::RequestDestination::kSharedWorker ||
      request.destination ==
          network::mojom::RequestDestination::kServiceWorker) {
    return false;
  }

  // 2. Blob, data, and embedded splash schemes: NEVER defer.
  if (request.url.SchemeIs("blob") || request.url.SchemeIs("data") ||
      request.url.SchemeIs("h5vcc-embedded")) {
    return false;
  }

  // 3. Media playback chunks: NEVER defer.
  if (request.url.path().find("videoplayback") != std::string::npos) {
    return false;
  }

  // 4. Background analytics, logging, and telemetry pings: ALWAYS DEFER.
  const std::string& url_spec = request.url.spec();
  if (url_spec.find("/log_event") != std::string::npos ||
      url_spec.find("/api/stats") != std::string::npos ||
      url_spec.find("/stats/") != std::string::npos ||
      url_spec.find("/ptracking") != std::string::npos ||
      url_spec.find("google-analytics.com") != std::string::npos) {
    return true;
  }

  // 5. High priority requests (Hero in-viewport image, critical API calls):
  // NEVER defer.
  if (request.priority >= net::RequestPriority::MEDIUM) {
    return false;
  }

  // 6. Low-priority image thumbnails (offscreen shelf cards, carousels): DEFER.
  const std::string& path_str = request.url.path();
  if (request.destination == network::mojom::RequestDestination::kImage ||
      request.url.DomainIs("i.ytimg.com") ||
      request.url.DomainIs("yt3.ggpht.com") ||
      request.url.DomainIs("ggpht.com") ||
      base::EndsWith(path_str, ".jpg", base::CompareCase::INSENSITIVE_ASCII) ||
      base::EndsWith(path_str, ".jpeg", base::CompareCase::INSENSITIVE_ASCII) ||
      base::EndsWith(path_str, ".png", base::CompareCase::INSENSITIVE_ASCII) ||
      base::EndsWith(path_str, ".webp", base::CompareCase::INSENSITIVE_ASCII) ||
      base::EndsWith(path_str, ".gif", base::CompareCase::INSENSITIVE_ASCII) ||
      base::EndsWith(path_str, ".svg", base::CompareCase::INSENSITIVE_ASCII)) {
    return true;
  }

  return false;
}

// static
std::unique_ptr<CobaltResourceThrottle> CobaltResourceThrottle::MaybeCreate(
    const network::ResourceRequest& request) {
  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  if (!scheduler || !scheduler->IsEnabled()) {
    return nullptr;
  }

  bool is_deferrable = IsRequestDeferrable(request);
  LOG(INFO) << "CobaltResourceThrottle::MaybeCreate: url=" << request.url.spec()
            << ", dest=" << static_cast<int>(request.destination)
            << ", prio=" << static_cast<int>(request.priority)
            << ", deferrable=" << is_deferrable
            << ", should_defer=" << scheduler->ShouldDeferRequests();
  return std::make_unique<CobaltResourceThrottle>(is_deferrable);
}

CobaltResourceThrottle::CobaltResourceThrottle(bool is_deferrable)
    : is_deferrable_(is_deferrable) {
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }
}

CobaltResourceThrottle::~CobaltResourceThrottle() {
  if (is_deferred_) {
    auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
    if (scheduler) {
      scheduler->UnregisterDeferredThrottle(this);
    }
  }
}

void CobaltResourceThrottle::WillStartRequest(network::ResourceRequest* request,
                                              bool* defer) {
  LOG(INFO) << "CobaltResourceThrottle::WillStartRequest: url="
            << request->url.spec() << ", is_deferrable_=" << is_deferrable_;
  if (!is_deferrable_) {
    return;
  }

  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  if (scheduler && scheduler->ShouldDeferRequests()) {
    *defer = true;
    is_deferred_ = true;
    LOG(INFO) << "CobaltResourceThrottle: Deferring low-priority request: "
              << request->url.spec();
    scheduler->RegisterDeferredThrottle(this);
  }
}

void CobaltResourceThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  if (!is_deferrable_) {
    return;
  }

  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  if (scheduler && scheduler->ShouldDeferRequests()) {
    *defer = true;
    is_deferred_ = true;
    LOG(INFO) << "CobaltResourceThrottle: Deferring redirect: "
              << redirect_info->new_url.spec();
    scheduler->RegisterDeferredThrottle(this);
  }
}

void CobaltResourceThrottle::ResumeLoading() {
  // If called from another thread/sequence (e.g. IO thread draining renderer
  // throttles), post safely back to the original sequence owning this throttle
  // and loader.
  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CobaltResourceThrottle::ResumeLoading,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (is_deferred_) {
    is_deferred_ = false;
    LOG(INFO)
        << "CobaltResourceThrottle: Resuming deferred request on sequence.";
    if (delegate_) {
      delegate_->Resume();
    }
  }
}

}  // namespace cobalt
