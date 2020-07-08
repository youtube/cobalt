// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_CONSOLE_LOG_H_
#define COBALT_BASE_CONSOLE_LOG_H_

#include <sstream>

#include "base/logging.h"
#include "cobalt/base/debugger_hooks.h"

namespace base {

// This class serves a similar purpose to LogMessage in logging.h, but for the
// CLOG macro that sends the message to the JS console through DebuggerHooks.
class ConsoleMessage {
 public:
  ConsoleMessage(::logging::LogSeverity severity,
                 const DebuggerHooks& debugger_hooks)
      : severity_(severity), debugger_hooks_(debugger_hooks) {}

  ~ConsoleMessage() { debugger_hooks_.ConsoleLog(severity_, stream_.str()); }

  std::ostream& stream() { return stream_; }

 private:
  ::logging::LogSeverity severity_;
  const DebuggerHooks& debugger_hooks_;
  std::ostringstream stream_;
};

#if defined(ENABLE_DEBUGGER)
#define CLOG_IS_ON true
#else
#define CLOG_IS_ON false
#endif

#define CLOG_STREAM(severity, debugger_hooks) \
  ::base::ConsoleMessage(::logging::LOG_##severity, (debugger_hooks)).stream()
#define CLOG(severity, debugger_hooks) \
  LAZY_STREAM(CLOG_STREAM(severity, debugger_hooks), CLOG_IS_ON)
#define CLOG_IF(severity, debugger_hooks, condition) \
  LAZY_STREAM(CLOG_STREAM(severity, debugger_hooks), CLOG_IS_ON && (condition))

}  // namespace base

#endif  // COBALT_BASE_CONSOLE_LOG_H_
