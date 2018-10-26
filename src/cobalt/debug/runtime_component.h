// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DEBUG_RUNTIME_COMPONENT_H_
#define COBALT_DEBUG_RUNTIME_COMPONENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/command_map.h"
#include "cobalt/debug/debug_dispatcher.h"
#include "cobalt/debug/debug_script_runner.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {

class RuntimeComponent {
 public:
  explicit RuntimeComponent(DebugDispatcher* dispatcher);

 private:
  void CompileScript(const Command& command);
  void Disable(const Command& command);
  void Enable(const Command& command);

  DebugDispatcher* dispatcher_;

  // Map of member functions implementing commands.
  CommandMap<RuntimeComponent> commands_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_RUNTIME_COMPONENT_H_
