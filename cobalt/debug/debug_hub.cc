// Copyright 2015 Google Inc. All Rights Reserved.
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

#if defined(ENABLE_DEBUG_CONSOLE)

#include "cobalt/debug/debug_hub.h"

#include <set>

#include "base/compiler_specific.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/console_commands.h"
#include "cobalt/base/source_location.h"

namespace cobalt {
namespace debug {

namespace {
class ScopedSetter {
 public:
  explicit ScopedSetter(bool* to_set) : to_set_(to_set) {
    DCHECK(to_set_);
    *to_set_ = true;
  }
  ~ScopedSetter() { *to_set_ = false; }

 private:
  bool* to_set_;
};
}  // namespace

DebugHub::DebugHub(
    const GetHudModeCallback& get_hud_mode_callback,
    const Debugger::GetDebugServerCallback& get_debug_server_callback)
    : message_loop_(MessageLoop::current()),
      get_hud_mode_callback_(get_hud_mode_callback),
      debugger_(new Debugger(get_debug_server_callback)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_ptr_(weak_ptr_factory_.GetWeakPtr()),
      is_logging_(false) {
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

void DebugHub::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(debugger_);
}

bool DebugHub::OnLogMessage(int severity, const char* file, int line,
                            size_t message_start, const std::string& str) {
  // Don't run recursively.
  if (MessageLoop::current() == message_loop_ && is_logging_) {
    return false;
  }

  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&DebugHub::OnLogMessageInternal, weak_ptr_,
                                     severity, file, line, message_start, str));

  // Don't suppress the log message.
  return false;
}

void DebugHub::OnLogMessageInternal(int severity, const char* file, int line,
                                    size_t message_start,
                                    const std::string& str) {
  DCHECK(this);
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (log_message_callback_) {
    ScopedSetter scoped_setter(&is_logging_);
    log_message_callback_->value().Run(severity, file, line, message_start,
                                       str);
  }
}

void DebugHub::SetLogMessageCallback(const LogMessageCallbackArg& callback) {
  DCHECK(MessageLoop::current() == message_loop_);
  log_message_callback_.reset(
      new LogMessageCallbackArg::Reference(this, callback));
}

// TODO: This function should be modified to return an array of strings instead
// of a single space-separated string, once the bindings support return of a
// string array.
std::string DebugHub::GetConsoleValueNames() const {
  std::string ret = "";
  base::CValManager* cvm = base::CValManager::GetInstance();
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
  base::CValManager* cvm = base::CValManager::GetInstance();
  DCHECK(cvm);

  if (cvm) {
    base::optional<std::string> result = cvm->GetValueAsPrettyString(name);
    if (result) {
      return *result;
    }
  }
  return ret;
}

int DebugHub::GetDebugConsoleMode() const {
  return get_hud_mode_callback_.Run();
}

// TODO: This function should be modified to return an array of strings instead
// of a single space-separated string, once the bindings support return of a
// string array.
std::string DebugHub::GetCommandChannels() const {
  std::string result = "";
  base::ConsoleCommandManager* command_mananger =
      base::ConsoleCommandManager::GetInstance();
  DCHECK(command_mananger);

  if (command_mananger) {
    std::set<std::string> channels = command_mananger->GetRegisteredChannels();
    for (std::set<std::string>::const_iterator it = channels.begin();
         it != channels.end(); ++it) {
      result += (*it);
      std::set<std::string>::const_iterator next = it;
      ++next;
      if (next != channels.end()) {
        result += " ";
      }
    }
  }
  return result;
}

std::string DebugHub::GetCommandChannelShortHelp(
    const std::string& channel) const {
  std::string result = "<undefined>";
  base::ConsoleCommandManager* command_mananger =
      base::ConsoleCommandManager::GetInstance();
  DCHECK(command_mananger);
  if (command_mananger) {
    result = command_mananger->GetShortHelp(channel);
  }
  return result;
}

std::string DebugHub::GetCommandChannelLongHelp(
    const std::string& channel) const {
  std::string result = "<undefined>";
  base::ConsoleCommandManager* command_mananger =
      base::ConsoleCommandManager::GetInstance();
  DCHECK(command_mananger);
  if (command_mananger) {
    result = command_mananger->GetLongHelp(channel);
  }
  return result;
}

void DebugHub::SendCommand(const std::string& channel,
                           const std::string& message) {
  base::ConsoleCommandManager* console_command_manager =
      base::ConsoleCommandManager::GetInstance();
  DCHECK(console_command_manager);
  console_command_manager->HandleCommand(channel, message);
}

}  // namespace debug
}  // namespace cobalt

#endif  // ENABLE_DEBUG_CONSOLE
