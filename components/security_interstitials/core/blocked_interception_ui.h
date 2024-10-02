// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_BLOCKED_INTERCEPTION_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_BLOCKED_INTERCEPTION_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/ssl_errors/error_classification.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace security_interstitials {

// Provides UI for SSL errors caused by blocked interceptions.
class BlockedInterceptionUI {
 public:
  BlockedInterceptionUI(const GURL& request_url,
                        int cert_error,
                        const net::SSLInfo& ssl_info,
                        ControllerClient* controller_);

  BlockedInterceptionUI(const BlockedInterceptionUI&) = delete;
  BlockedInterceptionUI& operator=(const BlockedInterceptionUI&) = delete;

  ~BlockedInterceptionUI();

  void PopulateStringsForHTML(base::Value::Dict& load_time_data);
  void HandleCommand(SecurityInterstitialCommand command);

 private:
  const GURL request_url_;
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  raw_ptr<ControllerClient> controller_;
  bool user_made_decision_;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_BLOCKED_INTERCEPTION_UI_H_
