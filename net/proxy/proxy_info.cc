// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
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
}

void ProxyInfo::UseDirect() {
  proxy_list_.SetSingleProxyServer(ProxyServer::Direct());
}

void ProxyInfo::UseNamedProxy(const std::string& proxy_uri_list) {
  proxy_list_.Set(proxy_uri_list);
}

void ProxyInfo::UseProxyServer(const ProxyServer& proxy_server) {
  proxy_list_.SetSingleProxyServer(proxy_server);
}

std::string ProxyInfo::ToPacString() const {
  return proxy_list_.ToPacString();
}

bool ProxyInfo::Fallback(ProxyRetryInfoMap* proxy_retry_info) {
  return proxy_list_.Fallback(proxy_retry_info);
}

void ProxyInfo::DeprioritizeBadProxies(
    const ProxyRetryInfoMap& proxy_retry_info) {
  proxy_list_.DeprioritizeBadProxies(proxy_retry_info);
}

void ProxyInfo::RemoveProxiesWithoutScheme(int scheme_bit_field) {
  proxy_list_.RemoveProxiesWithoutScheme(scheme_bit_field);
}

}  // namespace net
