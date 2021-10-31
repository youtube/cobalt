// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_PROTOCOL_LOG_ENTRY_H_
#define COBALT_WEBDRIVER_PROTOCOL_LOG_ENTRY_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// Log entry object:
// https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#Log-Entry-JSON-Object
class LogEntry {
 public:
  enum LogLevel {
    kDebug,
    kInfo,
    kWarning,
    kSevere,
  };
  static std::unique_ptr<base::Value> ToValue(const LogEntry& log_entry);
  LogEntry(const base::Time& log_time, LogLevel level,
           const std::string& message);

 private:
  base::TimeDelta timestamp_;
  LogLevel level_;
  std::string message_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_LOG_ENTRY_H_
