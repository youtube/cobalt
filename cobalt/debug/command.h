// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_DEBUG_COMMAND_H_
#define COBALT_DEBUG_COMMAND_H_

#include <string>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/debug/debug_client.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {

// Holds a protocol command being processed for a particular client. The command
// method and params are defined here:
// https://chromedevtools.github.io/devtools-protocol/1-3
//
// A reference is kept to the client's callback and the message loop from which
// it was sent so the command handler can send back its response through this
// class.
class Command {
 public:
  // Matches v8_inspector::protocol::DispatchResponse::ErrorCode
  enum ErrorCode {
    kParseError = -32700,
    kInvalidRequest = -32600,
    kMethodNotFound = -32601,
    kInvalidParams = -32602,
    kInternalError = -32603,
    kServerError = -32000,
  };

  Command(const std::string& method, const std::string& json_params,
          const DebugClient::ResponseCallback& response_callback)
      : method_(method),
        domain_(method_, 0, method_.find('.')),
        json_params_(json_params),
        callback_(response_callback),
        task_runner_(base::MessageLoop::current()->task_runner()),
        response_sent_(false) {
    DCHECK(!method_.empty());
    DCHECK(!domain_.empty());
    DCHECK(!callback_.is_null());
  }

  // Not copyable.
  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

  // Movable.
  Command(Command&&) = default;
  Command& operator=(Command&&) = default;

  ~Command() {
    // A response must be sent for all commands, except for the residual null
    // objects that have been moved-from.
    DCHECK(response_sent_ || is_null()) << "No response sent for " << method_;
  }

  // Returns true if this instance has been moved-from and is no longer active.
  bool is_null() {
    // We use the default move constructor, and rely on callback's is_null()
    // to detect whether it has been moved-from. A DCHECK in our constructor
    // ensures that only moved-from callbacks are null.
    return callback_.is_null();
  }

  const std::string& GetMethod() const { return method_; }
  const std::string& GetDomain() const { return domain_; }
  const std::string& GetParams() const { return json_params_; }

  void SendResponse(const std::string& json_response) {
    DCHECK(!is_null()) << "Sending response for null Command";
    DCHECK(!response_sent_) << "Response already sent for " << method_;
    response_sent_ = true;
    task_runner_->PostTask(FROM_HERE, base::Bind(callback_, json_response));
  }

  void SendResponse(const JSONObject& response) {
    SendResponse(JSONStringify(response));
  }

  void SendResponse() { SendResponse(JSONObject()); }

  void SendErrorResponse(ErrorCode error_code,
                         const std::string& error_message) {
    JSONObject error_response(new base::DictionaryValue());
    error_response->SetInteger("error.code", error_code);
    error_response->SetString("error.message", error_message);
    SendResponse(error_response);
  }

  // Constructs a Command with a no-op response callback.
  static Command IgnoreResponse(const std::string& method,
                                const std::string& params = "") {
    return Command(
        method, params,
        base::Bind([](const base::Optional<std::string>& response) {}));
  }

 private:
  std::string method_;
  std::string domain_;
  std::string json_params_;
  DebugClient::ResponseCallback callback_;
  base::SingleThreadTaskRunner* task_runner_;
  bool response_sent_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_COMMAND_H_
