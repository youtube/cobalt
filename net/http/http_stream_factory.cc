// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_request.h"

namespace net {

// static
const HostMappingRules* HttpStreamFactory::host_mapping_rules_ = NULL;
// static
const std::string* HttpStreamFactory::next_protos_ = NULL;
// static
bool HttpStreamFactory::spdy_enabled_ = true;
// static
bool HttpStreamFactory::use_alternate_protocols_ = false;
// static
bool HttpStreamFactory::force_spdy_over_ssl_ = true;
// static
bool HttpStreamFactory::force_spdy_always_ = false;
// static
std::list<HostPortPair>* HttpStreamFactory::forced_spdy_exclusions_ = NULL;
// static
bool HttpStreamFactory::ignore_certificate_errors_ = false;

HttpStreamFactory::HttpStreamFactory() {
}

HttpStreamFactory::~HttpStreamFactory() {
  RequestCallbackMap request_callback_map;
  request_callback_map.swap(request_callback_map_);
  for (RequestCallbackMap::iterator it = request_callback_map.begin();
       it != request_callback_map.end(); ++it) {
    delete it->first;
    // We don't invoke the callback in the destructor.
  }
}

// static
void HttpStreamFactory::SetHostMappingRules(const std::string& rules) {
  HostMappingRules* host_mapping_rules = new HostMappingRules();
  host_mapping_rules->SetRulesFromString(rules);
  delete host_mapping_rules_;
  host_mapping_rules_ = host_mapping_rules;
}

StreamRequest* HttpStreamFactory::RequestStream(
    const HttpRequestInfo* request_info,
    SSLConfig* ssl_config,
    ProxyInfo* proxy_info,
    HttpNetworkSession* session,
    StreamRequest::Delegate* delegate,
    const BoundNetLog& net_log) {
  HttpStreamRequest* stream = new HttpStreamRequest(this, session);
  stream->Start(request_info, ssl_config, proxy_info, delegate, net_log);
  return stream;
}

int HttpStreamFactory::PreconnectStreams(
    int num_streams,
    const HttpRequestInfo* request_info,
    SSLConfig* ssl_config,
    ProxyInfo* proxy_info,
    HttpNetworkSession* session,
    const BoundNetLog& net_log,
    CompletionCallback* callback) {
  HttpStreamRequest* stream = new HttpStreamRequest(this, session);
  int rv = stream->Preconnect(num_streams, request_info, ssl_config,
                              proxy_info, this, net_log);
  DCHECK_EQ(ERR_IO_PENDING, rv);
  request_callback_map_[stream] = callback;
  return rv;
}

void HttpStreamFactory::AddTLSIntolerantServer(const GURL& url) {
  tls_intolerant_servers_.insert(GetHostAndPort(url));
}

bool HttpStreamFactory::IsTLSIntolerantServer(const GURL& url) {
  return ContainsKey(tls_intolerant_servers_, GetHostAndPort(url));
}

void HttpStreamFactory::ProcessAlternateProtocol(
    HttpAlternateProtocols* alternate_protocols,
    const std::string& alternate_protocol_str,
    const HostPortPair& http_host_port_pair) {
  std::vector<std::string> port_protocol_vector;
  base::SplitString(alternate_protocol_str, ':', &port_protocol_vector);
  if (port_protocol_vector.size() != 2) {
    DLOG(WARNING) << HttpAlternateProtocols::kHeader
                  << " header has too many tokens: "
                  << alternate_protocol_str;
    return;
  }

  int port;
  if (!base::StringToInt(port_protocol_vector[0], &port) ||
      port <= 0 || port >= 1 << 16) {
    DLOG(WARNING) << HttpAlternateProtocols::kHeader
                  << " header has unrecognizable port: "
                  << port_protocol_vector[0];
    return;
  }

  HttpAlternateProtocols::Protocol protocol = HttpAlternateProtocols::BROKEN;
  // We skip NPN_SPDY_1 here, because we've rolled the protocol version to 2.
  for (int i = HttpAlternateProtocols::NPN_SPDY_2;
       i < HttpAlternateProtocols::NUM_ALTERNATE_PROTOCOLS; ++i) {
    if (port_protocol_vector[1] == HttpAlternateProtocols::kProtocolStrings[i])
      protocol = static_cast<HttpAlternateProtocols::Protocol>(i);
  }

  if (protocol == HttpAlternateProtocols::BROKEN) {
    // Currently, we only recognize the npn-spdy protocol.
    DLOG(WARNING) << HttpAlternateProtocols::kHeader
                  << " header has unrecognized protocol: "
                  << port_protocol_vector[1];
    return;
  }

  HostPortPair host_port(http_host_port_pair);
  if (host_mapping_rules_)
    host_mapping_rules_->RewriteHost(&host_port);

  if (alternate_protocols->HasAlternateProtocolFor(host_port)) {
    const HttpAlternateProtocols::PortProtocolPair existing_alternate =
        alternate_protocols->GetAlternateProtocolFor(host_port);
    // If we think the alternate protocol is broken, don't change it.
    if (existing_alternate.protocol == HttpAlternateProtocols::BROKEN)
      return;
  }

  alternate_protocols->SetAlternateProtocolFor(host_port, port, protocol);
}

GURL HttpStreamFactory::ApplyHostMappingRules(const GURL& url,
                                              HostPortPair* endpoint) {
  if (host_mapping_rules_ && host_mapping_rules_->RewriteHost(endpoint)) {
    url_canon::Replacements<char> replacements;
    const std::string port_str = base::IntToString(endpoint->port());
    replacements.SetPort(port_str.c_str(),
                         url_parse::Component(0, port_str.size()));
    replacements.SetHost(endpoint->host().c_str(),
                         url_parse::Component(0, endpoint->host().size()));
    return url.ReplaceComponents(replacements);
  }
  return url;
}

void HttpStreamFactory::OnPreconnectsComplete(
    HttpStreamRequest* request, int result) {
  RequestCallbackMap::iterator it = request_callback_map_.find(request);
  DCHECK(it != request_callback_map_.end());
  CompletionCallback* callback = it->second;
  request_callback_map_.erase(it);
  delete request;
  callback->Run(result);
}

}  // namespace net

