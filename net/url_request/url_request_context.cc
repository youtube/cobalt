// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context.h"

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/string_util.h"
#include "net/base/host_resolver.h"
#include "net/cookies/cookie_store.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"

namespace net {

URLRequestContext::URLRequestContext()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      net_log_(NULL),
      host_resolver_(NULL),
      cert_verifier_(NULL),
      server_bound_cert_service_(NULL),
      fraudulent_certificate_reporter_(NULL),
      http_auth_handler_factory_(NULL),
      proxy_service_(NULL),
      network_delegate_(NULL),
      http_server_properties_(NULL),
      transport_security_state_(NULL),
      ftp_auth_cache_(new FtpAuthCache),
      http_transaction_factory_(NULL),
      ftp_transaction_factory_(NULL),
      job_factory_(NULL),
      throttler_manager_(NULL),
      url_requests_(new std::set<const URLRequest*>) {
}

void URLRequestContext::CopyFrom(URLRequestContext* other) {
  // Copy URLRequestContext parameters.
  set_net_log(other->net_log());
  set_host_resolver(other->host_resolver());
  set_cert_verifier(other->cert_verifier());
  set_server_bound_cert_service(other->server_bound_cert_service());
  set_fraudulent_certificate_reporter(other->fraudulent_certificate_reporter());
  set_http_auth_handler_factory(other->http_auth_handler_factory());
  set_proxy_service(other->proxy_service());
  set_ssl_config_service(other->ssl_config_service());
  set_network_delegate(other->network_delegate());
  set_http_server_properties(other->http_server_properties());
  set_cookie_store(other->cookie_store());
  set_transport_security_state(other->transport_security_state());
  // FTPAuthCache is unique per context.
  set_accept_language(other->accept_language());
  set_accept_charset(other->accept_charset());
  set_referrer_charset(other->referrer_charset());
  set_http_transaction_factory(other->http_transaction_factory());
  set_ftp_transaction_factory(other->ftp_transaction_factory());
  set_job_factory(other->job_factory());
  set_throttler_manager(other->throttler_manager());
}

void URLRequestContext::set_cookie_store(CookieStore* cookie_store) {
  cookie_store_ = cookie_store;
}

const std::string& URLRequestContext::GetUserAgent(const GURL& url) const {
  return EmptyString();
}

void URLRequestContext::AssertNoURLRequests() const {
  int num_requests = url_requests_->size();
  if (num_requests != 0) {
    // We're leaking URLRequests :( Dump the URL of the first one and record how
    // many we leaked so we have an idea of how bad it is.
    char url_buf[128];
    const URLRequest* request = *url_requests_->begin();
    base::strlcpy(url_buf, request->url().spec().c_str(), arraysize(url_buf));
    bool has_delegate = request->has_delegate();
    int load_flags = request->load_flags();
    base::debug::Alias(url_buf);
    base::debug::Alias(&num_requests);
    base::debug::Alias(&has_delegate);
    base::debug::Alias(&load_flags);
    CHECK(false);
  }
}

URLRequestContext::~URLRequestContext() {
  AssertNoURLRequests();
}

}  // namespace net
