// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_MAC_H_
#define NET_PROXY_PROXY_RESOLVER_MAC_H_
#pragma once

#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver.h"

namespace net {

// Implementation of ProxyResolver that uses the Mac CFProxySupport to implement
// proxies.
class ProxyResolverMac : public ProxyResolver {
 public:
  ProxyResolverMac();
  virtual ~ProxyResolverMac();

  // ProxyResolver methods:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log);

  virtual void CancelRequest(RequestHandle request);

  virtual void CancelSetPacScript();

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      CompletionCallback* /*callback*/);

 private:
  scoped_refptr<ProxyResolverScriptData> script_data_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_MAC_H_
