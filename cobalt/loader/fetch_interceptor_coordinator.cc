// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/fetch_interceptor_coordinator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "url/gurl.h"

namespace cobalt {
namespace loader {

// static
FetchInterceptorCoordinator* FetchInterceptorCoordinator::GetInstance() {
  return base::Singleton<
      FetchInterceptorCoordinator,
      base::LeakySingletonTraits<FetchInterceptorCoordinator>>::get();
}

FetchInterceptorCoordinator::FetchInterceptorCoordinator()
    : fetch_interceptor_(nullptr) {}


void FetchInterceptorCoordinator::TryIntercept(
    const GURL& url,
    base::OnceCallback<void(std::unique_ptr<std::string>)> callback,
    base::OnceCallback<void(const net::LoadTimingInfo&)>
        report_load_timing_info,
    base::OnceClosure fallback) {
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#handle-fetch

  if (!fetch_interceptor_) {
    std::move(fallback).Run();
    return;
  }
  // Steps 17 to 23
  // TODO: add should skip event handling
  // (https://www.w3.org/TR/2022/CRD-service-workers-20220712/#should-skip-event).

  // 18. Let shouldSoftUpdate be true if any of the following are true, and
  //     false otherwise:
  //      . request is a non-subresource request.
  // Note: check if destination is: "document", "embed", "frame", "iframe",
  // "object", "report", "serviceworker", "sharedworker", or "worker".
  //
  //      . request is a subresource request and registration is stale.
  // Note: check if destination is "audio", "audioworklet", "font", "image",
  // "manifest", "paintworklet", "script", "style", "track", "video", "xslt"

  // TODO: test interception once registered service workers are persisted.
  //       Consider moving the interception out of the ServiceWorkerGlobalScope
  //       to avoid a race condition. Fetches should be able to be intercepted
  //       by an active, registered, service worker.
  fetch_interceptor_->StartFetch(url, std::move(callback),
                                 std::move(report_load_timing_info),
                                 std::move(fallback));
}

}  // namespace loader
}  // namespace cobalt
