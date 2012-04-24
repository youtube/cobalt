// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

//-----------------------------------------------------------------------------
HttpNetworkLayer::HttpNetworkLayer(HttpNetworkSession* session)
    : session_(session),
      suspended_(false) {
  DCHECK(session_.get());
}

HttpNetworkLayer::~HttpNetworkLayer() {
}

//-----------------------------------------------------------------------------

// static
HttpTransactionFactory* HttpNetworkLayer::CreateFactory(
    HttpNetworkSession* session) {
  DCHECK(session);

  return new HttpNetworkLayer(session);
}

// static
void HttpNetworkLayer::EnableSpdy(const std::string& mode) {
  static const char kOff[] = "off";
  static const char kSSL[] = "ssl";
  static const char kDisableSSL[] = "no-ssl";
  static const char kDisablePing[] = "no-ping";
  static const char kExclude[] = "exclude";  // Hosts to exclude
  static const char kDisableCompression[] = "no-compress";
  static const char kDisableAltProtocols[] = "no-alt-protocols";
  static const char kForceAltProtocols[] = "force-alt-protocols";
  static const char kSingleDomain[] = "single-domain";

  static const char kInitialMaxConcurrentStreams[] = "init-max-streams";

  std::vector<std::string> spdy_options;
  base::SplitString(mode, ',', &spdy_options);

  for (std::vector<std::string>::iterator it = spdy_options.begin();
       it != spdy_options.end(); ++it) {
    const std::string& element = *it;
    std::vector<std::string> name_value;
    base::SplitString(element, '=', &name_value);
    const std::string& option = name_value[0];
    const std::string value = name_value.size() > 1 ? name_value[1] : "";

    if (option == kOff) {
      HttpStreamFactory::set_spdy_enabled(false);
    } else if (option == kDisableSSL) {
      SpdySession::set_default_protocol(kProtoSPDY2);
      HttpStreamFactory::set_force_spdy_over_ssl(false);
      HttpStreamFactory::set_force_spdy_always(true);
    } else if (option == kSSL) {
      SpdySession::set_default_protocol(kProtoSPDY2);
      HttpStreamFactory::set_force_spdy_over_ssl(true);
      HttpStreamFactory::set_force_spdy_always(true);
    } else if (option == kDisablePing) {
      SpdySession::set_enable_ping_based_connection_checking(false);
    } else if (option == kExclude) {
      HttpStreamFactory::add_forced_spdy_exclusion(value);
    } else if (option == kDisableCompression) {
      BufferedSpdyFramer::set_enable_compression_default(false);
    } else if (option == kDisableAltProtocols) {
      HttpStreamFactory::set_use_alternate_protocols(false);
    } else if (option == kForceAltProtocols) {
      PortAlternateProtocolPair pair;
      pair.port = 443;
      pair.protocol = NPN_SPDY_2;
      HttpServerPropertiesImpl::ForceAlternateProtocol(pair);
    } else if (option == kSingleDomain) {
      SpdySessionPool::ForceSingleDomain();
      LOG(ERROR) << "FORCING SINGLE DOMAIN";
    } else if (option == kInitialMaxConcurrentStreams) {
      int streams;
      if (base::StringToInt(value, &streams) && streams > 0)
        SpdySession::set_init_max_concurrent_streams(streams);
    } else if (option.empty() && it == spdy_options.begin()) {
      continue;
    } else {
      LOG(DFATAL) << "Unrecognized spdy option: " << option;
    }
  }
}

//-----------------------------------------------------------------------------

int HttpNetworkLayer::CreateTransaction(scoped_ptr<HttpTransaction>* trans) {
  if (suspended_)
    return ERR_NETWORK_IO_SUSPENDED;

  trans->reset(new HttpNetworkTransaction(GetSession()));
  return OK;
}

HttpCache* HttpNetworkLayer::GetCache() {
  return NULL;
}

HttpNetworkSession* HttpNetworkLayer::GetSession() {
  return session_;
}

void HttpNetworkLayer::OnSuspend() {
  suspended_ = true;

  if (session_)
    session_->CloseIdleConnections();
}

void HttpNetworkLayer::OnResume() {
  suspended_ = false;
}

}  // namespace net
