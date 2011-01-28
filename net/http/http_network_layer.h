// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_LAYER_H_
#define NET_HTTP_HTTP_NETWORK_LAYER_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class CertVerifier;
class ClientSocketFactory;
class DnsCertProvenanceChecker;
class DnsRRResolver;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkDelegate;
class HttpNetworkSession;
class NetLog;
class ProxyService;
class SpdySessionPool;
class SSLConfigService;
class SSLHostInfoFactory;

class HttpNetworkLayer : public HttpTransactionFactory,
                         public base::NonThreadSafe {
 public:
  // Construct a HttpNetworkLayer with an existing HttpNetworkSession which
  // contains a valid ProxyService.
  explicit HttpNetworkLayer(HttpNetworkSession* session);
  virtual ~HttpNetworkLayer();

  // Create a transaction factory that instantiate a network layer over an
  // existing network session. Network session contains some valuable
  // information (e.g. authentication data) that we want to share across
  // multiple network layers. This method exposes the implementation details
  // of a network layer, use this method with an existing network layer only
  // when network session is shared.
  static HttpTransactionFactory* CreateFactory(HttpNetworkSession* session);

  // Enable the spdy protocol.
  // Without calling this function, SPDY is disabled.  The mode can be:
  //   ""            : (default) SSL and compression are enabled, flow
  //                   control disabled.
  //   "no-ssl"      : disables SSL.
  //   "no-compress" : disables compression.
  //   "flow-control": enables flow control.
  //   "none"        : disables both SSL and compression.
  static void EnableSpdy(const std::string& mode);

  // HttpTransactionFactory methods:
  virtual int CreateTransaction(scoped_ptr<HttpTransaction>* trans);
  virtual HttpCache* GetCache();
  virtual HttpNetworkSession* GetSession();
  virtual void Suspend(bool suspend);

 private:
  const scoped_refptr<HttpNetworkSession> session_;
  bool suspended_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_LAYER_H_
