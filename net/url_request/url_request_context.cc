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

void URLRequestContext::CopyFrom(URLRequestContext* other) {
  // Copy URLRequestContext parameters.
  // Do not copy is_main_.
  set_net_log(other->net_log());
  set_host_resolver(other->host_resolver());
  set_cert_verifier(other->cert_verifier());
  set_dnsrr_resolver(other->dnsrr_resolver());
  set_dns_cert_checker(other->dns_cert_checker());
  set_http_auth_handler_factory(other->http_auth_handler_factory());
  set_proxy_service(other->proxy_service());
  set_ssl_config_service(other->ssl_config_service());
  set_network_delegate(other->network_delegate());
  set_cookie_store(other->cookie_store());
  set_cookie_policy(other->cookie_policy());
  set_transport_security_state(other->transport_security_state());
  // FTPAuthCache is unique per context.
  set_accept_language(other->accept_language());
  set_accept_charset(other->accept_charset());
  set_referrer_charset(other->referrer_charset());
  set_http_transaction_factory(other->http_transaction_factory());
  set_ftp_transaction_factory(other->ftp_transaction_factory());
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
