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

#include "cobalt/webdriver/server.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"

namespace cobalt {
namespace webdriver {
namespace {

const char kJsonContentType[] = "application/json;charset=UTF-8";
const char kTextPlainContentType[] = "text/plain";
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webdriver_server", "WebDriver Server");

WebDriverServer::HttpMethod StringToHttpMethod(const std::string& method) {
  if (base::LowerCaseEqualsASCII(method, "get")) {
    return WebDriverServer::kGet;
  } else if (base::LowerCaseEqualsASCII(method, "post")) {
    return WebDriverServer::kPost;
  } else if (base::LowerCaseEqualsASCII(method, "delete")) {
    return WebDriverServer::kDelete;
  }
  return WebDriverServer::kUnknownMethod;
}

std::string HttpMethodToString(WebDriverServer::HttpMethod method) {
  switch (method) {
    case WebDriverServer::kGet:
      return "GET";
    case WebDriverServer::kPost:
      return "POST";
    case WebDriverServer::kDelete:
      return "DELETE";
    case WebDriverServer::kUnknownMethod:
      return "UNKNOWN";
  }
  NOTREACHED();
  return "";
}

// Implementation of the ResponseHandler interface.
// A Delegate implementation will call the methods on this interface for
// successful, failed, and invalid requests.
// For each of these cases, prepare an appropriate Http Response according to
// the spec, and send it to the specified connection through the net::HttpServer
// instance.
class ResponseHandlerImpl : public WebDriverServer::ResponseHandler {
 public:
  ResponseHandlerImpl(net::HttpServer* server, int connection_id)
      : task_runner_(base::MessageLoop::current()->task_runner()),
        server_(server),
        connection_id_(connection_id) {}

  // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Responses
  void Success(std::unique_ptr<base::Value> value) override {
    DCHECK(value);
    std::string data;
    base::JSONWriter::Write(*value, &data);
    SendInternal(net::HTTP_OK, data, kJsonContentType);
  }

  void SuccessData(const std::string& content_type, const char* data,
                   int len) override {
    std::string data_copied(data, len);
    server_->Send(connection_id_, net::HTTP_OK, data_copied, content_type,
                  kTrafficAnnotation);
  }

  // Failed commands map to a valid WebDriver command and contain the expected
  // parameters, but otherwise failed to execute for some reason. This should
  // send a 500 Internal Server Error.
  // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Error-Handling
  void FailedCommand(std::unique_ptr<base::Value> value) override {
    DCHECK(value);
    std::string data;
    base::JSONWriter::Write(*value, &data);
    SendInternal(net::HTTP_INTERNAL_SERVER_ERROR, data, kJsonContentType);
  }

  // A number of cases for invalid requests are explained here:
  // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Invalid-Requests
  // The response type should be text/plain and the message body is an error
  // message

  // The command request is not mapped to anything.
  void UnknownCommand(const std::string& path) override {
    LOG(INFO) << "Unknown command: " << path;
    SendInternal(net::HTTP_NOT_FOUND, "Unknown command", kTextPlainContentType);
  }

  // The command request is mapped to a valid command, but this WebDriver
  // implementation has not implemented it.
  void UnimplementedCommand(const std::string& path) override {
    LOG(INFO) << "Unimplemented command: " << path;
    SendInternal(net::HTTP_NOT_IMPLEMENTED, "Unimplemented command",
                 kTextPlainContentType);
  }

  // The request maps to a valid command, but the variable part of the path
  // does not map to a valid instance.
  void VariableResourceNotFound(const std::string& variable_name) override {
    SendInternal(net::HTTP_NOT_FOUND,
                 "Unknown variable resource: " + variable_name,
                 kTextPlainContentType);
  }

  // The request maps to a valid command, but with an unsupported Http method.
  void InvalidCommandMethod(WebDriverServer::HttpMethod requested_method,
                            const std::vector<WebDriverServer::HttpMethod>&
                                allowed_methods) override {
    DCHECK(!allowed_methods.empty());
    std::vector<std::string> allowed_method_strings;
    for (int i = 0; i < allowed_methods.size(); ++i) {
      allowed_method_strings.push_back(HttpMethodToString(allowed_methods[i]));
    }
    net::HttpServerResponseInfo response_info;
    response_info.AddHeader("Allow",
                            base::JoinString(allowed_method_strings, ", "));
    SendInternal(net::HTTP_METHOD_NOT_ALLOWED,
                 "Invalid method: " + HttpMethodToString(requested_method),
                 kTextPlainContentType, response_info);
  }

  // The POST command's JSON request body does not contain the required
  // parameters.
  void MissingCommandParameters(const std::string& message) override {
    SendInternal(net::HTTP_BAD_REQUEST, message, kTextPlainContentType);
  }

 private:
  static void SendToServer(
      net::HttpServer* server, int connection_id, net::HttpStatusCode status,
      const std::string& message, const std::string& content_type,
      const base::Optional<net::HttpServerResponseInfo>& response_info) {
    if (response_info) {
      server->SendResponse(connection_id, response_info.value(),
                           kTrafficAnnotation);
    }
    server->Send(connection_id, status, message, content_type,
                 kTrafficAnnotation);
  }

  // Send a response on the Http Server's thread.
  void SendInternal(net::HttpStatusCode status, const std::string& message,
                    const std::string& content_type,
                    base::Optional<net::HttpServerResponseInfo> response_info =
                        base::Optional<net::HttpServerResponseInfo>()) {
    if (base::MessageLoop::current()->task_runner() == task_runner_) {
      SendToServer(server_, connection_id_, status, message, content_type,
                   response_info);
    } else {
      base::Closure closure =
          base::Bind(&SendToServer, server_, connection_id_, status, message,
                     content_type, response_info);
      task_runner_->PostTask(FROM_HERE, closure);
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  net::HttpServer* server_;
  int connection_id_;
};
}  // namespace

WebDriverServer::WebDriverServer(int port, const std::string& listen_ip,
                                 const HandleRequestCallback& callback,
                                 const std::string& address_cval_name)
    : handle_request_callback_(callback),
      server_address_(address_cval_name,
                      "Address to communicate with WebDriver.") {
  // Create http server
  std::unique_ptr<net::ServerSocket> server_socket =
      std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
  server_socket->ListenWithAddressAndPort(listen_ip, port, 1 /*backlog*/);
  server_ = std::make_unique<net::HttpServer>(std::move(server_socket), this);
  net::IPEndPoint ip_addr;
  int result = server_->GetLocalInterfaceAddress(&ip_addr);
  if (result == net::OK) {
    LOG(INFO) << "Starting WebDriver server on port " << port;
    server_address_ = "http://" + ip_addr.ToString();
  } else {
    LOG(WARNING) << "Could not start WebDriver server";
    server_address_ = "<NOT RUNNING>";
  }
}

void WebDriverServer::OnConnect(int connection_id) {}

void WebDriverServer::OnHttpRequest(int connection_id,
                                    const net::HttpServerRequestInfo& info) {
  TRACE_EVENT0("cobalt::webdriver", "WebDriverServer::OnHttpRequest()");

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string path = info.path;
  size_t query_position = path.find("?");
  // Discard any URL variables from the path
  if (query_position != std::string::npos) {
    path.resize(query_position);
  }

  DLOG(INFO) << "Got request: " << path;
  // Create a new ResponseHandler that will send a response to this connection.
  std::unique_ptr<ResponseHandler> response_handler(
      new ResponseHandlerImpl(server_.get(), connection_id));

  std::unique_ptr<base::Value> parameters;
  HttpMethod method = StringToHttpMethod(info.method);
  if (method == kPost) {
    base::JSONReader reader;
    parameters = std::move(reader.ReadToValue(info.data));
    if (!parameters) {
      // Failed to parse request body as JSON.
      response_handler->MissingCommandParameters(reader.GetErrorMessage());
      return;
    }
  }

  // Call the HandleRequestCallback.
  handle_request_callback_.Run(StringToHttpMethod(info.method), path,
                               std::move(parameters),
                               std::move(response_handler));
}

}  // namespace webdriver
}  // namespace cobalt
