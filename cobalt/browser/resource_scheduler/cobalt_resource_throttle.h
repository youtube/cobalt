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

#ifndef COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_RESOURCE_THROTTLE_H_
#define COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_RESOURCE_THROTTLE_H_

#include <memory>

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace cobalt {

// A URLLoaderThrottle that inspects incoming resource requests and defers
// non-critical requests (such as off-screen images and background telemetry)
// during active spatial navigation / scrolling.
class CobaltResourceThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<CobaltResourceThrottle> MaybeCreate(
      const network::ResourceRequest& request);

  explicit CobaltResourceThrottle(bool is_deferrable);
  ~CobaltResourceThrottle() override;

  CobaltResourceThrottle(const CobaltResourceThrottle&) = delete;
  CobaltResourceThrottle& operator=(const CobaltResourceThrottle&) = delete;

  // blink::URLLoaderThrottle implementation:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;

  // Called by CobaltAdaptiveResourceScheduler when the scroll state settles.
  void ResumeLoading();

  bool is_deferred() const { return is_deferred_; }

 private:
  const bool is_deferrable_;
  bool is_deferred_ = false;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_RESOURCE_THROTTLE_H_
