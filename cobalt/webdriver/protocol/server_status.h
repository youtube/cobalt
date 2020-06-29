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

#ifndef COBALT_WEBDRIVER_PROTOCOL_SERVER_STATUS_H_
#define COBALT_WEBDRIVER_PROTOCOL_SERVER_STATUS_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Represents the JSON object that describes the WebDriver server's current
// status in response to the /status command:
// https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#status
class ServerStatus {
 public:
  ServerStatus();

  static std::unique_ptr<base::Value> ToValue(const ServerStatus& status);

 private:
  base::Optional<std::string> os_name_;
  base::Optional<std::string> os_arch_;
  base::Optional<std::string> os_version_;
  std::string build_time_;
  std::string build_version_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_SERVER_STATUS_H_
