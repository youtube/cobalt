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

#include "cobalt/browser/resource_scheduler/cobalt_resource_throttle.h"

#include "base/logging.h"
#include "cobalt/browser/resource_scheduler/cobalt_adaptive_resource_scheduler.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/resource_request.h"

namespace cobalt {

namespace {

// Determines if a request can be safely deferred during active scrolling.
// Critical resources (Main HTML, CSS, Scripts, and High Priority requests)
// are NEVER deferred.
bool IsRequestDeferrable(const network::ResourceRequest& request) {
  // Never defer scripts, stylesheets, or main documents.
  if (request.destination == network::mojom::RequestDestination::kScript ||
      request.destination == network::mojom::RequestDestination::kStyle ||
      request.destination == network::mojom::RequestDestination::kDocument) {
    return false;
  }

  // Never defer high-priority or media chunk requests.
  if (request.priority >= net::RequestPriority::MEDIUM) {
    return false;
  }

  // Images, fonts, and low-priority fetches are candidates for deferral.
  return true;
}

}  // namespace

// static
std::unique_ptr<CobaltResourceThrottle> CobaltResourceThrottle::MaybeCreate(
    const network::ResourceRequest& request) {
  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  if (!scheduler || !scheduler->IsEnabled()) {
    return nullptr;
  }

  bool is_deferrable = IsRequestDeferrable(request);
  return std::make_unique<CobaltResourceThrottle>(is_deferrable);
}

CobaltResourceThrottle::CobaltResourceThrottle(bool is_deferrable)
    : is_deferrable_(is_deferrable) {}

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
  if (!is_deferrable_) {
    return;
  }

  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  if (scheduler && scheduler->IsScrolling()) {
    *defer = true;
    is_deferred_ = true;
    LOG(INFO) << "CobaltResourceThrottle: Deferring low-priority request "
                 "during scroll: "
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
  if (scheduler && scheduler->IsScrolling()) {
    *defer = true;
    is_deferred_ = true;
    LOG(INFO) << "CobaltResourceThrottle: Deferring redirect during scroll: "
              << redirect_info->new_url.spec();
    scheduler->RegisterDeferredThrottle(this);
  }
}

void CobaltResourceThrottle::ResumeLoading() {
  if (is_deferred_) {
    is_deferred_ = false;
    LOG(INFO) << "CobaltResourceThrottle: Resuming deferred request.";
    if (delegate_) {
      delegate_->Resume();
    }
  }
}

}  // namespace cobalt
