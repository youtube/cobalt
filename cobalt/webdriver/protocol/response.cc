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

#include "cobalt/webdriver/protocol/response.h"

#include <memory>
#include <utility>

namespace cobalt {
namespace webdriver {
namespace protocol {

std::unique_ptr<base::Value> Response::CreateErrorResponse(
    const std::string& message) {
  base::Value ret(base::Value::Type::DICT);
  base::Value::Dict* error_value = ret->GetIfDict();
  error_value->Set("message", message);
  return base::Value::ToUniquePtrValue(std::move(ret));
}

std::unique_ptr<base::Value> Response::CreateResponse(
    const base::Optional<protocol::SessionId>& session_id,
    StatusCode status_code,
    std::unique_ptr<base::Value> webdriver_response_value) {
  base::Value ret(base::Value::Type::DICT);
  base::Value::Dict* http_response_value = ret->GetIfDict();
  if (session_id) {
    http_response_value->Set(
        "sessionId", protocol::SessionId::ToValue(session_id.value())->Clone());
  } else {
    http_response_value->Set("sessionId", base::Value());
  }
  http_response_value->Set("status", status_code);
  if (webdriver_response_value) {
    http_response_value->Set("value", webdriver_response_value->Clone());
  } else {
    http_response_value->Set("value", base::Value());
  }
  return base::Value::ToUniquePtrValue(std::move(ret));
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
