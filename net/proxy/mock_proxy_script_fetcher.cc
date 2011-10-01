// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mock_proxy_script_fetcher.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"

namespace net {

MockProxyScriptFetcher::MockProxyScriptFetcher()
    : pending_request_callback_(NULL), pending_request_text_(NULL) {
}

// ProxyScriptFetcher implementation.
int MockProxyScriptFetcher::Fetch(const GURL& url,
                                  string16* text,
                                  OldCompletionCallback* callback) {
  DCHECK(!has_pending_request());

  // Save the caller's information, and have them wait.
  pending_request_url_ = url;
  pending_request_callback_ = callback;
  pending_request_text_ = text;
  return ERR_IO_PENDING;
}

void MockProxyScriptFetcher::NotifyFetchCompletion(
    int result, const std::string& ascii_text) {
  DCHECK(has_pending_request());
  *pending_request_text_ = ASCIIToUTF16(ascii_text);
  OldCompletionCallback* callback = pending_request_callback_;
  pending_request_callback_ = NULL;
  callback->Run(result);
}

void MockProxyScriptFetcher::Cancel() {
}

URLRequestContext* MockProxyScriptFetcher::GetRequestContext() const {
  return NULL;
}

const GURL& MockProxyScriptFetcher::pending_request_url() const {
  return pending_request_url_;
}

bool MockProxyScriptFetcher::has_pending_request() const {
  return pending_request_callback_ != NULL;
}

}  // namespace net
