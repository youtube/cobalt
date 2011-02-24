// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_storage.h"

#include "base/logging.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_policy.h"
#include "net/base/cookie_store.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_resolver.h"
#include "net/base/net_log.h"
#include "net/base/network_delegate.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

namespace net {

URLRequestContextStorage::URLRequestContextStorage(URLRequestContext* context)
    : context_(context) {
  DCHECK(context);
}

URLRequestContextStorage::~URLRequestContextStorage() {}

void URLRequestContextStorage::set_net_log(NetLog* net_log) {
  context_->set_net_log(net_log);
  net_log_.reset(net_log);
}

void URLRequestContextStorage::set_host_resolver(HostResolver* host_resolver) {
  context_->set_host_resolver(host_resolver);
  host_resolver_.reset(host_resolver);
}

void URLRequestContextStorage::set_cert_verifier(CertVerifier* cert_verifier) {
  context_->set_cert_verifier(cert_verifier);
  cert_verifier_.reset(cert_verifier);
}

void URLRequestContextStorage::set_dnsrr_resolver(
    DnsRRResolver* dnsrr_resolver) {
  context_->set_dnsrr_resolver(dnsrr_resolver);
  dnsrr_resolver_.reset(dnsrr_resolver);
}

void URLRequestContextStorage::set_dns_cert_checker(
    DnsCertProvenanceChecker* dns_cert_checker) {
  context_->set_dns_cert_checker(dns_cert_checker);
  dns_cert_checker_.reset(dns_cert_checker);
}

void URLRequestContextStorage::set_http_auth_handler_factory(
    HttpAuthHandlerFactory* http_auth_handler_factory) {
  context_->set_http_auth_handler_factory(http_auth_handler_factory);
  http_auth_handler_factory_.reset(http_auth_handler_factory);
}

void URLRequestContextStorage::set_proxy_service(ProxyService* proxy_service) {
  context_->set_proxy_service(proxy_service);
  proxy_service_ = proxy_service;
}

void URLRequestContextStorage::set_ssl_config_service(
    SSLConfigService* ssl_config_service) {
  context_->set_ssl_config_service(ssl_config_service);
  ssl_config_service_ = ssl_config_service;
}

void URLRequestContextStorage::set_network_delegate(
    NetworkDelegate* network_delegate) {
  context_->set_network_delegate(network_delegate);
  network_delegate_.reset(network_delegate);
}

void URLRequestContextStorage::set_cookie_store(CookieStore* cookie_store) {
  context_->set_cookie_store(cookie_store);
  cookie_store_ = cookie_store;
}

void URLRequestContextStorage::set_cookie_policy(CookiePolicy* cookie_policy) {
  context_->set_cookie_policy(cookie_policy);
  cookie_policy_.reset(cookie_policy);
}

void URLRequestContextStorage::set_transport_security_state(
    TransportSecurityState* transport_security_state) {
  context_->set_transport_security_state(transport_security_state);
  transport_security_state_ = transport_security_state;
}

void URLRequestContextStorage::set_http_transaction_factory(
    HttpTransactionFactory* http_transaction_factory) {
  context_->set_http_transaction_factory(http_transaction_factory);
  http_transaction_factory_.reset(http_transaction_factory);
}

void URLRequestContextStorage::set_ftp_transaction_factory(
    FtpTransactionFactory* ftp_transaction_factory) {
  context_->set_ftp_transaction_factory(ftp_transaction_factory);
  ftp_transaction_factory_.reset(ftp_transaction_factory);
}

}  // namespace net
