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

#include "cobalt/webdriver/server.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_listen_socket.h"
#include "net/server/http_server_request_info.h"

namespace cobalt {
namespace webdriver {
namespace {

const char kJsonContentType[] = "application/json;charset=UTF-8";
const char kTextPlainContentType[] = "text/plain";

WebDriverServer::HttpMethod StringToHttpMethod(const std::string& method) {
  if (LowerCaseEqualsASCII(method, "get")) {
    return WebDriverServer::kGet;
  } else if (LowerCaseEqualsASCII(method, "post")) {
    return WebDriverServer::kPost;
  } else if (LowerCaseEqualsASCII(method, "delete")) {
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
    default:
      return "UNKNOWN";
  }
}

// Implementation of the ResponseHandler interface.
// A Delegate implementation will call the methods on this interface for
// successful, failed, and invalid requests.
// For each of these cases, prepare an appropriate Http Response according to
// the spec, and send it to the specified connection through the net::HttpServer
// instance.
class ResponseHandlerImpl : public WebDriverServer::ResponseHandler {
 public:
  ResponseHandlerImpl(const scoped_refptr<net::HttpServer>& server,
                      int connection_id)
      : server_message_loop_(base::MessageLoopProxy::current()),
        server_(server),
        connection_id_(connection_id) {}

  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Responses
  void Success(scoped_ptr<base::Value> value) override {
    DCHECK(value);
    std::string data;
    base::JSONWriter::Write(value.get(), &data);
    SendInternal(net::HTTP_OK, data, kJsonContentType);
  }

  // Failed commands map to a valid WebDriver command and contain the expected
  // parameters, but otherwise failed to execute for some reason. This should
  // send a 500 Internal Server Error.
  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Error_Handling
  void FailedCommand(scoped_ptr<base::Value> value) override {
    DCHECK(value);
    std::string data;
    base::JSONWriter::Write(value.get(), &data);
    SendInternal(net::HTTP_INTERNAL_SERVER_ERROR, data, kJsonContentType);
  }

  // A number of cases for invalid requests are explained here:
  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Invalid_Requests
  // The response type should be text/plain and the message body is an error
  // message

  // The command request is not mapped to anything.
  void UnknownCommand(const std::string& path) override {
    LOG(INFO) << "Unknown command: " << path;
    SendInternal(net::HTTP_NOT_FOUND, "Unknown command",
                 kTextPlainContentType);
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
    std::vector<std::string> headers;
    headers.push_back("Allow: " + JoinString(allowed_method_strings, ", "));
    SendInternal(net::HTTP_METHOD_NOT_ALLOWED,
                 "Invalid method: " + HttpMethodToString(requested_method),
                 kTextPlainContentType, headers);
  }

  // The POST command's JSON request body does not contain the required
  // parameters.
  void MissingCommandParameters(const std::string& message) override {
    SendInternal(net::HTTP_BAD_REQUEST, message, kTextPlainContentType);
  }

 private:
  static void SendToServer(const scoped_refptr<net::HttpServer>& server,
                           int connection_id, net::HttpStatusCode status,
                           const std::string& message,
                           const std::string& content_type,
                           const std::vector<std::string>& headers) {
    server->Send(connection_id, status, message, content_type, headers);
  }

  // Send response with no additional headers specified.
  void SendInternal(net::HttpStatusCode status, const std::string& message,
                    const std::string& content_type) {
    std::vector<std::string> headers;
    SendInternal(status, message, content_type, headers);
  }

  // Send a response on the Http Server's thread.
  void SendInternal(net::HttpStatusCode status, const std::string& message,
                    const std::string& content_type,
                    const std::vector<std::string>& headers) {
    if (base::MessageLoopProxy::current() == server_message_loop_) {
      SendToServer(server_, connection_id_, status, message, content_type,
                   headers);
    } else {
      base::Closure closure =
          base::Bind(&SendToServer, server_, connection_id_, status, message,
                     content_type, headers);
      server_message_loop_->PostTask(FROM_HERE, closure);
    }
  }

  scoped_refptr<base::MessageLoopProxy> server_message_loop_;
  scoped_refptr<net::HttpServer> server_;
  int connection_id_;
};
}  // namespace

WebDriverServer::WebDriverServer(int port, const std::string& listen_ip,
                                 const HandleRequestCallback& callback)
    : handle_request_callback_(callback),
      server_address_("WebDriver.Server",
                      "Address to communicate with WebDriver.") {
  // Create http server
  factory_.reset(new net::TCPListenSocketFactory(listen_ip, port));
  server_ = new net::HttpServer(*factory_, this);
  GURL address;
  int result = GetLocalAddress(&address);
  if (result == net::OK) {
    LOG(INFO) << "Starting WebDriver server on port " << port;
    server_address_ = address.spec();
  } else {
    LOG(WARNING) << "Could not start WebDriver server";
    server_address_ = "<NOT RUNNING>";
  }
}

void WebDriverServer::OnHttpRequest(int connection_id,
                                    const net::HttpServerRequestInfo& info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string path = info.path;
  size_t query_position = path.find("?");
  // Discard any URL variables from the path
  if (query_position != std::string::npos) {
    path.resize(query_position);
  }

  DLOG(INFO) << "Got request: " << path;
  // Create a new ResponseHandler that will send a response to this connection.
  scoped_ptr<ResponseHandler> response_handler(
      new ResponseHandlerImpl(server_, connection_id));

  scoped_ptr<base::Value> parameters;
  HttpMethod method = StringToHttpMethod(info.method);
  if (method == kPost) {
    base::JSONReader reader;
    parameters.reset(reader.ReadToValue(info.data));
    if (!parameters) {
      // Failed to parse request body as JSON.
      response_handler->MissingCommandParameters(reader.GetErrorMessage());
      return;
    }
  }

  // Call the HandleRequestCallback.
  handle_request_callback_.Run(StringToHttpMethod(info.method), path,
                               parameters.Pass(), response_handler.Pass());
}

int WebDriverServer::GetLocalAddress(GURL* out) const {
  net::IPEndPoint ip_addr;
  int result = server_->GetLocalAddress(&ip_addr);
  if (result == net::OK) {
    *out = GURL("http://" + ip_addr.ToString());
  }
  return result;
}

}  // namespace webdriver
}  // namespace cobalt
