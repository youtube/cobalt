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

#include "cobalt/base/console_commands.h"

#include "base/logging.h"

namespace base {

ConsoleCommandManager* ConsoleCommandManager::GetInstance() {
  return Singleton<ConsoleCommandManager,
                   StaticMemorySingletonTraits<ConsoleCommandManager> >::get();
}

#if defined(ENABLE_DEBUG_CONSOLE)

ConsoleCommandManager::CommandHandler::CommandHandler(
    const std::string& channel,
    const ConsoleCommandManager::CommandCallback& callback,
    const std::string& short_help, const std::string& long_help)
    : channel_(channel),
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

void ConsoleCommandManager::HandleCommand(const std::string& channel,
                                          const std::string& message) {
  DCHECK_GT(channel.length(), size_t(0));
  base::AutoLock auto_lock(lock_);
  CommandHandlerMap::const_iterator iter = command_channel_map_.find(channel);
  if (iter != command_channel_map_.end()) {
    iter->second->callback().Run(message);
  } else {
    DLOG(WARNING) << "No command handler registered for channel: " << channel;
  }
}

std::set<std::string> ConsoleCommandManager::GetRegisteredChannels() const {
  std::set<std::string> result;
  for (CommandHandlerMap::const_iterator iter = command_channel_map_.begin();
       iter != command_channel_map_.end(); ++iter) {
    result.insert(iter->first);
  }
  return result;
}

std::string ConsoleCommandManager::GetShortHelp(
    const std::string& channel) const {
  CommandHandlerMap::const_iterator iter = command_channel_map_.find(channel);
  if (iter != command_channel_map_.end()) {
    return iter->second->short_help();
  }
  return "No help available for unregistered channel: " + channel;
}

std::string ConsoleCommandManager::GetLongHelp(
    const std::string& channel) const {
  CommandHandlerMap::const_iterator iter = command_channel_map_.find(channel);
  if (iter != command_channel_map_.end()) {
    return iter->second->long_help();
  }
  return "No help available for unregistered channel: " + channel;
}

void ConsoleCommandManager::RegisterCommandHandler(
    const CommandHandler* handler) {
  DCHECK_GT(handler->channel().length(), size_t(0));
  base::AutoLock auto_lock(lock_);
  command_channel_map_[handler->channel()] = handler;
}

void ConsoleCommandManager::UnregisterCommandHandler(
    const CommandHandler* handler) {
  DCHECK_GT(handler->channel().length(), size_t(0));
  base::AutoLock auto_lock(lock_);
  command_channel_map_.erase(handler->channel());
}

#else   // ENABLE_DEBUG_CONSOLE

ConsoleCommandManager::CommandHandler::CommandHandler(
    const std::string& channel,
    const ConsoleCommandManager::CommandCallback& callback,
    const std::string& short_help, const std::string& long_help) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(callback);
  UNREFERENCED_PARAMETER(short_help);
  UNREFERENCED_PARAMETER(long_help);
}

ConsoleCommandManager::CommandHandler::~CommandHandler() {}

void ConsoleCommandManager::HandleCommand(const std::string& channel,
                                          const std::string& message) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(message);
}

std::set<std::string> ConsoleCommandManager::GetRegisteredChannels() const {
  return std::set<std::string>();
}

std::string ConsoleCommandManager::GetShortHelp(
    const std::string& channel) const {
  UNREFERENCED_PARAMETER(channel);
  return "";
}

std::string ConsoleCommandManager::GetLongHelp(
    const std::string& channel) const {
  UNREFERENCED_PARAMETER(channel);
  return "";
}
#endif  // ENABLE_DEBUG_CONSOLE

}  // namespace base
