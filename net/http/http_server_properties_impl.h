// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_server_properties.h"

namespace base {
class ListValue;
}

namespace net {

// The implementation for setting/retrieving the HTTP server properties.
class NET_EXPORT HttpServerPropertiesImpl
    : public HttpServerProperties,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  HttpServerPropertiesImpl();
  virtual ~HttpServerPropertiesImpl();

  // Initializes |spdy_servers_table_| with the servers (host/port) from
  // |spdy_servers| that either support SPDY or not.
  void Initialize(StringVector* spdy_servers, bool support_spdy);

  // Returns true if |server| supports SPDY.
  virtual bool SupportsSpdy(const HostPortPair& server) const OVERRIDE;

  // Add |server| into the persistent store.
  virtual void SetSupportsSpdy(const HostPortPair& server,
                               bool support_spdy) OVERRIDE;

  // Deletes all data.
  virtual void DeleteAll() OVERRIDE;

  // Get the list of servers (host/port) that support SPDY.
  void GetSpdyServerList(base::ListValue* spdy_server_list) const;

  // Returns flattened string representation of the |host_port_pair|. Used by
  // unittests.
  static std::string GetFlattenedSpdyServer(
      const net::HostPortPair& host_port_pair);

 private:
  // |spdy_servers_table_| has flattened representation of servers (host/port
  // pair) that either support or not support SPDY protocol.
  typedef base::hash_map<std::string, bool> SpdyServerHostPortTable;
  SpdyServerHostPortTable spdy_servers_table_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
