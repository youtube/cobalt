// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_LAYER_H_
#define NET_HTTP_HTTP_NETWORK_LAYER_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class ClientSocketFactory;
class HostResolver;
class HttpNetworkSession;
class ProxyInfo;
class ProxyService;
class SSLConfigService;

class HttpNetworkLayer : public HttpTransactionFactory {
 public:
  // |socket_factory|, |proxy_service| and |host_resolver| must remain valid
  // for the lifetime of HttpNetworkLayer.
  HttpNetworkLayer(ClientSocketFactory* socket_factory,
                   HostResolver* host_resolver, ProxyService* proxy_service,
                   SSLConfigService* ssl_config_service);
  // Construct a HttpNetworkLayer with an existing HttpNetworkSession which
  // contains a valid ProxyService.
  explicit HttpNetworkLayer(HttpNetworkSession* session);
  ~HttpNetworkLayer();

  // This function hides the details of how a network layer gets instantiated
  // and allows other implementations to be substituted.
  static HttpTransactionFactory* CreateFactory(
      HostResolver* host_resolver,
      ProxyService* proxy_service,
      SSLConfigService* ssl_config_service);
  // Create a transaction factory that instantiate a network layer over an
  // existing network session. Network session contains some valuable
  // information (e.g. authentication data) that we want to share across
  // multiple network layers. This method exposes the implementation details
  // of a network layer, use this method with an existing network layer only
  // when network session is shared.
  static HttpTransactionFactory* CreateFactory(HttpNetworkSession* session);

  // HttpTransactionFactory methods:
  virtual int CreateTransaction(scoped_ptr<HttpTransaction>* trans);
  virtual HttpCache* GetCache();
  virtual HttpNetworkSession* GetSession();
  virtual void Suspend(bool suspend);

  // Enable the flip protocol.
  // Without calling this function, FLIP is disabled.  The mode can be:
  //   ""            : (default) SSL and compression are enabled.
  //   "no-ssl"      : disables SSL.
  //   "no-compress" : disables compression.
  //   "none"        : disables both SSL and compression.
  static void EnableFlip(const std::string& mode);

 private:
  // The factory we will use to create network sockets.
  ClientSocketFactory* socket_factory_;

  // The host resolver and proxy service that will be used when lazily
  // creating |session_|.
  scoped_refptr<HostResolver> host_resolver_;
  scoped_refptr<ProxyService> proxy_service_;

  // The SSL config service being used for the session.
  scoped_refptr<SSLConfigService> ssl_config_service_;

  scoped_refptr<HttpNetworkSession> session_;
  bool suspended_;
  static bool enable_flip_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_LAYER_H_
