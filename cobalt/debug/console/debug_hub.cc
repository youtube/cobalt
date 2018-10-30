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

#if defined(ENABLE_DEBUG_CONSOLE)

#include "cobalt/debug/console/debug_hub.h"

#include <set>

#include "base/compiler_specific.h"
#include "cobalt/base/c_val.h"

namespace cobalt {
namespace debug {
namespace console {

DebugHub::DebugHub(
    const GetHudModeCallback& get_hud_mode_callback,
    const CreateDebugClientCallback& create_debug_client_callback)
    : get_hud_mode_callback_(get_hud_mode_callback),
      debugger_(new Debugger(create_debug_client_callback)) {}

DebugHub::~DebugHub() {
  base::LogMessageHandler::GetInstance()->RemoveCallback(
      log_message_handler_callback_id_);
}

void DebugHub::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(debugger_);
}

// TODO: This function should be modified to return an array of strings instead
// of a single space-separated string, once the bindings support return of a
// string array.
std::string DebugHub::GetConsoleValueNames() const {
  std::string ret = "";
  base::CValManager* cvm = base::CValManager::GetInstance();
  DCHECK(cvm);

  if (cvm) {
    std::set<std::string> names = cvm->GetOrderedCValNames();
    for (std::set<std::string>::const_iterator it = names.begin();
         it != names.end(); ++it) {
      ret += (*it);
      std::set<std::string>::const_iterator next = it;
      ++next;
      if (next != names.end()) {
        ret += " ";
      }
    }
  }
  return ret;
}

std::string DebugHub::GetConsoleValue(const std::string& name) const {
  std::string ret = "<undefined>";
  base::CValManager* cvm = base::CValManager::GetInstance();
  DCHECK(cvm);

  if (cvm) {
    base::optional<std::string> result = cvm->GetValueAsPrettyString(name);
    if (result) {
      return *result;
    }
  }
  return ret;
}

int DebugHub::GetDebugConsoleMode() const {
  return get_hud_mode_callback_.Run();
}

}  // namespace console
}  // namespace debug
}  // namespace cobalt

#endif  // ENABLE_DEBUG_CONSOLE
