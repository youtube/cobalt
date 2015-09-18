/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/webdriver/protocol/response.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

scoped_ptr<base::Value> Response::CreateErrorResponse(
    const std::string& message) {
  scoped_ptr<base::DictionaryValue> error_value(new base::DictionaryValue());
  error_value->SetString("message", message);
  return error_value.PassAs<base::Value>();
}

scoped_ptr<base::Value> Response::CreateResponse(
    const base::optional<protocol::SessionId>& session_id,
    StatusCode status_code, scoped_ptr<base::Value> webdriver_response_value) {
  scoped_ptr<base::DictionaryValue> http_response_value(
      new base::DictionaryValue());
  if (session_id) {
    http_response_value->Set(
        "sessionId",
        protocol::SessionId::ToValue(session_id.value()).release());
  } else {
    http_response_value->Set("sessionId", base::Value::CreateNullValue());
  }
  http_response_value->SetInteger("status", status_code);
  if (webdriver_response_value) {
    http_response_value->Set("value", webdriver_response_value.release());
  } else {
    http_response_value->Set("value", base::Value::CreateNullValue());
  }
  return http_response_value.PassAs<base::Value>();
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
