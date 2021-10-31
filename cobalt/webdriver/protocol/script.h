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

#ifndef COBALT_WEBDRIVER_PROTOCOL_SCRIPT_H_
#define COBALT_WEBDRIVER_PROTOCOL_SCRIPT_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Represents the JSON parameters passed to the execute and execute_async
// WebDriver commands.
// https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#sessionsessionidexecute
// https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#sessionsessionidexecute-async
class Script {
 public:
  static base::Optional<Script> FromValue(const base::Value* script);
  const std::string& function_body() const { return function_body_; }
  const std::string& argument_array() const { return argument_array_; }

 private:
  Script(const std::string& function_body, const std::string& argument_array)
      : function_body_(function_body), argument_array_(argument_array) {}
  std::string function_body_;
  std::string argument_array_;
};

// The result from script execution could be any JSON object. This will be
// returned as a JSON string.
class ScriptResult {
 public:
  static std::unique_ptr<base::Value> ToValue(
      const ScriptResult& script_result);

  explicit ScriptResult(const std::string& result_string)
      : result_string_(result_string) {}

 private:
  std::string result_string_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_SCRIPT_H_
