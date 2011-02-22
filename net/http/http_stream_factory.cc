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
#include "net/http/http_alternate_protocols.h"

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

HttpStreamFactory::~HttpStreamFactory() {}

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
  host_mapping_rules().RewriteHost(&host_port);

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
