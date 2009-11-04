// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/logging.h"
#include "net/flip/flip_framer.h"
#include "net/flip/flip_network_transaction.h"
#include "net/flip/flip_session.h"
#include "net/flip/flip_session_pool.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/socket/client_socket_factory.h"

namespace net {

//-----------------------------------------------------------------------------

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    HostResolver* host_resolver,
    ProxyService* proxy_service,
    SSLConfigService* ssl_config_service) {
  DCHECK(proxy_service);

  return new HttpNetworkLayer(ClientSocketFactory::GetDefaultFactory(),
                              host_resolver, proxy_service, ssl_config_service);
}

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    HttpNetworkSession* session) {
  DCHECK(session);

  return new HttpNetworkLayer(session);
}

//-----------------------------------------------------------------------------
bool HttpNetworkLayer::enable_flip_ = false;

HttpNetworkLayer::HttpNetworkLayer(ClientSocketFactory* socket_factory,
                                   HostResolver* host_resolver,
                                   ProxyService* proxy_service,
                                   SSLConfigService* ssl_config_service)
    : socket_factory_(socket_factory),
      host_resolver_(host_resolver),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      session_(NULL),
      flip_session_pool_(NULL),
      suspended_(false) {
  DCHECK(proxy_service_);
  DCHECK(ssl_config_service_.get());
}

HttpNetworkLayer::HttpNetworkLayer(HttpNetworkSession* session)
    : socket_factory_(ClientSocketFactory::GetDefaultFactory()),
      ssl_config_service_(NULL),
      session_(session),
      flip_session_pool_(session->flip_session_pool()),
      suspended_(false) {
  DCHECK(session_.get());
}

HttpNetworkLayer::~HttpNetworkLayer() {
}

int HttpNetworkLayer::CreateTransaction(scoped_ptr<HttpTransaction>* trans) {
  if (suspended_)
    return ERR_NETWORK_IO_SUSPENDED;

  if (enable_flip_)
    trans->reset(new FlipNetworkTransaction(GetSession()));
  else
    trans->reset(new HttpNetworkTransaction(GetSession()));
  return OK;
}

HttpCache* HttpNetworkLayer::GetCache() {
  return NULL;
}

void HttpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  if (suspend && session_)
    session_->tcp_socket_pool()->CloseIdleSockets();
}

HttpNetworkSession* HttpNetworkLayer::GetSession() {
  if (!session_) {
    DCHECK(proxy_service_);
    FlipSessionPool* flip_pool = enable_flip_ ? new FlipSessionPool : NULL;
    session_ = new HttpNetworkSession(
        host_resolver_, proxy_service_, socket_factory_,
        ssl_config_service_, flip_pool);
    // These were just temps for lazy-initializing HttpNetworkSession.
    host_resolver_ = NULL;
    proxy_service_ = NULL;
    socket_factory_ = NULL;
  }
  return session_;
}

// static
void HttpNetworkLayer::EnableFlip(const std::string& mode) {
  static const std::string kDisableSSL("no-ssl");
  static const std::string kDisableCompression("no-compress");
  static const std::string kDisableEverything("no-ssl-no-compress");

  // Enable flip mode.
  enable_flip_ = true;

  // Disable SSL
  if (mode == kDisableEverything || mode == kDisableSSL)
    FlipSession::SetSSLMode(false);

  // Disable compression
  if (mode == kDisableEverything || mode == kDisableCompression)
    flip::FlipFramer::set_enable_compression_default(false);
}

}  // namespace net
