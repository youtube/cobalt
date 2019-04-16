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

#ifndef COBALT_BASE_LOG_MESSAGE_HANDLER_H_
#define COBALT_BASE_LOG_MESSAGE_HANDLER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"

namespace base {

// Provides functionality for any component to receive log messages.
// base::logging supports a single global log message handler.
// This class extends that by implementing a log message handler
// that can relay the log messages to an arbitrary number of callbacks.
//
// Client code that wants to receive log messages should define a callback
// function and pass it to the AddCallback method. The returned CallbackId
// can be used to remove the callback later if necessary.
//
// By default, log messages are still sent to the regular log destinations
// after the callbacks (if any) have been invoked. This can be changed globally
// by calling the SetSuppressLogOutput() method.
// Callbacks can individually suppress messages as well, by returning true
// that they have handled the message.
//
// This class is designed to be a singleton, instanced only through the methods
// of the base::Singleton class. For example,
// LogMessageHandler::GetInstance()->AddCallback(...)
class LogMessageHandler {
 public:
  // Type for callback function. Return true to suppress the message, false
  // to continue dispatch to the default handler.
  typedef Callback<bool(int severity, const char* file, int line,
                        size_t message_start, const std::string& str)>
      OnLogMessageCallback;

  // Type for callback function ID.
  typedef int CallbackId;

  // Method to get the singleton instance of this class.
  static LogMessageHandler* GetInstance();

  // Public methods that operate on the singleton instance.
  CallbackId AddCallback(const OnLogMessageCallback& callback);
  void RemoveCallback(CallbackId callback_id);
  void SetSuppressLogOutput(bool suppress_log_output);
  bool GetSuppressLogOutput();

 private:
  // Type for map of callbacks.
  typedef std::map<LogMessageHandler::CallbackId,
                   LogMessageHandler::OnLogMessageCallback> CallbackMap;

  // This class should only be instanced by the base::Singleton.
  LogMessageHandler();
  ~LogMessageHandler();

  // The global message handler function used by this class.
  static bool OnLogMessage(int severity, const char* file, int line,
                           size_t message_start, const std::string& str);

  friend struct base::StaticMemorySingletonTraits<LogMessageHandler>;

  // The previous global log message handler, restored when the singleton
  // instance is destroyed.
  logging::LogMessageHandlerFunction old_log_message_handler_;

  // Map used to store the registered callbacks.
  // This map is expected to be relatively small, but is still useful
  // to simplify the code and provide somewhat more thread-safe access.
  CallbackMap callbacks_;

  base::Lock lock_;

  CallbackId next_callback_id_;

  bool suppress_log_output_;

  DISALLOW_COPY_AND_ASSIGN(LogMessageHandler);
};

}  // namespace base

#endif  // COBALT_BASE_LOG_MESSAGE_HANDLER_H_
