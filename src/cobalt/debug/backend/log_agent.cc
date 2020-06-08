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

#include "cobalt/debug/backend/log_agent.h"

#include "base/logging.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {
// Error levels:
constexpr char kInfoLevel[] = "info";
constexpr char kWarningLevel[] = "warning";
constexpr char kErrorLevel[] = "error";

const char* GetLogLevelFromSeverity(int severity) {
  switch (severity) {
    // LOG_VERBOSE is a pseudo-severity, which we never get here.
    case logging::LOG_INFO:
      return kInfoLevel;
    case logging::LOG_WARNING:
      return kWarningLevel;
    case logging::LOG_ERROR:
    case logging::LOG_FATAL:
      return kErrorLevel;
  }
  NOTREACHED();
  return "";
}
}  // namespace

LogAgent::LogAgent(DebugDispatcher* dispatcher) : AgentBase("Log", dispatcher) {
  // Get log output while still making it available elsewhere.
  log_message_handler_callback_id_ =
      base::LogMessageHandler::GetInstance()->AddCallback(
          base::Bind(&LogAgent::OnLogMessage, base::Unretained(this)));
}

LogAgent::~LogAgent() {
  base::LogMessageHandler::GetInstance()->RemoveCallback(
      log_message_handler_callback_id_);
}

bool LogAgent::OnLogMessage(int severity, const char* file, int line,
                            size_t message_start, const std::string& str) {
  DCHECK(this);

  if (enabled_) {
    // Our custom "Log.browserEntryAdded" event is just like "Log.entryAdded"
    // except it only shows up in the debug console and not in remote devtools.
    // TODO: Flesh out the rest of LogEntry properties (source, timestamp)
    JSONObject params(new base::DictionaryValue());
    params->SetString("entry.text", str);
    params->SetString("entry.level", GetLogLevelFromSeverity(severity));
    dispatcher_->SendEvent(domain_ + ".browserEntryAdded", params);
  }

  // Don't suppress the log message.
  return false;
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
