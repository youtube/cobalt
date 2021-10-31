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

#ifndef COBALT_WEBDRIVER_PROTOCOL_SESSION_ID_H_
#define COBALT_WEBDRIVER_PROTOCOL_SESSION_ID_H_

#include <memory>
#include <string>

#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// sessionId is mentioned in the spec describing WebDriver responses:
// https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Responses
//
class SessionId {
 public:
  static std::unique_ptr<base::Value> ToValue(const SessionId& session_id) {
    return std::make_unique<base::Value>(session_id.id_);
  }

  explicit SessionId(const std::string& id) : id_(id) {}
  const std::string& id() const { return id_; }
  bool operator==(const SessionId& rhs) const { return id_ == rhs.id_; }

 private:
  std::string id_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_SESSION_ID_H_
