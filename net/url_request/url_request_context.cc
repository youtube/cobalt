// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context.h"

#include "base/string_util.h"
#include "net/base/cookie_store.h"
#include "net/base/host_resolver.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/http/http_transaction_factory.h"

namespace net {

URLRequestContext::URLRequestContext()
    : is_main_(false),
      net_log_(NULL),
      host_resolver_(NULL),
      cert_verifier_(NULL),
      dnsrr_resolver_(NULL),
      dns_cert_checker_(NULL),
      http_auth_handler_factory_(NULL),
      network_delegate_(NULL),
      cookie_policy_(NULL),
      transport_security_state_(NULL),
      http_transaction_factory_(NULL),
      ftp_transaction_factory_(NULL) {
}

void URLRequestContext::set_cookie_store(CookieStore* cookie_store) {
  cookie_store_ = cookie_store;
}

const std::string& URLRequestContext::GetUserAgent(const GURL& url) const {
  return EmptyString();
}

URLRequestContext::~URLRequestContext() {
}

}  // namespace net
