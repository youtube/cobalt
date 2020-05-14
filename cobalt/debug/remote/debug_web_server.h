// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DEBUG_REMOTE_DEBUG_WEB_SERVER_H_
#define COBALT_DEBUG_REMOTE_DEBUG_WEB_SERVER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "cobalt/base/c_val.h"
#include "cobalt/debug/debug_client.h"
#include "net/server/http_server.h"

namespace cobalt {
namespace debug {
namespace remote {

// This class implements the net::HttpServer::Delegate interface, and
// is registered with a net::HttpServer instance that this class owns.
//
// Also implements the DebugClient::Delegate interface, to receive events
// from the DebugClient instance, |debug_client_|, owned by this class.
class DebugWebServer : public net::HttpServer::Delegate,
                       public DebugClient::Delegate {
 public:
  DebugWebServer(int port, const std::string& listen_ip,
                 const CreateDebugClientCallback& create_debug_client_callback);
  ~DebugWebServer();

 protected:
  // net::HttpServer::Delegate implementation.
  void OnConnect(int connection_id) override {}
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;

  void OnWebSocketMessage(int connection_id, const std::string& json) override;

  void OnClose(int connection_id) override;

  // Debugger command response handler.
  void OnDebuggerResponse(int id, const base::Optional<std::string>& response);

  // DebugClient::Delegate implementation.
  void OnDebugClientEvent(const std::string& method,
                          const std::string& json_params) override;

  void OnDebugClientDetach(const std::string& reason) override;

 private:
  void StartServer(int port, const std::string& listen_ip);

  void StopServer();

  void SendErrorResponseOverWebSocket(int id, const std::string& message);

  THREAD_CHECKER(thread_checker_);
  base::Thread http_server_thread_;
  // net::HttpServer is a ref-counted object, so we have to use scoped_refptr.
  std::unique_ptr<net::HttpServer> server_;
  CreateDebugClientCallback create_debug_client_callback_;

  // The debug client that connects to the dispatcher.
  std::unique_ptr<DebugClient> debug_client_;

  int websocket_id_;
  base::CVal<std::string> local_address_;
  base::FilePath content_root_dir_;
};

}  // namespace remote
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_REMOTE_DEBUG_WEB_SERVER_H_
