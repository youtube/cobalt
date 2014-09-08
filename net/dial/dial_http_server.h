// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DIAL_DIAL_HTTP_SERVER_H
#define NET_DIAL_DIAL_HTTP_SERVER_H

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/tcp_listen_socket.h"
#include "net/dial/dial_service_handler.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server.h"

namespace net {

class IPEndPoint;
class HttpResponseInfo;

class NET_EXPORT DialHttpServer : public HttpServer::Delegate,
    public base::RefCountedThreadSafe<DialHttpServer> {
 public:
  DialHttpServer();
  virtual ~DialHttpServer();

  bool Start();
  bool Stop();

  // HttpServer::Delegate implementation
  virtual void OnHttpRequest(int conn_id,
                             const HttpServerRequestInfo& info) OVERRIDE;

  virtual void OnClose(int conn_id) OVERRIDE;

  // Unused HttpServer::Delegate
  virtual void OnWebSocketRequest(int connection_id,
                                  const HttpServerRequestInfo& info) OVERRIDE {
  }

  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE { }

  // Return the formatted application URL
  std::string application_url() const {
    return server_url_ + "apps/";
  }

  // Return the formatted location URL.
  std::string location_url() const {
    return server_url_ + "dd.xml";
  }

  // Somewhat similar to HttpServer::GetLocalAddress, but figures out the
  // network IP address and uses that. The port remains the same.
  int GetLocalAddress(IPEndPoint* addr);

 private:
  void ConfigureApplicationUrl();

  // Send the DIAL Device Description Manifest to the client.
  void SendDeviceDescriptionManifest(int conn_id);

  // Callbacks Javascript Handlers, if any.
  bool CallbackJsHttpRequest(int conn_id, const HttpServerRequestInfo& info);
  void AsyncReceivedResponse(int, HttpServerResponseInfo*, bool);

  scoped_ptr<TCPListenSocketFactory> factory_;
  scoped_refptr<HttpServer> http_server_;
  std::string server_url_;
};

} // namespace net

#endif // NET_DIAL_DIAL_HTTP_SERVER_H

