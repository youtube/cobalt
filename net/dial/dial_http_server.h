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

class DialService;
class HttpResponseInfo;
class IPEndPoint;

// This class is created and owned by DialService and is not meant to be
// used externally.
// All functions run on the DialService's message loop, except for
// the callback AsyncReceivedResponse() which is eventually called
// by a DialServiceHandler.
// It's refcounted threadsafe so we can safely bind it to the callback we pass
// to DialServiceHandler::handleRequest().
class NET_EXPORT DialHttpServer : public HttpServer::Delegate,
    public base::RefCountedThreadSafe<DialHttpServer> {
 public:
  explicit DialHttpServer(DialService* dial_service);
  void Stop();

  // HttpServer::Delegate implementation
  virtual void OnHttpRequest(int conn_id,
                             const HttpServerRequestInfo& info) override;

  virtual void OnClose(int conn_id) override;

  // Unused HttpServer::Delegate
  virtual void OnWebSocketRequest(
      int /*connection_id*/,
      const HttpServerRequestInfo& /*info*/) override {}

  virtual void OnWebSocketMessage(int /*connection_id*/,
                                  const std::string& /*data*/) override {}

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
  friend class base::RefCountedThreadSafe<DialHttpServer>;
  virtual ~DialHttpServer();

  void ConfigureApplicationUrl();

  // Send the DIAL Device Description Manifest to the client.
  void SendDeviceDescriptionManifest(int conn_id);

  // Query DIAL service for a handler for the given request.
  // Return false if no handler found, true if handleRequest() was issued
  // to the handler.
  bool DispatchToHandler(int conn_id, const HttpServerRequestInfo& info);

  // Callback from WebKit thread when the HTTP task is complete.
  // Post the response info to DIAL service thread.
  void AsyncReceivedResponse(int conn_id,
                             scoped_ptr<HttpServerResponseInfo> response);
  // Handles DIAL response.
  void OnReceivedResponse(int conn_id,
                          scoped_ptr<HttpServerResponseInfo> response);

  TCPListenSocketFactory factory_;
  scoped_refptr<HttpServer> http_server_;
  std::string server_url_;
  // DialService owns this object.
  DialService* dial_service_;

  // Message Loop of the thread that created us. We make sure http server
  // is only called on the proper thread.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(DialHttpServer);
};

} // namespace net

#endif // NET_DIAL_DIAL_HTTP_SERVER_H

