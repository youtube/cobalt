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
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {
namespace backend {

// Type of an agent member function that implements a protocol command.
template <class T>
using CommandFn = void (T::*)(const Command& command);

// A map of method names to agent command functions. Provides a standard
// implementation of the top-level domain command handler.
template <class T>
class CommandMap : public std::map<std::string, CommandFn<T>> {
 public:
  explicit CommandMap(T* agent) : agent_(agent) {}

  // Calls the mapped method implementation.
  // Returns a true iff the command method is mapped and has been run.
  bool RunCommand(const Command& command) {
    auto iter = this->find(command.GetMethod());
    if (iter == this->end()) return false;
    auto command_fn = iter->second;
    (agent_->*command_fn)(command);
    return true;
  }

  // Binds |RunCommand| to a callback to be registered with |DebugDispatcher|.
  DebugDispatcher::CommandHandler Bind() {
    return base::Bind(&CommandMap<T>::RunCommand, base::Unretained(this));
  }

 private:
  T* agent_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_COMMAND_MAP_H_
