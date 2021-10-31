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

#include <memory>

#include "cobalt/webdriver/protocol/response.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

std::unique_ptr<base::Value> Response::CreateErrorResponse(
    const std::string& message) {
  std::unique_ptr<base::DictionaryValue> error_value(
      new base::DictionaryValue());
  error_value->SetString("message", message);
  return std::unique_ptr<base::Value>(error_value.release());
}

std::unique_ptr<base::Value> Response::CreateResponse(
    const base::Optional<protocol::SessionId>& session_id,
    StatusCode status_code,
    std::unique_ptr<base::Value> webdriver_response_value) {
  std::unique_ptr<base::DictionaryValue> http_response_value(
      new base::DictionaryValue());
  if (session_id) {
    http_response_value->Set(
        "sessionId",
        std::move(protocol::SessionId::ToValue(session_id.value())));
  } else {
    http_response_value->Set("sessionId", std::make_unique<base::Value>());
  }
  http_response_value->SetInteger("status", status_code);
  if (webdriver_response_value) {
    http_response_value->Set("value", std::move(webdriver_response_value));
  } else {
    http_response_value->Set("value", std::make_unique<base::Value>());
  }
  return std::unique_ptr<base::Value>(std::move(http_response_value));
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
