// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_NETWORK_DIAL_DIAL_HTTP_SERVER_H_
#define COBALT_NETWORK_DIAL_DIAL_HTTP_SERVER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "cobalt/network/dial/dial_service_handler.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
namespace cobalt {
namespace network {

class DialService;

// This class is created and owned by DialService and is not meant to be
// used externally.
// All functions run on the DialService's message loop, except for
// the callback AsyncReceivedResponse() which is eventually called
// by a DialServiceHandler.
// It's refcounted threadsafe so we can safely bind it to the callback we pass
// to DialServiceHandler::handleRequest().
class NET_EXPORT DialHttpServer
    : public net::HttpServer::Delegate,
      public base::RefCountedThreadSafe<DialHttpServer> {
 public:
  explicit DialHttpServer(DialService* dial_service);
  void Stop();

  // HttpServer::Delegate implementation
  void OnConnect(int /*conn_id*/) override{};
  void OnHttpRequest(int conn_id,
                     const net::HttpServerRequestInfo& info) override;

  void OnClose(int conn_id) override;

  // Unused HttpServer::Delegate
  void OnWebSocketRequest(int /*connection_id*/,
                          const net::HttpServerRequestInfo& /*info*/) override {
  }

  void OnWebSocketMessage(int /*connection_id*/,
                          const std::string /*data*/) override {}

  // Return the formatted application URL
  std::string application_url() const { return server_url_ + "apps/"; }

  // Return the formatted location URL.
  std::string location_url() const { return server_url_ + "dd.xml"; }

  // Somewhat similar to HttpServer::GetLocalAddress, but figures out the
  // network IP address and uses that. The port remains the same.
  int GetLocalAddress(net::IPEndPoint* addr);

 private:
  friend class base::RefCountedThreadSafe<DialHttpServer>;
  virtual ~DialHttpServer();

  void ConfigureApplicationUrl();

  // Send the DIAL Device Description Manifest to the client.
  void SendDeviceDescriptionManifest(int conn_id);

  // Query DIAL service for a handler for the given request.
  // Return false if no handler found, true if handleRequest() was issued
  // to the handler.
  bool DispatchToHandler(int conn_id, const net::HttpServerRequestInfo& info);

  // Callback from WebKit thread when the HTTP task is complete.
  // Post the response info to DIAL service thread.
  void AsyncReceivedResponse(
      int conn_id, std::unique_ptr<net::HttpServerResponseInfo> response);
  // Handles DIAL response.
  void OnReceivedResponse(
      int conn_id, std::unique_ptr<net::HttpServerResponseInfo> response);

  std::unique_ptr<net::HttpServer> http_server_;
  std::string server_url_;
  // DialService owns this object.
  DialService* dial_service_;

  // Message Loop of the thread that created us. We make sure http server
  // is only called on the proper thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DialHttpServer);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DIAL_DIAL_HTTP_SERVER_H_
