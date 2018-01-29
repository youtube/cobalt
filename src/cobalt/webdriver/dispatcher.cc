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

#include "cobalt/webdriver/dispatcher.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "cobalt/webdriver/protocol/response.h"

namespace cobalt {
namespace webdriver {
namespace {

const char kVariablePrefix = ':';

// CommandResultHandler implementation that sends the results of a
// WebDriver command to a WebDriverServer::ResponseHandler.
class CommandResultHandlerImpl
    : public WebDriverDispatcher::CommandResultHandler {
 public:
  CommandResultHandlerImpl(
      scoped_ptr<WebDriverServer::ResponseHandler> response_handler)
      : response_handler_(response_handler.Pass()) {}

  void SendResult(const base::optional<protocol::SessionId>& session_id,
                  protocol::Response::StatusCode status_code,
                  scoped_ptr<base::Value> webdriver_response_value) override {
    scoped_ptr<base::Value> response = protocol::Response::CreateResponse(
        session_id, status_code, webdriver_response_value.Pass());
    if (status_code == protocol::Response::kSuccess) {
      response_handler_->Success(response.Pass());
    } else {
      response_handler_->FailedCommand(response.Pass());
    }
  }

  void SendInvalidRequestResponse(RequestError error,
                                  const std::string& error_string) override {
    switch (error) {
      case kInvalidParameters:
        response_handler_->MissingCommandParameters(error_string);
        break;
      case kInvalidPathVariable:
        response_handler_->VariableResourceNotFound(error_string);
        break;
    }
  }

 private:
  scoped_ptr<WebDriverServer::ResponseHandler> response_handler_;
};

// Helper function to get all supported methods for a given mapping of
// HttpMethod to DispatchCommandCallback.
typedef base::hash_map<WebDriverServer::HttpMethod,
                       WebDriverDispatcher::DispatchCommandCallback>
    MethodToCommandMap;
std::vector<WebDriverServer::HttpMethod> GetAvailableMethods(
    const MethodToCommandMap& method_lookup) {
  std::vector<WebDriverServer::HttpMethod> available_methods;
  for (MethodToCommandMap::const_iterator it = method_lookup.begin();
       it != method_lookup.end(); ++it) {
    available_methods.push_back(it->first);
  }
  return available_methods;
}

// Populate a PathVariableMapInternal mapping based on the components of a
// registered URL and a requested path.
// The key of the map will be the name of the variable component, and the value
// will be the actual value of the corresponding component in the request.
typedef base::hash_map<std::string, std::string> PathVariableMapInternal;
void PopulatePathVariableMap(PathVariableMapInternal* path_variable_map,
                             const std::vector<std::string>& request,
                             const std::vector<std::string>& path_components) {
  DCHECK_EQ(path_components.size(), request.size());
  for (size_t i = 0; i < request.size(); ++i) {
    DCHECK(!path_components[i].empty());
    // If the path component starts with a colon, it is a variable. Map the
    // variable name (dropping the colon) to the actual value in the request.
    if (path_components[i][0] == kVariablePrefix) {
      (*path_variable_map)[path_components[i]] = request[i];
    }
  }
}

// Tokenize a URL path by component.
std::vector<std::string> TokenizePath(const std::string& path) {
  std::vector<std::string> tokenized_path;
  StringTokenizer tokenizer(path, "/");
  while (tokenizer.GetNext()) {
    tokenized_path.push_back(tokenizer.token());
  }
  return tokenized_path;
}

// Used to compare two path components. If match_exact is set to false,
// then if either path is a variable component (starts with ':'), they will
// trivially match.
class PathComponentsAreEqualPredicate {
 public:
  explicit PathComponentsAreEqualPredicate(bool match_exact)
      : match_exact_(match_exact) {}

  bool operator()(const std::string& lhs, const std::string& rhs) const {
    if (!match_exact_ && (StartsWithColon(lhs) || StartsWithColon(rhs))) {
      return true;
    }
    return lhs == rhs;
  }

 private:
  bool StartsWithColon(const std::string& s) const {
    return !s.empty() && s[0] == kVariablePrefix;
  }
  bool match_exact_;
};

}  // namespace

void WebDriverDispatcher::RegisterCommand(
    WebDriverServer::HttpMethod method, const std::string& path,
    const DispatchCommandCallback& callback) {
  // Break the registered path into components.
  std::vector<std::string> tokenized_path = TokenizePath(path);
  // Find the registered CommandMapping struct if any other methods have been
  // registered for this path yet. Use exact matching so that we don't match
  // any path variables as wildcards.
  CommandMapping* mapping = GetMappingForPath(tokenized_path, kMatchExact);
  if (!mapping) {
    // No commands registered for this path yet, so create a new CommandMapping.
    int tokenized_path_size = static_cast<int>(tokenized_path.size());
    CommandMappingLookup::iterator it = command_lookup_.insert(
        std::make_pair(tokenized_path_size, CommandMapping(tokenized_path)));
    mapping = &it->second;
  }

  // Register this Http method for this CommandMapping.
  CommandMapping::MethodToCommandMap& method_to_command_map =
      mapping->command_map;
  DCHECK(method_to_command_map.end() == method_to_command_map.find(method));
  method_to_command_map[method] = callback;
}

WebDriverDispatcher::CommandMapping* WebDriverDispatcher::GetMappingForPath(
    const std::vector<std::string>& components, MatchStrategy strategy) {
  // Use reverse iterators for the match so we can early-out of a mismatch
  // more quickly. Most requests start with '/session/:sessionId/', for
  // example.
  typedef std::vector<std::string>::const_reverse_iterator MismatchResult;
  typedef std::pair<MismatchResult, MismatchResult> MismatchResultPair;
  typedef std::pair<CommandMappingLookup::iterator,
                    CommandMappingLookup::iterator> EqualRangeResultPair;

  PathComponentsAreEqualPredicate predicate(strategy == kMatchExact);

  // First find all commands that have the same number of components.
  EqualRangeResultPair pair_range =
      command_lookup_.equal_range(components.size());

  // For each command mapping that has the correct number of components,
  // check if the components match the CommandMapping's components.
  for (CommandMappingLookup::iterator it = pair_range.first;
       it != pair_range.second; ++it) {
    DCHECK_EQ(components.size(), it->second.path_components.size());
    // mismatch will return a pair of iterators to the first item in each
    // sequence that is not equal.
    MismatchResultPair result_pair =
        std::mismatch(components.rbegin(), components.rend(),
                      it->second.path_components.rbegin(), predicate);
    if (result_pair.first == components.rend()) {
      DCHECK(
          result_pair.second ==
          static_cast<MismatchResult>(it->second.path_components.rend()));
      return &it->second;
    }
  }
  return NULL;
}

void WebDriverDispatcher::HandleWebDriverServerRequest(
    WebDriverServer::HttpMethod method, const std::string& path,
    scoped_ptr<base::Value> request_value,
    scoped_ptr<WebDriverServer::ResponseHandler> response_handler) {
  // Tokenize the requested resource path and look up a CommandMapping for it,
  // matching variables.
  std::vector<std::string> tokenized_request = TokenizePath(path);
  const CommandMapping* command_mapping =
      GetMappingForPath(tokenized_request, kMatchVariables);

  if (command_mapping == NULL) {
    // No commands were registered to this path.
    response_handler->UnknownCommand(path);
    return;
  }

  CommandMapping::MethodToCommandMap::const_iterator command_it =
      command_mapping->command_map.find(method);
  if (command_it == command_mapping->command_map.end()) {
    // The requested URL maps to a known command, but not with the requested
    // method.
    response_handler->InvalidCommandMethod(
        method, GetAvailableMethods(command_mapping->command_map));
    return;
  }

  // Convert any variable components of the path to a mapping.
  PathVariableMapInternal path_variables;
  PopulatePathVariableMap(&path_variables, tokenized_request,
                          command_mapping->path_components);
  PathVariableMap path_variable_map(path_variables);

  // Create a new CommandResultHandler that will be passed to the Command
  // callback, and run the callback.
  scoped_ptr<CommandResultHandler> result_handler(
      new CommandResultHandlerImpl(response_handler.Pass()));
  command_it->second.Run(request_value.get(), &path_variable_map,
                         result_handler.Pass());
}

}  // namespace webdriver
}  // namespace cobalt
