// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_STUB_H_

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "content/public/browser/global_request_id.h"
#include "net/base/auth.h"

namespace content {

class DevToolsURLLoaderInterceptor {
 public:
  static void HandleAuthRequest(GlobalRequestID req_id,
                                const net::AuthChallengeInfo& auth_info,
                                base::OnceCallback<void(bool,
                                const std::optional<net::AuthCredentials>&)>
                                callback) {
    std::move(callback).Run(true, std::nullopt);
  }
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_STUB_H_
