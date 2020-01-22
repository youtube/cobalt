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

#ifndef COBALT_DEBUG_BACKEND_COMMAND_MAP_H_
#define COBALT_DEBUG_BACKEND_COMMAND_MAP_H_

#include <map>
#include <string>

#include "base/bind.h"
#include "base/optional.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {
namespace backend {

// Type of a function that implements a protocol command.
using CommandFn = base::Callback<void(Command command)>;

// A map of method names to agent command functions. Provides a standard
// implementation of the top-level domain command handler.
class CommandMap : public std::map<std::string, CommandFn> {
 public:
  // If |domain| is specified, then commands for that domain should be mapped
  // using just the the simple method name (i.e. "method" not "Domain.method").
  explicit CommandMap(const std::string& domain = std::string())
      : domain_(domain) {}

  // Calls the mapped method implementation.
  // Passes ownership of the command to the mapped method, otherwise returns
  // ownership of the not-run command for a fallback JS implementation.
  base::Optional<Command> RunCommand(Command command) {
    // If the domain matches, trim it and the dot from the method name.
    const std::string& method =
        (domain_ == command.GetDomain())
            ? command.GetMethod().substr(domain_.size() + 1)
            : command.GetMethod();
    auto iter = this->find(method);
    if (iter == this->end()) return base::make_optional(std::move(command));
    auto command_impl = iter->second;
    command_impl.Run(std::move(command));
    return base::nullopt;
  }

  // Binds |RunCommand| to a callback to be registered with |DebugDispatcher|.
  DebugDispatcher::CommandHandler Bind() {
    return base::Bind(&CommandMap::RunCommand, base::Unretained(this));
  }

 private:
  std::string domain_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_COMMAND_MAP_H_
