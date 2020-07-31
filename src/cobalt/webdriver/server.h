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

#ifndef COBALT_WEBDRIVER_SERVER_H_
#define COBALT_WEBDRIVER_SERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "cobalt/base/c_val.h"
#include "cobalt/webdriver/protocol/server_status.h"
#include "net/server/http_server.h"
#include "net/socket/tcp_server_socket.h"

namespace cobalt {
namespace webdriver {

// This class implements the net::HttpServer::Delegate interface, and thus
// is registered with a net::HttpServer instance that this class owns.
// the WebDriverServer is responsible for providing an interface to respond
// to WebDriver API requests without needing to interact with the HttpServer
// directly.
// The WebDriverServer and related classes will take care of parsing the
// Http requests from a client, as well as preparing the Http responses.
class WebDriverServer : public net::HttpServer::Delegate {
 public:
  enum HttpMethod {
    kUnknownMethod,
    kGet,
    // TODO: Support HEAD requests
    kPost,
    kDelete
  };

  // The spec describes how responses for successful, failed, and invalid
  // commands should be formed. The ResponseHandler class will craft an
  // appropriate Http Responses for these cases, and send the response to the
  // client.
  class ResponseHandler {
   public:
    // Called after a successful WebDriver command.
    // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Responses
    virtual void Success(std::unique_ptr<base::Value>) = 0;
    // |content_type| specifies the type of the data using HTTP mime types.
    virtual void SuccessData(const std::string& content_type, const char* data,
                             int len) = 0;

    // Called after a failed WebDriver command
    // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Failed-Commands
    virtual void FailedCommand(std::unique_ptr<base::Value>) = 0;

    // Called after an invalid request.
    // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Invalid-Requests
    virtual void UnknownCommand(const std::string& path) = 0;
    virtual void UnimplementedCommand(const std::string& path) = 0;
    virtual void VariableResourceNotFound(const std::string& variable_name) = 0;
    virtual void InvalidCommandMethod(
        HttpMethod requested_method,
        const std::vector<HttpMethod>& allowed_methods) = 0;
    // TODO: The message should be a list of the missing parameters.
    virtual void MissingCommandParameters(const std::string& message) = 0;
    virtual ~ResponseHandler() {}
  };

  typedef base::Callback<void(HttpMethod, const std::string&,
                              std::unique_ptr<base::Value>,
                              std::unique_ptr<ResponseHandler>)>
      HandleRequestCallback;

  // |address_cval_name| is the name of the CVal that identifies the Webdriver
  // address and port
  WebDriverServer(int port, const std::string& listen_ip,
                  const HandleRequestCallback& callback,
                  const std::string& address_cval_name);

 protected:
  // net::HttpServer::Delegate implementation.
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;

  void OnWebSocketRequest(int, const net::HttpServerRequestInfo&) override {}

  void OnWebSocketMessage(int, const std::string&) override {}

  void OnClose(int) override {}  // NOLINT(readability/casting)

 private:
  THREAD_CHECKER(thread_checker_);
  HandleRequestCallback handle_request_callback_;
  std::unique_ptr<net::HttpServer> server_;
  base::CVal<std::string> server_address_;

  friend std::unique_ptr<WebDriverServer>::deleter_type;
};

}  // namespace webdriver
}  // namespace cobalt

#if defined(COMPILER_GCC) && !defined(COMPILER_SNC)

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<cobalt::webdriver::WebDriverServer::HttpMethod> {
  size_t operator()(
      cobalt::webdriver::WebDriverServer::HttpMethod method) const {
    return hash<size_t>()(static_cast<size_t>(method));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

#endif  // COBALT_WEBDRIVER_SERVER_H_
