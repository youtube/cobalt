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

#ifndef COBALT_WEBDRIVER_SERVER_H_
#define COBALT_WEBDRIVER_SERVER_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "cobalt/base/c_val.h"
#include "cobalt/webdriver/protocol/server_status.h"
#include "googleurl/src/gurl.h"
#include "net/base/stream_listen_socket.h"
#include "net/server/http_server.h"

namespace cobalt {
namespace webdriver {

// This class implements the net::HttpServer::Delegate interface, and thus
// is registered with a net::HttpServer instance that this class owns.
// the WebDriverServer is responsible for providing an interface to respond
// to WebDriver API requests without needing to interact with the HttpServer
// directly.
// The WebDriverServer and related classes will take care of parsing the
// Http requests from a client, as well as preparing the Http responses.
class WebDriverServer : private net::HttpServer::Delegate {
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
    // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Responses
    virtual void Success(scoped_ptr<base::Value>) = 0;

    // Called after a failed WebDriver command
    // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Failed_Commands
    virtual void FailedCommand(scoped_ptr<base::Value>) = 0;

    // Called after an invalid request.
    // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Invalid_Requests
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

  typedef base::Callback<void(
      HttpMethod, const std::string&, scoped_ptr<base::Value>,
      scoped_ptr<ResponseHandler>)> HandleRequestCallback;

  WebDriverServer(int port, const std::string& listen_ip,
                  const HandleRequestCallback& callback);

 protected:
  // net::HttpServer::Delegate implementation.
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;

  void OnWebSocketRequest(int, const net::HttpServerRequestInfo&) override {}

  void OnWebSocketMessage(int, const std::string&) override {}

  void OnClose(int) override {}  // NOLINT(readability/casting)

 private:
  int GetLocalAddress(GURL* out) const;

  base::ThreadChecker thread_checker_;
  HandleRequestCallback handle_request_callback_;
  scoped_ptr<net::StreamListenSocketFactory> factory_;
  scoped_refptr<net::HttpServer> server_;
  base::CVal<std::string> server_address_;
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
