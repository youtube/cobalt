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

#ifndef COBALT_BASE_CONSOLE_COMMANDS_H_
#define COBALT_BASE_CONSOLE_COMMANDS_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"

namespace base {

// The console command system allows the app to register actions that can be
// triggered by the debug console.
//
// Any component can create a handler object of the CommandHandler class to
// listen for commands on a particular channel. A CommandHandler object
// automatically registers itself with the ConsoleCommandManager singleton on
// construction and unregisters itself on destruction.
//
// Each CommandHandler object specifies a callback and a help string. When the
// ConsoleCommandManager receives a command broadcast on a particular channel,
// it runs the callback of the CommandHandler object registered to that
// channel. The help string is used within the debug console to document the
// channels that have been registered to listen for commands.

class ConsoleCommandManager {
 public:
  // Type for a callback to handle a command.
  typedef base::Callback<void(const std::string& message)> CommandCallback;

  // Command handler that registers itself with this object.
  class CommandHandler {
   public:
    CommandHandler(const std::string& channel, const CommandCallback& callback,
                   const std::string& short_help, const std::string& long_help);
    ~CommandHandler();

    const std::string& channel() const { return channel_; }
    const CommandCallback& callback() const { return callback_; }
    const std::string& short_help() const { return short_help_; }
    const std::string& long_help() const { return long_help_; }

   private:
    std::string channel_;
    CommandCallback callback_;
    std::string short_help_;
    std::string long_help_;
  };

  friend struct StaticMemorySingletonTraits<ConsoleCommandManager>;

  // Method to get the singleton instance of this class.
  static ConsoleCommandManager* GetInstance();

  // Handles a command by posting the message to the handler registered for
  // the specified channel, if any.
  void HandleCommand(const std::string& channel,
                     const std::string& message) const;

  // Returns a set of all the currently registered channels.
  std::set<std::string> GetRegisteredChannels() const;

  // Returns the help strings for a channel.
  std::string GetShortHelp(const std::string& channel) const;
  std::string GetLongHelp(const std::string& channel) const;

 private:
  // Class can only be instanced via the singleton
  ConsoleCommandManager() {}
  ~ConsoleCommandManager() {}

#if defined(ENABLE_DEBUG_CONSOLE)
  // Command handler map type.
  typedef std::multimap<std::string, const CommandHandler*> CommandHandlerMap;

  // Methods to register/unregister command handlers.
  // These are intended only to be called from the command handler objects.
  friend class CommandHandler;
  void RegisterCommandHandler(const CommandHandler* handler);
  void UnregisterCommandHandler(const CommandHandler* handler);

  mutable base::Lock lock_;

  // Map of command handlers, one for each channel.
  CommandHandlerMap command_channel_map_;
#endif  // ENABLE_DEBUG_CONSOLE
};

}  // namespace base

#endif  // COBALT_BASE_CONSOLE_COMMANDS_H_
