// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
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
  static const char kExclude[] = "exclude";  // Hosts to exclude
  static const char kDisableCompression[] = "no-compress";
  static const char kDisableAltProtocols[] = "no-alt-protocols";
  static const char kEnableVersionOne[] = "v1";
  static const char kForceAltProtocols[] = "force-alt-protocols";
  static const char kSingleDomain[] = "single-domain";

  // If flow-control is enabled, received WINDOW_UPDATE and SETTINGS
  // messages are processed and outstanding window size is actually obeyed
  // when sending data frames, and WINDOW_UPDATE messages are generated
  // when data is consumed.
  static const char kEnableFlowControl[] = "flow-control";

  // We want an A/B experiment between SPDY enabled and SPDY disabled,
  // but only for pages where SPDY *could have been* negotiated.  To do
  // this, we use NPN, but prevent it from negotiating SPDY.  If the
  // server negotiates HTTP, rather than SPDY, today that will only happen
  // on servers that installed NPN (and could have done SPDY).  But this is
  // a bit of a hack, as this correlation between NPN and SPDY is not
  // really guaranteed.
  static const char kEnableNPN[] = "npn";
  static const char kEnableNpnHttpOnly[] = "npn-http";

  // Except for the first element, the order is irrelevant.  First element
  // specifies the fallback in case nothing matches
  // (SSLClientSocket::kNextProtoNoOverlap).  Otherwise, the SSL library
  // will choose the first overlapping protocol in the server's list, since
  // it presumedly has a better understanding of which protocol we should
  // use, therefore the rest of the ordering here is not important.
  static const char kNpnProtosFull[] = "\x08http/1.1\x06spdy/2";
  // This is a temporary hack to pretend we support version 1.
  static const char kNpnProtosFullV1[] = "\x08http/1.1\x06spdy/1";
  // No spdy specified.
  static const char kNpnProtosHttpOnly[] = "\x08http/1.1\x07http1.1";

  std::vector<std::string> spdy_options;
  base::SplitString(mode, ',', &spdy_options);

  bool use_alt_protocols = true;

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
      SpdySession::SetSSLMode(false);  // Disable SSL
      HttpStreamFactory::set_force_spdy_over_ssl(false);
      HttpStreamFactory::set_force_spdy_always(true);
    } else if (option == kSSL) {
      HttpStreamFactory::set_force_spdy_over_ssl(true);
      HttpStreamFactory::set_force_spdy_always(true);
    } else if (option == kExclude) {
      HttpStreamFactory::add_forced_spdy_exclusion(value);
    } else if (option == kDisableCompression) {
      spdy::SpdyFramer::set_enable_compression_default(false);
    } else if (option == kEnableNPN) {
      HttpStreamFactory::set_use_alternate_protocols(use_alt_protocols);
      HttpStreamFactory::set_next_protos(kNpnProtosFull);
    } else if (option == kEnableNpnHttpOnly) {
      // Avoid alternate protocol in this case. Otherwise, browser will try SSL
      // and then fallback to http. This introduces extra load.
      HttpStreamFactory::set_use_alternate_protocols(false);
      HttpStreamFactory::set_next_protos(kNpnProtosHttpOnly);
    } else if (option == kEnableVersionOne) {
      spdy::SpdyFramer::set_protocol_version(1);
      HttpStreamFactory::set_next_protos(kNpnProtosFullV1);
    } else if (option == kDisableAltProtocols) {
      use_alt_protocols = false;
      HttpStreamFactory::set_use_alternate_protocols(false);
    } else if (option == kEnableFlowControl) {
      SpdySession::set_flow_control(true);
    } else if (option == kForceAltProtocols) {
      HttpAlternateProtocols::PortProtocolPair pair;
      pair.port = 443;
      pair.protocol = HttpAlternateProtocols::NPN_SPDY_2;
      HttpAlternateProtocols::ForceAlternateProtocol(pair);
    } else if (option == kSingleDomain) {
      SpdySessionPool::ForceSingleDomain();
      LOG(ERROR) << "FORCING SINGLE DOMAIN";
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

void HttpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;

  if (suspend && session_)
    session_->CloseIdleConnections();
}

}  // namespace net
