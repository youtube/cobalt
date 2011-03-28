// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_H_
#define NET_PROXY_PROXY_RESOLVER_H_
#pragma once

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_resolver_script_data.h"

namespace net {

class BoundNetLog;
class ProxyInfo;

// Interface for "proxy resolvers". A ProxyResolver fills in a list of proxies
// to use for a particular URL. Generally the backend for a ProxyResolver is
// a PAC script, but it doesn't need to be. ProxyResolver can service multiple
// requests at a time.
class ProxyResolver {
 public:
  // Opaque pointer type, to return a handle to cancel outstanding requests.
  typedef void* RequestHandle;

  // See |expects_pac_bytes()| for the meaning of |expects_pac_bytes|.
  explicit ProxyResolver(bool expects_pac_bytes)
      : expects_pac_bytes_(expects_pac_bytes) {}

  virtual ~ProxyResolver() {}

  // Gets a list of proxy servers to use for |url|. If the request will
  // complete asynchronously returns ERR_IO_PENDING and notifies the result
  // by running |callback|.  If the result code is OK then
  // the request was successful and |results| contains the proxy
  // resolution information.  In the case of asynchronous completion
  // |*request| is written to, and can be passed to CancelRequest().
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) = 0;

  // Cancels |request|.
  virtual void CancelRequest(RequestHandle request) = 0;

  // The PAC script backend can be specified to the ProxyResolver either via
  // URL, or via the javascript text itself.  If |expects_pac_bytes| is true,
  // then the ProxyResolverScriptData passed to SetPacScript() should
  // contain the actual script bytes rather than just the URL.
  bool expects_pac_bytes() const { return expects_pac_bytes_; }

  virtual void CancelSetPacScript() = 0;

  // Frees any unneeded memory held by the resolver, e.g. garbage in the JS
  // engine.  Most subclasses don't need to do anything, so we provide a default
  // no-op implementation.
  virtual void PurgeMemory() {}

  // Called to set the PAC script backend to use.
  // Returns ERR_IO_PENDING in the case of asynchronous completion, and notifies
  // the result through |callback|.
  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      CompletionCallback* callback) = 0;

  // Optional shutdown code to be run before destruction. This is only used
  // by the multithreaded runner to signal cleanup from origin thread
  virtual void Shutdown() {}

 private:
  const bool expects_pac_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolver);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_H_
