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

#include "cobalt/debug/console/command_manager.h"

#include "base/logging.h"
#include "starboard/string.h"

namespace cobalt {
namespace debug {
namespace console {

ConsoleCommandManager* ConsoleCommandManager::GetInstance() {
  return base::Singleton<
      ConsoleCommandManager,
      base::StaticMemorySingletonTraits<ConsoleCommandManager> >::get();
}

ConsoleCommandManager::CommandHandler::CommandHandler(
    const std::string& command,
    const ConsoleCommandManager::CommandCallback& callback,
    const std::string& short_help, const std::string& long_help)
    : command_(command),
      callback_(callback),
      short_help_(short_help),
      long_help_(long_help) {
  ConsoleCommandManager* manager = ConsoleCommandManager::GetInstance();
  DCHECK(manager);
  manager->RegisterCommandHandler(this);
}

ConsoleCommandManager::CommandHandler::~CommandHandler() {
  ConsoleCommandManager* manager = ConsoleCommandManager::GetInstance();
  DCHECK(manager);
  manager->UnregisterCommandHandler(this);
}

// Returns true if the message is 'on', 'enable', or 'true'.
// static
bool ConsoleCommandManager::CommandHandler::IsOnEnableOrTrue(
    const std::string& message) {
  return (strcasecmp("on", message.c_str()) == 0) ||
         (strcasecmp("enable", message.c_str()) == 0) ||
         (strcasecmp("true", message.c_str()) == 0);
}


void ConsoleCommandManager::HandleCommand(const std::string& command,
                                          const std::string& message) const {
  DCHECK_GT(command.length(), size_t(0));
  base::AutoLock auto_lock(lock_);
  CommandHandlerMap::const_iterator iter = command_command_map_.find(command);
  if (iter != command_command_map_.end()) {
    iter->second->callback().Run(message);
  } else {
    DLOG(WARNING) << "No handler registered for command: " << command;
  }
}

std::set<std::string> ConsoleCommandManager::GetRegisteredCommands() const {
  std::set<std::string> result;
  base::AutoLock auto_lock(lock_);
  for (CommandHandlerMap::const_iterator iter = command_command_map_.begin();
       iter != command_command_map_.end(); ++iter) {
    result.insert(iter->first);
  }
  return result;
}

std::string ConsoleCommandManager::GetShortHelp(
    const std::string& command) const {
  base::AutoLock auto_lock(lock_);
  CommandHandlerMap::const_iterator iter = command_command_map_.find(command);
  if (iter != command_command_map_.end()) {
    return iter->second->short_help();
  }
  return "No help available for unregistered command: " + command;
}

std::string ConsoleCommandManager::GetLongHelp(
    const std::string& command) const {
  base::AutoLock auto_lock(lock_);
  CommandHandlerMap::const_iterator iter = command_command_map_.find(command);
  if (iter != command_command_map_.end()) {
    return iter->second->long_help();
  }
  return "No help available for unregistered command: " + command;
}

void ConsoleCommandManager::RegisterCommandHandler(
    const CommandHandler* handler) {
  DCHECK_GT(handler->command().length(), size_t(0));
  base::AutoLock auto_lock(lock_);
  command_command_map_[handler->command()] = handler;
}

void ConsoleCommandManager::UnregisterCommandHandler(
    const CommandHandler* handler) {
  DCHECK_GT(handler->command().length(), size_t(0));
  base::AutoLock auto_lock(lock_);
  command_command_map_.erase(handler->command());
}

}  // namespace console
}  // namespace debug
}  // namespace cobalt
