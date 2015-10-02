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

#include "cobalt/debug/debug_hub.h"

#include "cobalt/base/console_values.h"
#include "cobalt/base/source_location.h"

namespace cobalt {
namespace debug {

#if defined(ENABLE_DEBUG_CONSOLE)

DebugHub::DebugHub(const ExecuteJavascriptCallback& execute_javascript_callback)
    : execute_javascript_callback_(execute_javascript_callback),
      next_log_message_callback_id_(0),
      debug_console_mode_(kDebugConsoleHud) {
  // Get log output while still making it available elsewhere.
  const base::LogMessageHandler::OnLogMessageCallback on_log_message_callback =
      base::Bind(&DebugHub::OnLogMessage, base::Unretained(this));
  log_message_handler_callback_id_ =
      base::LogMessageHandler::GetInstance()->AddCallback(
          on_log_message_callback);
}

DebugHub::~DebugHub() {
  base::LogMessageHandler::GetInstance()->RemoveCallback(
      log_message_handler_callback_id_);
}

void DebugHub::OnLogMessage(int severity, const char* file, int line,
                            size_t message_start, const std::string& str) {
  // Use a lock here and in the other methods, as callbacks may be added by
  // multiple web modules on different threads, and log messages may be
  // be generated on other threads.
  base::AutoLock auto_lock(lock_);

  for (LogMessageCallbacks::const_iterator it = log_message_callbacks_.begin();
       it != log_message_callbacks_.end(); ++it) {
    const LogMessageCallbackInfo& callbackInfo = it->second;
    const scoped_refptr<LogMessageCallback>& callback = callbackInfo.callback;
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy =
        callbackInfo.message_loop_proxy;

    message_loop_proxy->PostTask(
        FROM_HERE, base::Bind(&LogMessageCallback::Run, callback, severity,
                              file, line, message_start, str));
  }
}

int DebugHub::AddLogMessageCallback(
    const scoped_refptr<LogMessageCallback>& callback) {
  base::AutoLock auto_lock(lock_);
  const int callback_id = next_log_message_callback_id_++;
  LogMessageCallbackInfo& callbackInfo = log_message_callbacks_[callback_id];
  callbackInfo.callback = callback;
  callbackInfo.message_loop_proxy =
      MessageLoop::current()->message_loop_proxy();
  return callback_id;
}

void DebugHub::RemoveLogMessageCallback(int callback_id) {
  base::AutoLock auto_lock(lock_);
  log_message_callbacks_.erase(callback_id);
}

// TODO(***REMOVED***) - This function should be modified to return an array of
// strings instead of a single space-separated string, once the bindings
// support return of a string array.
std::string DebugHub::GetConsoleValueNames() const {
  std::string ret = "";
  base::ConsoleValueManager* cvm = base::ConsoleValueManager::GetInstance();
  DCHECK(cvm);

  if (cvm) {
    std::set<std::string> names = cvm->GetOrderedCValNames();
    for (std::set<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it) {
      ret += (*it);
      std::set<std::string>::const_iterator next = it;
      ++next;
      if (next != names.end()) {
        ret += " ";
      }
    }
  }
  return ret;
}

std::string DebugHub::GetConsoleValue(const std::string& name) const {
  std::string ret = "<undefined>";
  base::ConsoleValueManager* cvm = base::ConsoleValueManager::GetInstance();
  DCHECK(cvm);

  if (cvm) {
    base::ConsoleValueManager::ValueQueryResults result =
        cvm->GetValueAsPrettyString(name);
    if (result.valid) {
      ret = result.value;
    }
  }
  return ret;
}

int DebugHub::GetDebugConsoleMode() const { return debug_console_mode_; }

void DebugHub::SetDebugConsoleMode(int debug_console_mode) {
  debug_console_mode_ = debug_console_mode;
}

int DebugHub::CycleDebugConsoleMode() {
  debug_console_mode_ = (debug_console_mode_ + 1) % kDebugConsoleNumModes;
  return debug_console_mode_;
}

void DebugHub::ExecuteCommand(const std::string& command) {
  // TODO(***REMOVED***) The command string should first be checked to see if it
  // matches any of a set of known commands to be executed locally, and only
  // interpreted as Javascript if it is not a known command.
  execute_javascript_callback_.Run(
      command, base::SourceLocation("[object DebugHub]", 1, 1));
}

#else   // ENABLE_DEBUG_CONSOLE

// Stub implementation when debug not enabled (release builds)

DebugHub::DebugHub(
    const ExecuteJavascriptCallback& execute_javascript_callback) {
  UNREFERENCED_PARAMETER(execute_javascript_callback);
}

DebugHub::~DebugHub() {}

int DebugHub::AddLogMessageCallback(
    const scoped_refptr<LogMessageCallback>& callback) {
  UNREFERENCED_PARAMETER(callback);
  return 0;
}

void DebugHub::RemoveLogMessageCallback(int callback_id) {
  UNREFERENCED_PARAMETER(callback_id);
}

std::string DebugHub::GetConsoleValueNames() const { return ""; }

std::string DebugHub::GetConsoleValue(const std::string& name) const {
  UNREFERENCED_PARAMETER(name);
  return "";
}

void DebugHub::SetDebugConsoleMode(int debug_console_mode) {
  UNREFERENCED_PARAMETER(debug_console_mode);
}

int DebugHub::CycleDebugConsoleMode() { return kDebugConsoleOff; }

int DebugHub::GetDebugConsoleMode() const { return kDebugConsoleOff; }

void DebugHub::ExecuteCommand(const std::string& command) {
  UNREFERENCED_PARAMETER(command);
}
#endif  // ENABLE_DEBUG_CONSOLE

}  // namespace debug
}  // namespace cobalt
