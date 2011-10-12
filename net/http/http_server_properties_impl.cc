// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"

namespace net {

HttpServerPropertiesImpl::HttpServerPropertiesImpl() {
}

HttpServerPropertiesImpl::~HttpServerPropertiesImpl() {
}

void HttpServerPropertiesImpl::InitializeSpdyServers(
    std::vector<std::string>* spdy_servers,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  spdy_servers_table_.clear();
  if (!spdy_servers)
    return;
  for (std::vector<std::string>::iterator it = spdy_servers->begin();
       it != spdy_servers->end(); ++it) {
    spdy_servers_table_[*it] = support_spdy;
  }
}

void HttpServerPropertiesImpl::InitializeAlternateProtocolServers(
    AlternateProtocolMap* alternate_protocol_map) {
  // First swap, and then add back all the ALTERNATE_PROTOCOL_BROKEN ones since
  // those don't get persisted.
  alternate_protocol_map_.swap(*alternate_protocol_map);
  for (AlternateProtocolMap::const_iterator it =
       alternate_protocol_map->begin();
       it != alternate_protocol_map->end(); ++it) {
    if (it->second.protocol == ALTERNATE_PROTOCOL_BROKEN)
      alternate_protocol_map_[it->first] = it->second;
  }
}

void HttpServerPropertiesImpl::GetSpdyServerList(
    base::ListValue* spdy_server_list) const {
  DCHECK(CalledOnValidThread());
  DCHECK(spdy_server_list);
  spdy_server_list->Clear();
  // Get the list of servers (host/port) that support SPDY.
  for (SpdyServerHostPortTable::const_iterator it = spdy_servers_table_.begin();
       it != spdy_servers_table_.end(); ++it) {
    const std::string spdy_server_host_port = it->first;
    if (it->second)
      spdy_server_list->Append(new StringValue(spdy_server_host_port));
  }
}

// static
std::string HttpServerPropertiesImpl::GetFlattenedSpdyServer(
    const net::HostPortPair& host_port_pair) {
  std::string spdy_server;
  spdy_server.append(host_port_pair.host());
  spdy_server.append(":");
  base::StringAppendF(&spdy_server, "%d", host_port_pair.port());
  return spdy_server;
}

static const PortAlternateProtocolPair* g_forced_alternate_protocol = NULL;

// static
void HttpServerPropertiesImpl::ForceAlternateProtocol(
    const PortAlternateProtocolPair& pair) {
  // Note: we're going to leak this.
  if (g_forced_alternate_protocol)
    delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = new PortAlternateProtocolPair(pair);
}

// static
void HttpServerPropertiesImpl::DisableForcedAlternateProtocol() {
  delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = NULL;
}

void HttpServerPropertiesImpl::Clear() {
  DCHECK(CalledOnValidThread());
  spdy_servers_table_.clear();
  alternate_protocol_map_.clear();
}

bool HttpServerPropertiesImpl::SupportsSpdy(
    const net::HostPortPair& host_port_pair) const {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;
  std::string spdy_server = GetFlattenedSpdyServer(host_port_pair);

  SpdyServerHostPortTable::const_iterator spdy_host_port =
      spdy_servers_table_.find(spdy_server);
  if (spdy_host_port != spdy_servers_table_.end())
    return spdy_host_port->second;
  return false;
}

void HttpServerPropertiesImpl::SetSupportsSpdy(
    const net::HostPortPair& host_port_pair,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return;
  std::string spdy_server = GetFlattenedSpdyServer(host_port_pair);

  SpdyServerHostPortTable::iterator spdy_host_port =
      spdy_servers_table_.find(spdy_server);
  if ((spdy_host_port != spdy_servers_table_.end()) &&
      (spdy_host_port->second == support_spdy)) {
    return;
  }
  // Cache the data.
  spdy_servers_table_[spdy_server] = support_spdy;
}

bool HttpServerPropertiesImpl::HasAlternateProtocol(
    const HostPortPair& server) const {
  return ContainsKey(alternate_protocol_map_, server) ||
      g_forced_alternate_protocol;
}

PortAlternateProtocolPair
HttpServerPropertiesImpl::GetAlternateProtocol(
    const HostPortPair& server) const {
  DCHECK(HasAlternateProtocol(server));

  // First check the map.
  AlternateProtocolMap::const_iterator it =
      alternate_protocol_map_.find(server);
  if (it != alternate_protocol_map_.end())
    return it->second;

  // We must be forcing an alternate.
  DCHECK(g_forced_alternate_protocol);
  return *g_forced_alternate_protocol;
}

void HttpServerPropertiesImpl::SetAlternateProtocol(
    const HostPortPair& server,
    uint16 alternate_port,
    AlternateProtocol alternate_protocol) {
  if (alternate_protocol == ALTERNATE_PROTOCOL_BROKEN) {
    LOG(DFATAL) << "Call MarkBrokenAlternateProtocolFor() instead.";
    return;
  }

  PortAlternateProtocolPair alternate;
  alternate.port = alternate_port;
  alternate.protocol = alternate_protocol;
  if (HasAlternateProtocol(server)) {
    const PortAlternateProtocolPair existing_alternate =
        GetAlternateProtocol(server);

    if (existing_alternate.protocol == ALTERNATE_PROTOCOL_BROKEN) {
      DVLOG(1) << "Ignore alternate protocol since it's known to be broken.";
      return;
    }

    if (alternate_protocol != ALTERNATE_PROTOCOL_BROKEN &&
        !existing_alternate.Equals(alternate)) {
      LOG(WARNING) << "Changing the alternate protocol for: "
                   << server.ToString()
                   << " from [Port: " << existing_alternate.port
                   << ", Protocol: " << existing_alternate.protocol
                   << "] to [Port: " << alternate_port
                   << ", Protocol: " << alternate_protocol
                   << "].";
    }
  }

  alternate_protocol_map_[server] = alternate;
}

void HttpServerPropertiesImpl::SetBrokenAlternateProtocol(
    const HostPortPair& server) {
  alternate_protocol_map_[server].protocol = ALTERNATE_PROTOCOL_BROKEN;
}

const AlternateProtocolMap&
HttpServerPropertiesImpl::alternate_protocol_map() const {
  return alternate_protocol_map_;
}

}  // namespace net
