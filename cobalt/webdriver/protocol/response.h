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

#ifndef WEBDRIVER_PROTOCOL_RESPONSE_H_
#define WEBDRIVER_PROTOCOL_RESPONSE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "cobalt/webdriver/protocol/session_id.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

class Response {
 public:
  // WebDriver Response Status Codes:
  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Response_Status_Codes
  enum StatusCode {
    // The command executed successfully.
    kSuccess = 0,

    // An element could not be located on the page using the given search
    // parameters.
    kNoSuchElement = 7,

    // The requested resource could not be found, or a request was received
    // using an HTTP method that is not supported by the mapped resource.
    kUnknownCommand = 9,

    // An unknown server-side error occurred while processing the command.
    kUnknownError = 13,

    // A new session could not be created.
    kSessionNotCreatedException = 33,
  };

  // Create a JSON object that will be used as the response body for a failed
  // command:
  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Failed_Commands
  // TODO(***REMOVED***): Add support for screenshot, stack trace, etc.
  static scoped_ptr<base::Value> CreateErrorResponse(
      const std::string& message);

  // Create a JSON object that will be used as the response body for a command:
  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Responses
  static scoped_ptr<base::Value> CreateResponse(
      const base::optional<protocol::SessionId>& session_id,
      StatusCode status_code, scoped_ptr<base::Value> webdriver_response_value);
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // WEBDRIVER_PROTOCOL_RESPONSE_H_
