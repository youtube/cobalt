// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_PROXY_CLIENT_SOCKET_H_
#define NET_HTTP_PROXY_CLIENT_SOCKET_H_
#pragma once

#include "net/socket/stream_socket.h"

namespace net {

class HttpAuthController;
class HttpStream;
class HttpResponseInfo;

class NET_EXPORT_PRIVATE ProxyClientSocket : public StreamSocket {
 public:
  ProxyClientSocket() {}
  virtual ~ProxyClientSocket() {}

  // Returns the HttpResponseInfo (including HTTP Headers) from
  // the response to the CONNECT request.
  virtual const HttpResponseInfo* GetConnectResponseInfo() const = 0;

  // Transfers ownership of a newly created HttpStream to the caller
  // which can be used to read the response body.
  virtual HttpStream* CreateConnectResponseStream() = 0;

  // Returns the HttpAuthController which can be used
  // to interact with an HTTP Proxy Authorization Required (407) request.
  virtual const scoped_refptr<HttpAuthController>& auth_controller() = 0;

  // If Connect (or its callback) returns PROXY_AUTH_REQUESTED, then
  // credentials should be added to the HttpAuthController before calling
  // RestartWithAuth.
  virtual int RestartWithAuth(OldCompletionCallback* callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyClientSocket);
};

}  // namespace net

#endif  // NET_HTTP_PROXY_CLIENT_SOCKET_H_
