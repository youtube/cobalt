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

#include "base/message_loop.h"
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
  explicit Command(const std::string& method, const std::string& json_params,
                   const DebugClient::ResponseCallback& response_callback)
      : method_(method),
        domain_(method_, 0, method_.find('.')),
        json_params_(json_params),
        callback_(response_callback),
        message_loop_(MessageLoop::current()) {}

  const std::string& GetMethod() const { return method_; }
  const std::string& GetDomain() const { return domain_; }
  const std::string& GetParams() const { return json_params_; }

  void SendResponse(const std::string& json_response) const {
    message_loop_->PostTask(FROM_HERE, base::Bind(callback_, json_response));
  }

  void SendResponse(const JSONObject& response) const {
    SendResponse(response ? JSONStringify(response) : "{}");
  }

  void SendResponse() const { SendResponse(JSONObject()); }

  void SendErrorResponse(const std::string& error_message) const {
    JSONObject error_response(new base::DictionaryValue());
    error_response->SetString("error", error_message);
    SendResponse(error_response);
  }

 private:
  std::string method_;
  std::string domain_;
  std::string json_params_;
  DebugClient::ResponseCallback callback_;
  MessageLoop* message_loop_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_COMMAND_H_
