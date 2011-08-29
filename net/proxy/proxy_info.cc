// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_info.h"

#include "net/proxy/proxy_retry_info.h"

namespace net {

ProxyInfo::ProxyInfo() : config_id_(ProxyConfig::INVALID_ID) {
}

ProxyInfo::~ProxyInfo() {
}

void ProxyInfo::Use(const ProxyInfo& other) {
  proxy_list_ = other.proxy_list_;
  proxy_retry_info_ = other.proxy_retry_info_;
}

void ProxyInfo::UseDirect() {
  proxy_list_.SetSingleProxyServer(ProxyServer::Direct());
  proxy_retry_info_.clear();
}

void ProxyInfo::UseNamedProxy(const std::string& proxy_uri_list) {
  proxy_list_.Set(proxy_uri_list);
  proxy_retry_info_.clear();
}

void ProxyInfo::UseProxyServer(const ProxyServer& proxy_server) {
  proxy_list_.SetSingleProxyServer(proxy_server);
  proxy_retry_info_.clear();
}

std::string ProxyInfo::ToPacString() const {
  return proxy_list_.ToPacString();
}

bool ProxyInfo::Fallback(const BoundNetLog& net_log) {
  return proxy_list_.Fallback(&proxy_retry_info_, net_log);
}

void ProxyInfo::DeprioritizeBadProxies(
    const ProxyRetryInfoMap& proxy_retry_info) {
  proxy_list_.DeprioritizeBadProxies(proxy_retry_info);
}

void ProxyInfo::RemoveProxiesWithoutScheme(int scheme_bit_field) {
  proxy_list_.RemoveProxiesWithoutScheme(scheme_bit_field);
}

}  // namespace net
