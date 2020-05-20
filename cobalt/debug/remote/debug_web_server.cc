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

#include "cobalt/debug/remote/debug_web_server.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/debug/json_object.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_server_socket.h"
#include "starboard/common/socket.h"

namespace cobalt {
namespace debug {
namespace remote {

namespace {

constexpr char kLogBrowserEntryAdded[] = "Log.browserEntryAdded";

std::string GetMimeType(const base::FilePath& path) {
  if (path.MatchesExtension(".html")) {
    return "text/html";
  } else if (path.MatchesExtension(".css")) {
    return "text/css";
  } else if (path.MatchesExtension(".js")) {
    return "application/javascript";
  } else if (path.MatchesExtension(".png")) {
    return "image/png";
  } else if (path.MatchesExtension(".gif")) {
    return "image/gif";
  } else if (path.MatchesExtension(".json")) {
    return "application/json";
  } else if (path.MatchesExtension(".svg")) {
    return "image/svg+xml";
  } else if (path.MatchesExtension(".ico")) {
    return "image/x-icon";
  }
  DLOG(ERROR) << "GetMimeType doesn't know mime type for: " << path.value()
              << " text/plain will be returned";
  return "text/plain";
}

base::Optional<base::FilePath> AppendIndexFile(
    const base::FilePath& directory) {
  DCHECK(base::DirectoryExists(directory));
  base::FilePath result;
  result = directory.AppendASCII("index.html");
  if (base::PathExists(result)) {
    return result;
  }
  result = directory.AppendASCII("index.json");
  if (base::PathExists(result)) {
    return result;
  }
  DLOG(ERROR) << "No index file found at: " << directory.value();
  return base::nullopt;
}

const char kContentDir[] = "debug_remote";
const char kDetached[] = "Inspector.detached";
const char kDetachReasonField[] = "params.reason";
const char kErrorField[] = "error.message";
const char kIdField[] = "id";
const char kMethodField[] = "method";
const char kParamsField[] = "params";
constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cobalt_debug_web_server",
                                        "cobalt_debug_web_server");

constexpr int kUnattachedWebSocketId = -1;
}  // namespace

DebugWebServer::DebugWebServer(
    int port, const std::string& listen_ip,
    const CreateDebugClientCallback& create_debug_client_callback)
    : http_server_thread_("DebugWebServer"),
      create_debug_client_callback_(create_debug_client_callback),
      websocket_id_(kUnattachedWebSocketId),
      // Local address will be set when the web server is successfully started.
      local_address_("Cobalt.Server.DevTools", "<NOT RUNNING>",
                     "Address to connect to for remote debugging.") {
  // Construct the content root directory to serve files from.
  base::PathService::Get(paths::DIR_COBALT_WEB_ROOT, &content_root_dir_);
  content_root_dir_ = content_root_dir_.AppendASCII(kContentDir);

  // Start the Http server thread and create the server on that thread.
  // Thread checker will be attached to that thread in |StartServer|.
  DETACH_FROM_THREAD(thread_checker_);
  const size_t stack_size = 0;
  http_server_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, stack_size));
  http_server_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DebugWebServer::StartServer,
                            base::Unretained(this), port, listen_ip));
}

DebugWebServer::~DebugWebServer() {
  // Destroy the server on its own thread then stop the thread.
  http_server_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DebugWebServer::StopServer, base::Unretained(this)));
  http_server_thread_.Stop();
}

void DebugWebServer::OnHttpRequest(int connection_id,
                                   const net::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DLOG(INFO) << "Got HTTP request: " << connection_id << ": " << info.path;

  // TODO: Requests for / or /json (listing of discoverable pages)
  // currently send static index pages. When the debugger has support to get
  // the current URL (and any other dynamic content), then the index pages
  // should be created dynamically from templates.

  // Get the relative URL path with no query parameters.
  std::string url_path = info.path;
  while (url_path[0] == '/') {
    url_path = url_path.substr(1);
  }
  size_t query_position = url_path.find("?");
  if (query_position != std::string::npos) {
    url_path.resize(query_position);
  }

  // Construct the local disk path corresponding to the request path.
  base::FilePath file_path(content_root_dir_);
  if (!base::IsStringASCII(url_path)) {
    LOG(WARNING) << "Got HTTP request with non-ASCII URL path.";
    server_->Send404(connection_id, kNetworkTrafficAnnotation);
    return;
  }
  file_path = file_path.AppendASCII(url_path);

  // If the disk path is a directory, look for an index file.
  if (base::DirectoryExists(file_path)) {
    base::Optional<base::FilePath> index_file_path = AppendIndexFile(file_path);
    if (index_file_path) {
      file_path = *index_file_path;
    } else {
      DLOG(WARNING) << "No index file in directory: " << file_path.value();
      server_->Send404(connection_id, kNetworkTrafficAnnotation);
      return;
    }
  }

  // If we can read the local file, send its contents, otherwise send a 404.
  std::string data;
  if (base::PathExists(file_path) && base::ReadFileToString(file_path, &data)) {
    DLOG(INFO) << "Sending data from: " << file_path.value();
    std::string mime_type = GetMimeType(file_path);
    server_->Send200(connection_id, data, mime_type, kNetworkTrafficAnnotation);
  } else {
    DLOG(WARNING) << "Cannot read file: " << file_path.value();
    server_->Send404(connection_id, kNetworkTrafficAnnotation);
  }
}

void DebugWebServer::OnWebSocketRequest(
    int connection_id, const net::HttpServerRequestInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string path = info.path;
  DLOG(INFO) << "Got web socket request [" << connection_id << "]: " << path;

  // Disconnect any other DevTools client that's already attached.
  if (websocket_id_ != kUnattachedWebSocketId) {
    server_->Close(websocket_id_);
  }

  // Ignore the path and bind any web socket request to the debugger.
  websocket_id_ = connection_id;
  server_->AcceptWebSocket(connection_id, info, kNetworkTrafficAnnotation);

  debug_client_ = create_debug_client_callback_.Run(this);
}

void DebugWebServer::OnClose(int connection_id) {
  if (connection_id == websocket_id_) {
    websocket_id_ = kUnattachedWebSocketId;
  }
}

void DebugWebServer::OnWebSocketMessage(int connection_id,
                                        const std::string& json) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(connection_id, websocket_id_) << "Mismatched WebSocket ID";

  // Parse the json string to get id, method and params.
  JSONObject json_object = JSONParse(json);
  if (!json_object) {
    return SendErrorResponseOverWebSocket(websocket_id_, "Error parsing JSON");
  }
  int id = 0;
  if (!json_object->GetInteger(kIdField, &id)) {
    return SendErrorResponseOverWebSocket(id, "Missing request id");
  }
  std::string method;
  if (!json_object->GetString(kMethodField, &method)) {
    return SendErrorResponseOverWebSocket(id, "Missing method");
  }
  // Parameters are optional.
  std::unique_ptr<base::Value> params_value;
  std::string json_params;
  if (json_object->Remove(kParamsField, &params_value)) {
    base::DictionaryValue* params_dictionary = NULL;
    params_value->GetAsDictionary(&params_dictionary);
    params_value.release();
    JSONObject params(params_dictionary);
    DCHECK(params);
    json_params = JSONStringify(params);
  }

  if (!debug_client_ || !debug_client_->IsAttached()) {
    return SendErrorResponseOverWebSocket(id, "Debugger is not connected.");
  }

  debug_client_->SendCommand(method, json_params,
                             base::Bind(&DebugWebServer::OnDebuggerResponse,
                                        base::Unretained(this), id));
}

void DebugWebServer::SendErrorResponseOverWebSocket(
    int id, const std::string& message) {
  DCHECK_GE(websocket_id_, 0);
  JSONObject response(new base::DictionaryValue());
  response->SetInteger(kIdField, id);
  response->SetString(kErrorField, message);
  server_->SendOverWebSocket(websocket_id_, JSONStringify(response),
                             kNetworkTrafficAnnotation);
}

void DebugWebServer::OnDebuggerResponse(
    int id, const base::Optional<std::string>& response) {
  JSONObject response_object = JSONParse(response.value());
  DCHECK(response_object);
  response_object->SetInteger(kIdField, id);
  server_->SendOverWebSocket(websocket_id_, JSONStringify(response_object),
                             kNetworkTrafficAnnotation);
}

void DebugWebServer::OnDebugClientEvent(const std::string& method,
                                        const std::string& json_params) {
  // Squelch the Cobalt-specific log message meant only for the overlay console.
  if (method == kLogBrowserEntryAdded) {
    return;
  }

  // Debugger events occur on the thread of the web module the debugger is
  // attached to, so we must post to the server thread here.
  if (base::MessageLoop::current() != http_server_thread_.message_loop()) {
    http_server_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&DebugWebServer::OnDebugClientEvent,
                              base::Unretained(this), method, json_params));
    return;
  }

  JSONObject event(new base::DictionaryValue());
  event->SetString(kMethodField, method);
  event->Set(kParamsField, JSONParse(json_params));
  server_->SendOverWebSocket(websocket_id_, JSONStringify(event),
                             kNetworkTrafficAnnotation);
}

void DebugWebServer::OnDebugClientDetach(const std::string& reason) {
  // Debugger events occur on the thread of the web module the debugger is
  // attached to, so we must post to the server thread here.
  if (base::MessageLoop::current() != http_server_thread_.message_loop()) {
    http_server_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&DebugWebServer::OnDebugClientDetach,
                              base::Unretained(this), reason));
    return;
  }

  DLOG(INFO) << "Got detach event: " << reason;
  JSONObject event(new base::DictionaryValue());
  event->SetString(kMethodField, kDetached);
  event->SetString(kDetachReasonField, reason);
  server_->SendOverWebSocket(websocket_id_, JSONStringify(event),
                             kNetworkTrafficAnnotation);
}

void DebugWebServer::StartServer(int port, const std::string& listen_ip) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Create http server
  auto* server_socket =
      new net::TCPServerSocket(NULL /*net_log*/, net::NetLogSource());
  server_socket->ListenWithAddressAndPort(
      listen_ip, static_cast<uint16_t>(port), 1 /*backlog*/);
  server_.reset(new net::HttpServer(
      std::unique_ptr<net::ServerSocket>(server_socket), this));

  net::IPEndPoint ip_addr;
  int result = server_->GetLocalInterfaceAddress(&ip_addr);
  if (result == net::OK) {
    std::string address = "http://" + ip_addr.ToString();
    // clang-format off
    LOG(INFO) << "\n---------------------------------"
              << "\n Connect to the web debugger at:"
              << "\n " << address
              << "\n---------------------------------";
    // clang-format on
    local_address_ = address;
  } else {
    LOG(WARNING) << "Could not start debug web server";
  }
}

void DebugWebServer::StopServer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  server_ = NULL;
}

}  // namespace remote
}  // namespace debug
}  // namespace cobalt
