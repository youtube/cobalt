// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/webdriver/protocol/log_entry.h"

#include "base/logging.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
std::string LevelToString(LogEntry::LogLevel level) {
  switch (level) {
    case LogEntry::kDebug:
      return "DEBUG";
    case LogEntry::kInfo:
      return "INFO";
    case LogEntry::kWarning:
      return "WARNING";
    case LogEntry::kSevere:
      return "SEVERE";
  }
  NOTREACHED();
  return "DEBUG";
}
}  // namespace

LogEntry::LogEntry(const base::Time& log_time, LogLevel level,
                   const std::string& message)
    : timestamp_(log_time - base::Time::UnixEpoch()),
      level_(level),
      message_(message) {}

scoped_ptr<base::Value> LogEntry::ToValue(const LogEntry& log_entry) {
  scoped_ptr<base::DictionaryValue> log_entry_value(
      new base::DictionaryValue());
  // Format of the Log Entry object can be found here:
  // https://code.google.com/p/selenium/wiki/JsonWireProtocol#Log_Entry_JSON_Object
  // timestamp is in milliseconds since the Unix Epoch.
  log_entry_value->SetInteger("timestamp",
                              log_entry.timestamp_.InMilliseconds());
  log_entry_value->SetString("level", LevelToString(log_entry.level_));
  log_entry_value->SetString("message", log_entry.message_);
  return log_entry_value.PassAs<base::Value>();
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
