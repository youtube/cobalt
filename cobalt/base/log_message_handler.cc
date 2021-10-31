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

#include "cobalt/base/log_message_handler.h"

#include "base/threading/thread_restrictions.h"
#include "starboard/thread.h"

namespace base {

namespace {
// Checks whether this thread allows base::Singleton access. Some threads
// (e.g. detached threads, non-joinable threads) do not allow Singleton
// access, which means we cannot access our |LogMessageHandler| instance,
// nor even call |base::MessageLoop::current|.
bool DoesThreadAllowSingletons() {
  return ThreadRestrictions::GetSingletonAllowed();
}
}  // namespace

LogMessageHandler* LogMessageHandler::GetInstance() {
  return base::Singleton<LogMessageHandler, base::StaticMemorySingletonTraits<
                                                LogMessageHandler> >::get();
}

LogMessageHandler::LogMessageHandler() {
  // Create the lock used to allow thread-safe checking of the thread.
  // Set the global log message handler to our static member function.
  old_log_message_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(OnLogMessage);
}

LogMessageHandler::~LogMessageHandler() {
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

// static
bool LogMessageHandler::OnLogMessage(int severity, const char* file, int line,
                                     size_t message_start,
                                     const std::string& str) {
  // If we are on a thread that doesn't support Singletons, we can't do
  // anything.
  if (!DoesThreadAllowSingletons()) {
    return false;
  }

  // Ignore recursive calls.
  static SbThreadId logging_thread = kSbThreadInvalidId;
  if (logging_thread == SbThreadGetId()) {
    return false;
  }

  LogMessageHandler* instance = GetInstance();
  AutoLock auto_lock(instance->lock_);
  logging_thread = SbThreadGetId();

  bool suppress = instance->suppress_log_output_;
  for (CallbackMap::const_iterator it = instance->callbacks_.begin();
       it != instance->callbacks_.end(); ++it) {
    if (it->second.Run(severity, file, line, message_start, str)) {
      suppress = true;
    }
  }

  logging_thread = kSbThreadInvalidId;
  return suppress;
}

void LogMessageHandler::SetSuppressLogOutput(bool suppress_log_output) {
  suppress_log_output_ = suppress_log_output;
}

bool LogMessageHandler::GetSuppressLogOutput() { return suppress_log_output_; }

}  // namespace base
