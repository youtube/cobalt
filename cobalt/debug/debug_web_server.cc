// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/debug/debug_web_server.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/debug/json_object.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_listen_socket.h"
#include "net/server/http_server_request_info.h"
#include "starboard/socket.h"

namespace cobalt {
namespace debug {

namespace {

std::string GetMimeType(const FilePath& path) {
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

base::optional<FilePath> AppendIndexFile(const FilePath& directory) {
  DCHECK(file_util::DirectoryExists(directory));
  FilePath result;
  result = directory.AppendASCII("index.html");
  if (file_util::PathExists(result)) {
    return result;
  }
  result = directory.AppendASCII("index.json");
  if (file_util::PathExists(result)) {
    return result;
  }
  DLOG(ERROR) << "No index file found at: " << directory.value();
  return base::nullopt;
}

base::optional<std::string> GetLocalIpAddress() {
  net::IPEndPoint ip_addr;
  SbSocketAddress local_ip;
  SbMemorySet(&local_ip, 0, sizeof(local_ip));
  bool result = false;

  // Prefer IPv4 addresses, as they're easier to type for debugging.
  SbSocketAddressType address_types[] = {kSbSocketAddressTypeIpv4,
                                         kSbSocketAddressTypeIpv6};

  for (std::size_t i = 0; i != SB_ARRAY_SIZE(address_types); ++i) {
    SbSocketAddress destination;
    SbMemorySet(&(destination.address), 0, sizeof(destination.address));
    destination.type = address_types[i];
    if (!SbSocketGetInterfaceAddress(&destination, &local_ip, NULL)) {
      continue;
    }

    if (ip_addr.FromSbSocketAddress(&local_ip)) {
      result = true;
      break;
    }
  }

  if (!result) {
    DLOG(WARNING) << "Unable to get a local interface address.";
    return base::nullopt;
  }

  return ip_addr.ToStringWithoutPort();
}

const char kContentDir[] = "cobalt/debug";
const char kDetached[] = "Inspector.detached";
const char kDetachReasonField[] = "params.reason";
const char kErrorField[] = "error.message";
const char kIdField[] = "id";
const char kMethodField[] = "method";
const char kParamsField[] = "params";
}  // namespace

DebugWebServer::DebugWebServer(
    int port, const GetDebugServerCallback& get_debug_server_callback)
    : http_server_thread_("DebugWebServer"),
      get_debug_server_callback_(get_debug_server_callback),
      websocket_id_(-1),
      // Local address will be set when the web server is successfully started.
      local_address_("Cobalt.Server.DevTools", "<NOT RUNNING>",
                     "Address to connect to for remote debugging.") {
  // Construct the content root directory to serve files from.
  PathService::Get(paths::DIR_COBALT_WEB_ROOT, &content_root_dir_);
  content_root_dir_ = content_root_dir_.AppendASCII(kContentDir);

  // Start the Http server thread and create the server on that thread.
  // Thread checker will be attached to that thread in |StartServer|.
  thread_checker_.DetachFromThread();
  const size_t stack_size = 0;
  http_server_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, stack_size));
  http_server_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DebugWebServer::StartServer, base::Unretained(this), port));
}

DebugWebServer::~DebugWebServer() {
  // Destroy the server on its own thread then stop the thread.
  http_server_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DebugWebServer::StopServer, base::Unretained(this)));
  http_server_thread_.Stop();
}

void DebugWebServer::OnHttpRequest(int connection_id,
                                   const net::HttpServerRequestInfo& info) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  FilePath file_path(content_root_dir_);
  file_path = file_path.AppendASCII(url_path);

  // If the disk path is a directory, look for an index file.
  if (file_util::DirectoryExists(file_path)) {
    base::optional<FilePath> index_file_path = AppendIndexFile(file_path);
    if (index_file_path) {
      file_path = *index_file_path;
    } else {
      DLOG(WARNING) << "No index file in directory: " << file_path.value();
      server_->Send404(connection_id);
      return;
    }
  }

  // If we can read the local file, send its contents, otherwise send a 404.
  std::string data;
  if (file_util::PathExists(file_path) &&
      file_util::ReadFileToString(file_path, &data)) {
    DLOG(INFO) << "Sending data from: " << file_path.value();
    std::string mime_type = GetMimeType(file_path);
    server_->Send200(connection_id, data, mime_type);
  } else {
    DLOG(WARNING) << "Cannot read file: " << file_path.value();
    server_->Send404(connection_id);
  }
}

void DebugWebServer::OnWebSocketRequest(
    int connection_id, const net::HttpServerRequestInfo& info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string path = info.path;
  DLOG(INFO) << "Got web socket request [" << connection_id << "]: " << path;

  // Ignore the path and bind any web socket request to the debugger.
  websocket_id_ = connection_id;
  server_->AcceptWebSocket(connection_id, info);

  DebugServer* debug_server = get_debug_server_callback_.Run();
  debug_client_.reset(new DebugClient(debug_server, this));
}

void DebugWebServer::OnWebSocketMessage(int connection_id,
                                        const std::string& json) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(connection_id, websocket_id_);

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
  base::Value* params_value = NULL;
  std::string json_params;
  if (json_object->Remove(kParamsField, &params_value)) {
    base::DictionaryValue* params_dictionary = NULL;
    params_value->GetAsDictionary(&params_dictionary);
    JSONObject params(params_dictionary);
    DCHECK(params);
    json_params = JSONStringify(params);
  }

  if (!debug_client_ || !debug_client_->IsAttached()) {
    return SendErrorResponseOverWebSocket(
        id, "Debugger is not connected - call attach first.");
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
  server_->SendOverWebSocket(websocket_id_, JSONStringify(response));
}

void DebugWebServer::OnDebuggerResponse(
    int id, const base::optional<std::string>& response) {
  JSONObject response_object = JSONParse(response.value());
  DCHECK(response_object);
  response_object->SetInteger(kIdField, id);
  server_->SendOverWebSocket(websocket_id_, JSONStringify(response_object));
}

void DebugWebServer::OnDebugClientEvent(
    const std::string& method, const base::optional<std::string>& json_params) {
  // Debugger events occur on the thread of the web module the debugger is
  // attached to, so we must post to the server thread here.
  if (MessageLoop::current() != http_server_thread_.message_loop()) {
    http_server_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&DebugWebServer::OnDebugClientEvent,
                              base::Unretained(this), method, json_params));
    return;
  }

  JSONObject event(new base::DictionaryValue());
  event->SetString(kMethodField, method);
  JSONObject params = JSONParse(json_params.value());
  // |params| may be NULL if event does not use them.
  if (params) {
    event->Set(kParamsField, params.release());
  }
  server_->SendOverWebSocket(websocket_id_, JSONStringify(event));
}

void DebugWebServer::OnDebugClientDetach(const std::string& reason) {
  // Debugger events occur on the thread of the web module the debugger is
  // attached to, so we must post to the server thread here.
  if (MessageLoop::current() != http_server_thread_.message_loop()) {
    http_server_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&DebugWebServer::OnDebugClientDetach,
                              base::Unretained(this), reason));
    return;
  }

  DLOG(INFO) << "Got detach event: " << reason;
  JSONObject event(new base::DictionaryValue());
  event->SetString(kMethodField, kDetached);
  event->SetString(kDetachReasonField, reason);
  server_->SendOverWebSocket(websocket_id_, JSONStringify(event));
}

int DebugWebServer::GetLocalAddress(std::string* out) const {
  net::IPEndPoint ip_addr;
  int result = server_->GetLocalAddress(&ip_addr);
  if (result == net::OK) {
    *out = std::string("http://") + ip_addr.ToString();
  }
  return result;
}

void DebugWebServer::StartServer(int port) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Create http server
  const base::optional<std::string> ip_addr = GetLocalIpAddress();
  if (!ip_addr) {
    DLOG(WARNING)
        << "Could not get a local IP address for the debug web server.";
    return;
  }
  factory_.reset(new net::TCPListenSocketFactory(*ip_addr, port));
  server_ = new net::HttpServer(*factory_, this);

  std::string address;
  int result = GetLocalAddress(&address);
  if (result == net::OK) {
    DLOG(INFO) << "Debug web server running at: " << address;
    local_address_ = address;
  } else {
    DLOG(WARNING) << "Could not start debug web server";
  }
}

void DebugWebServer::StopServer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  server_ = NULL;
}

}  // namespace debug
}  // namespace cobalt
