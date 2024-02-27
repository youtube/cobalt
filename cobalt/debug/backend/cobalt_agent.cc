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

#include <memory>
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
  JSONObject response(std::make_unique<base::Value::Dict>());
  JSONList list(std::make_unique<base::Value::List>());

  console::ConsoleCommandManager* command_manager =
      console::ConsoleCommandManager::GetInstance();
  DCHECK(command_manager);
  if (command_manager) {
    std::set<std::string> commands = command_manager->GetRegisteredCommands();
    for (auto& command_name : commands) {
      JSONObject console_command(std::make_unique<base::Value::Dict>());
      console_command->Set("command", command_name);
      console_command->Set("shortHelp",
                           command_manager->GetShortHelp(command_name));
      console_command->Set("longHelp",
                           command_manager->GetLongHelp(command_name));
      list->Append(std::move(*console_command));
    }
  }

  JSONObject commands(std::make_unique<base::Value::Dict>());
  commands->Set("commands", std::move(*list));
  response->Set("result", std::move(*commands));
  command.SendResponse(JSONObject(nullptr));
}

void CobaltAgent::SendConsoleCommand(Command command) {
  JSONObject params = JSONParse(command.GetParams());
  if (params) {
    std::string* console_command = params->FindString("command");
    if (console_command) {
      JSONObject response(new base::Value::Dict());
      std::string* message = params->FindString("message");
      console::ConsoleCommandManager* console_command_manager =
          console::ConsoleCommandManager::GetInstance();
      DCHECK(console_command_manager);
      std::string response_string =
          console_command_manager->HandleCommand(*console_command, *message);
      response->Set("result", std::move(response_string));
      command.SendResponse();
      return;
    }
  }
  command.SendErrorResponse(Command::kInvalidParams, "Missing command.");
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
