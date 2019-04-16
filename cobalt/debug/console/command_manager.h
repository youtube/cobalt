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

#ifndef COBALT_DEBUG_CONSOLE_COMMAND_MANAGER_H_
#define COBALT_DEBUG_CONSOLE_COMMAND_MANAGER_H_

#if !defined(ENABLE_DEBUGGER)
#error "Debugger is not enabled in this build."
#endif  // !ENABLE_DEBUGGER

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"

namespace cobalt {
namespace debug {
namespace console {

// The console command system allows the app to register actions that can be
// triggered by the debug console.
//
// Any component can create a handler object of the CommandHandler class to
// listen for commands. A CommandHandler object automatically registers itself
// with the ConsoleCommandManager singleton on construction and unregisters
// itself on destruction.
//
// Each CommandHandler object specifies a callback and a help string. The
// ConsoleCommandManager handles a particular command by running the callback of
// the CommandHandler object registered for that command. The help string is
// used within the debug console to document the registered commands.

class ConsoleCommandManager {
 public:
  // Type for a callback to handle a command.
  typedef base::Callback<void(const std::string& message)> CommandCallback;

  // Command handler that registers itself with this object.
  class CommandHandler {
   public:
    CommandHandler(const std::string& command, const CommandCallback& callback,
                   const std::string& short_help, const std::string& long_help);
    ~CommandHandler();

    const std::string& command() const { return command_; }
    const CommandCallback& callback() const { return callback_; }
    const std::string& short_help() const { return short_help_; }
    const std::string& long_help() const { return long_help_; }

   private:
    std::string command_;
    CommandCallback callback_;
    std::string short_help_;
    std::string long_help_;
  };

  friend struct base::StaticMemorySingletonTraits<ConsoleCommandManager>;

  // Method to get the singleton instance of this class.
  static ConsoleCommandManager* GetInstance();

  // Handles a command by posting the message to the handler registered for
  // the specified command, if any.
  void HandleCommand(const std::string& command,
                     const std::string& message) const;

  // Returns a set of all the currently registered commands.
  std::set<std::string> GetRegisteredCommands() const;

  // Returns the help strings for a command.
  std::string GetShortHelp(const std::string& command) const;
  std::string GetLongHelp(const std::string& command) const;

 private:
  // Class can only be instanced via the singleton
  ConsoleCommandManager() {}
  ~ConsoleCommandManager() {}

  // Command handler map type.
  typedef std::map<std::string, const CommandHandler*> CommandHandlerMap;

  // Methods to register/unregister command handlers.
  // These are intended only to be called from the command handler objects.
  friend class CommandHandler;
  void RegisterCommandHandler(const CommandHandler* handler);
  void UnregisterCommandHandler(const CommandHandler* handler);

  mutable base::Lock lock_;

  // Map of command handlers, one for each command.
  CommandHandlerMap command_command_map_;
};

}  // namespace console
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_CONSOLE_COMMAND_MANAGER_H_
