// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/cobalt_agent.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "cobalt/debug/console/command_manager.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {
namespace backend {

CobaltAgent::CobaltAgent(DebugDispatcher* dispatcher)
    : AgentBase("Cobalt", dispatcher) {
  commands_["getConsoleCommands"] =
      base::Bind(&CobaltAgent::GetConsoleCommands, base::Unretained(this));
  commands_["sendConsoleCommand"] =
      base::Bind(&CobaltAgent::SendConsoleCommand, base::Unretained(this));
  // dispatcher_->AddDomain(domain_, commands_.Bind());
}

void CobaltAgent::GetConsoleCommands(Command command) {
#ifndef USE_HACKY_COBALT_CHANGES
  JSONObject response(new base::DictionaryValue());
  JSONList list(new base::ListValue());
#else
  JSONObject response(nullptr);
  JSONList list(nullptr);
#endif

  console::ConsoleCommandManager* command_manager =
      console::ConsoleCommandManager::GetInstance();
  DCHECK(command_manager);
  if (command_manager) {
    std::set<std::string> commands = command_manager->GetRegisteredCommands();
    for (auto& command_name : commands) {
#ifndef USE_HACKY_COBALT_CHANGES
      JSONObject console_command(new base::DictionaryValue());
      console_command->SetString("command", command_name);
      console_command->SetString("shortHelp",
                                 command_manager->GetShortHelp(command_name));
      console_command->SetString("longHelp",
                                 command_manager->GetLongHelp(command_name));
      list->Append(std::move(console_command));
#endif
    }
  }

#ifndef USE_HACKY_COBALT_CHANGES
  JSONObject commands(new base::DictionaryValue());
  commands->Set("commands", std::move(list));
  response->Set("result", std::move(commands));
#endif
  command.SendResponse(JSONObject(nullptr));
}

void CobaltAgent::SendConsoleCommand(Command command) {
  JSONObject params = JSONParse(command.GetParams());
  if (params) {
    std::string* console_command = params->FindString("command");
    if (console_command) {
      std::string* message = params->FindString("message");
      console::ConsoleCommandManager* console_command_manager =
          console::ConsoleCommandManager::GetInstance();
      DCHECK(console_command_manager);
      console_command_manager->HandleCommand(*console_command, *message);
      command.SendResponse();
      return;
    }
  }
  command.SendErrorResponse(Command::kInvalidParams, "Missing command.");
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
