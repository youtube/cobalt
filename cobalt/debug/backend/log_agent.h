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
#ifndef COBALT_DEBUG_BACKEND_LOG_AGENT_H_
#define COBALT_DEBUG_BACKEND_LOG_AGENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/base/log_message_handler.h"
#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"

namespace cobalt {
namespace debug {
namespace backend {

class LogAgent {
 public:
  explicit LogAgent(DebugDispatcher* dispatcher);
  ~LogAgent();

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

 private:
  void Enable(Command command);
  void Disable(Command command);

  // Called by LogMessageHandler for each log message.
  // May be called from any thread.
  bool OnLogMessage(int severity, const char* file, int line,
                    size_t message_start, const std::string& str);

  // Sends a Log.entryAdded event to debug clients.
  // Must be called on |message_loop_|.
  void OnLogMessageInternal(int severity, const char* file, int line,
                            size_t message_start, const std::string& str);

  // Helper object to connect to the debug dispatcher, etc.
  DebugDispatcher* dispatcher_;

  // Map of member functions implementing commands.
  CommandMap commands_;

  // Indicates whether a client has enabled the "Log" protocol domain.
  bool enabled_;

  // The callback id of our recipient of log messages so we can unregister it.
  base::LogMessageHandler::CallbackId log_message_handler_callback_id_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_LOG_AGENT_H_
