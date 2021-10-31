// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/agent_base.h"

#include "base/values.h"

namespace cobalt {
namespace debug {
namespace backend {

using base::Value;

namespace {
// State parameters
constexpr char kEnabled[] = "enabled";
}  // namespace

AgentBase::AgentBase(const std::string& domain, const std::string& script_file,
                     DebugDispatcher* dispatcher)
    : domain_(domain),
      script_file_(script_file),
      dispatcher_(dispatcher),
      commands_(domain) {
  DCHECK(!domain_.empty());
  DCHECK(dispatcher_);

  commands_["disable"] =
      base::Bind(&AgentBase::Disable, base::Unretained(this));
  commands_["enable"] = base::Bind(&AgentBase::Enable, base::Unretained(this));
}

void AgentBase::Thaw(JSONObject agent_state) {
  dispatcher_->AddDomain(domain_, commands_.Bind());
  if (!agent_state) return;

  // Restore state
  if (agent_state->FindKeyOfType(kEnabled, Value::Type::BOOLEAN)->GetBool()) {
    Enable(Command::IgnoreResponse(domain_ + ".enable"));
  }
}

JSONObject AgentBase::Freeze() {
  dispatcher_->RemoveDomain(domain_);

  JSONObject agent_state(new base::DictionaryValue());
  agent_state->SetKey(kEnabled, Value(enabled_));
  return agent_state;
}

bool AgentBase::EnsureEnabled(Command* command) {
  if (!enabled_) {
    command->SendErrorResponse(Command::kInvalidRequest,
                               domain_ + " not enabled");
  }
  return enabled_;
}

bool AgentBase::DoEnable(Command* command) {
  if (enabled_) {
    command->SendErrorResponse(Command::kInvalidRequest,
                               domain_ + " already enabled");
    return false;
  }
  enabled_ = script_file_.empty() || dispatcher_->RunScriptFile(script_file_);
  if (!enabled_) {
    DLOG(ERROR) << "Failed to load " << script_file_;
    command->SendErrorResponse(Command::kInternalError,
                               domain_ + " agent failed to initialize");
    return false;
  }
  return true;
}

bool AgentBase::DoDisable(Command* command) {
  if (!EnsureEnabled(command)) return false;
  // TODO: Unload the script file
  enabled_ = false;
  return true;
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
