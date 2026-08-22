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

#ifndef CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_URL_LOADER_INTERCEPTOR_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_URL_LOADER_INTERCEPTOR_STUB_H_

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "content/public/browser/global_request_id.h"
#include "net/base/auth.h"

namespace content {

class DevToolsURLLoaderInterceptor {
 public:
  static void HandleAuthRequest(
      GlobalRequestID req_id,
      const net::AuthChallengeInfo& auth_info,
      base::OnceCallback<void(bool, const std::optional<net::AuthCredentials>&)>
          callback) {
    std::move(callback).Run(true, std::nullopt);
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_URL_LOADER_INTERCEPTOR_STUB_H_
