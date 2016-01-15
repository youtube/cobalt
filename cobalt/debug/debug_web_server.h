/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DEBUG_DEBUG_WEB_SERVER_H_
#define COBALT_DEBUG_DEBUG_WEB_SERVER_H_

#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "cobalt/base/console_values.h"
#include "cobalt/debug/debug_server.h"
#include "net/base/stream_listen_socket.h"
#include "net/server/http_server.h"

namespace cobalt {
namespace debug {

// This class implements the net::HttpServer::Delegate interface, and
// is registered with a net::HttpServer instance that this class owns.
class DebugWebServer : private net::HttpServer::Delegate {
 public:
  // Callback to create the debug server.
  typedef base::Callback<scoped_ptr<debug::DebugServer>(
      const debug::DebugServer::OnEventCallback&,
      const debug::DebugServer::OnDetachCallback&)> CreateDebugServerCallback;

  DebugWebServer(int port,
                 const CreateDebugServerCallback& create_debugger_callback);
  ~DebugWebServer();

 protected:
  // net::HttpServer::Delegate implementation.
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) OVERRIDE;

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) OVERRIDE;

  void OnWebSocketMessage(int connection_id, const std::string& json) OVERRIDE;

  void OnClose(int connection_id) OVERRIDE {
    UNREFERENCED_PARAMETER(connection_id);
  }

  // Debugger command response handler.
  void OnDebuggerResponse(int id, const base::optional<std::string>& response);

  // Handlers for debugger notifications. These may be called on an arbitrary
  // thread.
  void OnDebuggerEvent(const std::string& method,
                       const base::optional<std::string>& json_params) const;

  void OnDebuggerDetach(const std::string& reason) const;

 private:
  std::string GetLocalAddress() const;

  void StartServer(int port);

  void SendErrorResponseOverWebSocket(int id, const std::string& message);

  base::ThreadChecker thread_checker_;
  base::Thread http_server_thread_;
  scoped_ptr<net::StreamListenSocketFactory> factory_;
  scoped_refptr<net::HttpServer> server_;
  CreateDebugServerCallback create_debugger_callback_;
  scoped_ptr<DebugServer> debugger_;
  int websocket_id_;
  base::CVal<std::string> local_address_;
  FilePath content_root_dir_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_DEBUG_WEB_SERVER_H_
