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

namespace cobalt {
namespace debug {

DebugHub::DebugHub() : next_log_message_callback_id_(0) {
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

}  // namespace debug
}  // namespace cobalt
