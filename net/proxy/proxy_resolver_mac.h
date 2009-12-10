// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_MAC_H_
#define NET_PROXY_PROXY_RESOLVER_MAC_H_

#include <string>

#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver.h"

namespace net {

// Implementation of ProxyResolver that uses the Mac CFProxySupport to implement
// proxies.
class ProxyResolverMac : public ProxyResolver {
 public:
  ProxyResolverMac() : ProxyResolver(false /*expects_pac_bytes*/) {}

  // ProxyResolver methods:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             LoadLog* load_log);

  virtual void CancelRequest(RequestHandle request) {
    NOTREACHED();
  }

 private:
  virtual int SetPacScript(const GURL& pac_url,
                           const std::string& /*pac_bytes*/,
                           CompletionCallback* /*callback*/) {
    pac_url_ = pac_url;
    return OK;
  }

  GURL pac_url_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_MAC_H_
