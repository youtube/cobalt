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

#include "cobalt/base/log_message_handler.h"

namespace base {

LogMessageHandler* LogMessageHandler::GetInstance() {
  return Singleton<LogMessageHandler,
                   StaticMemorySingletonTraits<LogMessageHandler>>::get();
}

LogMessageHandler::LogMessageHandler() {
  AutoLock auto_lock(lock_);
  old_log_message_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(OnLogMessage);
}

LogMessageHandler::~LogMessageHandler() {
  AutoLock auto_lock(lock_);
  logging::SetLogMessageHandler(old_log_message_handler_);
}

LogMessageHandler::CallbackId LogMessageHandler::AddCallback(
    const OnLogMessageCallback& callback) {
  AutoLock auto_lock(lock_);
  const CallbackId callback_id = next_callback_id_++;
  callbacks_[callback_id] = callback;
  return callback_id;
}

void LogMessageHandler::RemoveCallback(CallbackId callback_id) {
  AutoLock auto_lock(lock_);
  callbacks_.erase(callback_id);
}

bool LogMessageHandler::OnLogMessage(int severity, const char* file, int line,
                                     size_t message_start,
                                     const std::string& str) {
  LogMessageHandler* instance = GetInstance();
  AutoLock auto_lock(instance->lock_);

  for (CallbackMap::const_iterator it = instance->callbacks_.begin();
       it != instance->callbacks_.end(); ++it) {
    it->second.Run(severity, file, line, message_start, str);
  }

  return instance->suppress_log_output_;
}

void LogMessageHandler::SetSuppressLogOutput(bool suppress_log_output) {
  suppress_log_output_ = suppress_log_output;
}

bool LogMessageHandler::GetSuppressLogOutput() { return suppress_log_output_; }

}  // namespace base
