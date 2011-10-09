// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"

namespace net {

// The interface for setting/retrieving the HTTP server properties.
class NET_EXPORT HttpServerProperties {
 public:
  typedef std::vector<std::string> StringVector;

  HttpServerProperties() {}
  virtual ~HttpServerProperties() {}

  // Returns true if |server| supports SPDY.
  virtual bool SupportsSpdy(const HostPortPair& server) const = 0;

  // Add |server| into the persistent store. Should only be called from IO
  // thread.
  virtual void SetSupportsSpdy(const HostPortPair& server,
                               bool support_spdy) = 0;

  // Deletes all data.
  virtual void DeleteAll() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerProperties);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_H_
