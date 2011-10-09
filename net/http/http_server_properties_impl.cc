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

void HttpServerPropertiesImpl::Initialize(StringVector* spdy_servers,
                                          bool support_spdy) {
  DCHECK(CalledOnValidThread());
  spdy_servers_table_.clear();
  if (!spdy_servers)
    return;
  for (StringVector::iterator it = spdy_servers->begin();
       it != spdy_servers->end(); ++it) {
    spdy_servers_table_[*it] = support_spdy;
  }
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

void HttpServerPropertiesImpl::DeleteAll() {
  DCHECK(CalledOnValidThread());
  spdy_servers_table_.clear();
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

}  // namespace net
