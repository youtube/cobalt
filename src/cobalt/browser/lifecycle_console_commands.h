// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_LIFECYCLE_CONSOLE_COMMANDS_H_
#define COBALT_BROWSER_LIFECYCLE_CONSOLE_COMMANDS_H_

#if !defined(ENABLE_DEBUGGER)
#error "Debugger is not enabled in this build."
#endif  // !ENABLE_DEBUGGER

namespace cobalt {
namespace browser {

class LifecycleConsoleCommands {
 public:
  LifecycleConsoleCommands();

 private:
  debug::console::ConsoleCommandManager::CommandHandler pause_command_handler_;
  debug::console::ConsoleCommandManager::CommandHandler
      unpause_command_handler_;
  debug::console::ConsoleCommandManager::CommandHandler
      suspend_command_handler_;
  debug::console::ConsoleCommandManager::CommandHandler quit_command_handler_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_LIFECYCLE_CONSOLE_COMMANDS_H_
