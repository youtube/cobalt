// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_WINHTTP_H_
#define NET_PROXY_PROXY_RESOLVER_WINHTTP_H_
#pragma once

#include "googleurl/src/gurl.h"
#include "net/proxy/proxy_resolver.h"

typedef void* HINTERNET;  // From winhttp.h

namespace net {

// An implementation of ProxyResolver that uses WinHTTP and the system
// proxy settings.
class ProxyResolverWinHttp : public ProxyResolver {
 public:
  ProxyResolverWinHttp();
  virtual ~ProxyResolverWinHttp();

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* /*callback*/,
                             RequestHandle* /*request*/,
                             const BoundNetLog& /*net_log*/);
  virtual void CancelRequest(RequestHandle request);

  virtual void CancelSetPacScript();

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      CompletionCallback* /*callback*/);

 private:
  bool OpenWinHttpSession();
  void CloseWinHttpSession();

  // Proxy configuration is cached on the session handle.
  HINTERNET session_handle_;

  GURL pac_url_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverWinHttp);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_WINHTTP_H_
