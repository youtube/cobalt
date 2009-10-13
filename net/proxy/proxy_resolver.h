// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_H_
#define NET_PROXY_PROXY_RESOLVER_H_

#include <string>

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"

namespace net {

class LoadLog;
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
                             LoadLog* load_log) = 0;

  // Cancels |request|.
  virtual void CancelRequest(RequestHandle request) = 0;

  // The PAC script backend can be specified to the ProxyResolver either via
  // URL, or via the javascript text itself.  If |expects_pac_bytes| is true,
  // then PAC scripts should be specified using SetPacScriptByData(). Otherwise
  // they should be specified using SetPacScriptByUrl().
  bool expects_pac_bytes() const { return expects_pac_bytes_; }

  // Sets the PAC script backend to use for this proxy resolver (by URL).
  int SetPacScriptByUrl(const GURL& url, CompletionCallback* callback) {
    DCHECK(!expects_pac_bytes());
    return SetPacScript(url, std::string(), callback);
  }

  // Sets the PAC script backend to use for this proxy resolver (by contents).
  int SetPacScriptByData(const std::string& bytes_utf8,
                         CompletionCallback* callback) {
    DCHECK(expects_pac_bytes());
    return SetPacScript(GURL(), bytes_utf8, callback);
  }

  // TODO(eroman): Make this =0.
  virtual void CancelSetPacScript() {
    NOTREACHED();
  }

  // Frees any unneeded memory held by the resolver, e.g. garbage in the JS
  // engine.  Most subclasses don't need to do anything, so we provide a default
  // no-op implementation.
  virtual void PurgeMemory() {}

 private:
  // Called to set the PAC script backend to use. If |pac_url| is invalid,
  // this is a request to use WPAD (auto detect). |bytes_utf8| may be empty if
  // the fetch failed, or if the fetch returned no content.
  // Returns ERR_IO_PENDING in the case of asynchronous completion, and notifies
  // the result through |callback|.
  virtual int SetPacScript(const GURL& pac_url,
                           const std::string& bytes_utf8,
                           CompletionCallback* callback) = 0;

  const bool expects_pac_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolver);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_H_
