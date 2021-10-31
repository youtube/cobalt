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

#ifndef COBALT_WEBDRIVER_DISPATCHER_H_
#define COBALT_WEBDRIVER_DISPATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/optional.h"
#include "base/values.h"
#include "cobalt/webdriver/protocol/response.h"
#include "cobalt/webdriver/protocol/session_id.h"
#include "cobalt/webdriver/server.h"
#include "net/http/http_status_code.h"

namespace cobalt {
namespace webdriver {

// The WebDriverDispatcher maps URL paths to WebDriver command callbacks.
// The Dispatcher will take care of a number of types of Invalid requests and
// send a response to the server.
// TODO: Refactor such that the Dispatcher can detect all kinds of
//   invalid requests, such that the registered Callback for a given path will
//   only be called for valid requests.
// A registered URL can contain variable components, which are prefixed
// with a colon. The Dispatcher will create a PathVariableMap which maps these
// variable components of the path to the actual value in the request.
class WebDriverDispatcher {
 public:
  // A handle to an instance of this class is passed to registered command
  // callbacks. A URL path component that starts with a colon is recognized
  // as a variable component. Path variables for a given request can be looked
  // up using this structure.
  class PathVariableMap {
   public:
    std::string GetVariable(const std::string& variable_name) const {
      PathVariableMapInternal::const_iterator it =
          path_variable_map_.find(variable_name);
      if (it == path_variable_map_.end()) {
        NOTREACHED();
        return "";
      }
      return it->second;
    }

   protected:
    typedef base::hash_map<std::string, std::string> PathVariableMapInternal;
    explicit PathVariableMap(const PathVariableMapInternal& variable_map)
        : path_variable_map_(variable_map) {}
    const PathVariableMapInternal& path_variable_map_;
    friend class WebDriverDispatcher;
  };

  // An instance of this class is passed to WebDriver API command
  // implementations through the CommandCallback function.
  class CommandResultHandler {
   public:
    enum RequestError {
      kInvalidParameters,
      kInvalidPathVariable,
    };
    // Send the result of the execution of a registered WebDriver command to be
    // sent as a response as described in the spec:
    // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Responses
    // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Failed-Commands
    virtual void SendResult(
        const base::Optional<protocol::SessionId>& session_id,
        protocol::Response::StatusCode status_code,
        std::unique_ptr<base::Value> result) = 0;

    // Send data as a result of a command to the dispatcher. This is similar to
    // SendResult with the primary difference being the type of data can be of
    // any valid HTTP content type, specified with |content_type|. For example,
    // this could be used to send an image. Not used in any
    // commands in the WebDriver specification.
    virtual void SendResultWithContentType(
        protocol::Response::StatusCode status_code,
        const std::string& content_type, const char* data, int len) = 0;

    // Some forms of Invalid Requests are detected in the CommandCallback by
    // checking the path variables and command parameters. Invalid requests are
    // described here:
    // https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Invalid-Requests
    //
    // TODO: Invalid requests should be handled before calling the
    //   CommandCallback.
    virtual void SendInvalidRequestResponse(
        RequestError error, const std::string& error_string) = 0;

    virtual ~CommandResultHandler() {}
  };

  typedef base::Callback<void(const base::Value*, const PathVariableMap*,
                              std::unique_ptr<CommandResultHandler>)>
      DispatchCommandCallback;

  // Register the Http method and path (with possible variable components) to
  // the specified callback.
  void RegisterCommand(WebDriverServer::HttpMethod method,
                       const std::string& path,
                       const DispatchCommandCallback& cb);

  // Find the DispatchCommandCallback that is mapped to the requested method
  // and path. If found, execute the command with the given request_body as the
  // command's parameters. The result of the request will be sent to the
  // WebDriverServer::ResponseHandler.
  void HandleWebDriverServerRequest(
      WebDriverServer::HttpMethod method, const std::string& path,
      std::unique_ptr<base::Value> request_body,
      std::unique_ptr<WebDriverServer::ResponseHandler> handler);

 private:
  enum MatchStrategy {
    kMatchExact,
    kMatchVariables,
  };
  // Internal structure that manages command mappings for a given URL.
  // Each Http method can have a DispatchCommandCallback mapped to it.
  // The registered path is stored as a tokenized vector of the registered
  // path's components, preserving variable components that start with a colon.
  struct CommandMapping {
    explicit CommandMapping(const std::vector<std::string>& components)
        : path_components(components) {}

    std::vector<std::string> path_components;
    typedef base::hash_map<WebDriverServer::HttpMethod, DispatchCommandCallback>
        MethodToCommandMap;
    MethodToCommandMap command_map;
  };

  // Helper method to find a CommandMapping for a given path. Based on the
  // MatchStrategy, this could get the CommandMapping for the exact URL path,
  // or it could match variable components as wildcards.
  CommandMapping* GetMappingForPath(const std::vector<std::string>& components,
                                    MatchStrategy strategy);

  // Use the number of components in the registered path as a key to speed up
  // lookup, which is otherwise done linearly.
  typedef base::hash_multimap<int, CommandMapping> CommandMappingLookup;
  CommandMappingLookup command_lookup_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_DISPATCHER_H_
