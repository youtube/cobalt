// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_server_properties.h"

namespace net {

// WARNING: If you modify or add any static flags, you must keep them in sync
// with |ResetStaticSettingsToInit|. This is critical for unit test isolation.

// static
const HostMappingRules* HttpStreamFactory::host_mapping_rules_ = NULL;
// static
std::vector<std::string>* HttpStreamFactory::next_protos_ = NULL;
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
// static
bool HttpStreamFactory::http_pipelining_enabled_ = false;
// static
uint16 HttpStreamFactory::testing_fixed_http_port_ = 0;
// static
uint16 HttpStreamFactory::testing_fixed_https_port_ = 0;

HttpStreamFactory::~HttpStreamFactory() {}

// static
void HttpStreamFactory::ResetStaticSettingsToInit() {
  // WARNING: These must match the initializers above.
  delete host_mapping_rules_;
  delete next_protos_;
  delete forced_spdy_exclusions_;
  host_mapping_rules_ = NULL;
  next_protos_ = NULL;
  spdy_enabled_ = true;
  use_alternate_protocols_ = false;
  force_spdy_over_ssl_ = true;
  force_spdy_always_ = false;
  forced_spdy_exclusions_ = NULL;
  ignore_certificate_errors_ = false;
}

void HttpStreamFactory::ProcessAlternateProtocol(
    HttpServerProperties* http_server_properties,
    const std::string& alternate_protocol_str,
    const HostPortPair& http_host_port_pair) {
  std::vector<std::string> port_protocol_vector;
  base::SplitString(alternate_protocol_str, ':', &port_protocol_vector);
  if (port_protocol_vector.size() != 2) {
    DLOG(WARNING) << kAlternateProtocolHeader
                  << " header has too many tokens: "
                  << alternate_protocol_str;
    return;
  }

  int port;
  if (!base::StringToInt(port_protocol_vector[0], &port) ||
      port <= 0 || port >= 1 << 16) {
    DLOG(WARNING) << kAlternateProtocolHeader
                  << " header has unrecognizable port: "
                  << port_protocol_vector[0];
    return;
  }

  AlternateProtocol protocol = ALTERNATE_PROTOCOL_BROKEN;
  // We skip NPN_SPDY_1 here, because we've rolled the protocol version to 2.
  for (int i = NPN_SPDY_2; i < NUM_ALTERNATE_PROTOCOLS; ++i) {
    if (port_protocol_vector[1] == kAlternateProtocolStrings[i])
      protocol = static_cast<AlternateProtocol>(i);
  }

  if (protocol == ALTERNATE_PROTOCOL_BROKEN) {
    // Currently, we only recognize the npn-spdy protocol.
    DLOG(WARNING) << kAlternateProtocolHeader
                  << " header has unrecognized protocol: "
                  << port_protocol_vector[1];
    return;
  }

  HostPortPair host_port(http_host_port_pair);
  host_mapping_rules().RewriteHost(&host_port);

  if (http_server_properties->HasAlternateProtocol(host_port)) {
    const PortAlternateProtocolPair existing_alternate =
        http_server_properties->GetAlternateProtocol(host_port);
    // If we think the alternate protocol is broken, don't change it.
    if (existing_alternate.protocol == ALTERNATE_PROTOCOL_BROKEN)
      return;
  }

  http_server_properties->SetAlternateProtocol(host_port, port, protocol);
}

GURL HttpStreamFactory::ApplyHostMappingRules(const GURL& url,
                                              HostPortPair* endpoint) {
  if (host_mapping_rules().RewriteHost(endpoint)) {
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

// static
void HttpStreamFactory::add_forced_spdy_exclusion(const std::string& value) {
  HostPortPair pair = HostPortPair::FromURL(GURL(value));
  if (!forced_spdy_exclusions_)
    forced_spdy_exclusions_ = new std::list<HostPortPair>();
  forced_spdy_exclusions_->push_back(pair);
}

// static
bool HttpStreamFactory::HasSpdyExclusion(const HostPortPair& endpoint) {
  std::list<HostPortPair>* exclusions = forced_spdy_exclusions_;
  if (!exclusions)
    return false;

  std::list<HostPortPair>::const_iterator it;
  for (it = exclusions->begin(); it != exclusions->end(); ++it)
    if (it->Equals(endpoint))
      return true;
  return false;
}

// static
void HttpStreamFactory::SetHostMappingRules(const std::string& rules) {
  HostMappingRules* host_mapping_rules = new HostMappingRules;
  host_mapping_rules->SetRulesFromString(rules);
  delete host_mapping_rules_;
  host_mapping_rules_ = host_mapping_rules;
}

HttpStreamFactory::HttpStreamFactory() {}

// static
const HostMappingRules& HttpStreamFactory::host_mapping_rules() {
  if (!host_mapping_rules_)
    host_mapping_rules_ = new HostMappingRules;
  return *host_mapping_rules_;
}

}  // namespace net
