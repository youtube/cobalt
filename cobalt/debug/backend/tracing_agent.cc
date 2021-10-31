// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/tracing_agent.h"

#include "base/bind.h"
#include "base/values.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {
// Definitions from the set specified here:
// https://chromedevtools.github.io/devtools-protocol/tot/Tracing
constexpr char kInspectorDomain[] = "Tracing";

// State parameters
constexpr char kStarted[] = "started";
constexpr char kCategories[] = "categories";

// Size in characters of JSON to batch dataCollected events.
constexpr size_t kDataCollectedSize = 24 * 1024;
}  // namespace

TracingAgent::TracingAgent(DebugDispatcher* dispatcher,
                           script::ScriptDebugger* script_debugger)
    : dispatcher_(dispatcher),
      script_debugger_(script_debugger),
      tracing_started_(false),
      collected_size_(0),
      commands_(kInspectorDomain) {
  DCHECK(dispatcher_);

  commands_["end"] = base::Bind(&TracingAgent::End, base::Unretained(this));
  commands_["start"] = base::Bind(&TracingAgent::Start, base::Unretained(this));
}

void TracingAgent::Thaw(JSONObject agent_state) {
  dispatcher_->AddDomain(kInspectorDomain, commands_.Bind());
  if (!agent_state) return;

  // Restore state
  categories_.clear();
  for (const auto& category : agent_state->FindKey(kCategories)->GetList()) {
    categories_.emplace_back(category.GetString());
  }
  tracing_started_ = agent_state->FindKey(kStarted)->GetBool();
  if (tracing_started_) {
    script_debugger_->StartTracing(categories_, this);
  }
}

JSONObject TracingAgent::Freeze() {
  if (tracing_started_) {
    script_debugger_->StopTracing();
  }

  dispatcher_->RemoveDomain(kInspectorDomain);

  // Save state
  JSONObject agent_state(new base::DictionaryValue());
  agent_state->SetKey(kStarted, base::Value(tracing_started_));
  base::Value::ListStorage categories_list;
  for (const auto& category : categories_) {
    categories_list.emplace_back(category);
  }
  agent_state->SetKey(kCategories, base::Value(std::move(categories_list)));

  return agent_state;
}

void TracingAgent::End(Command command) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!tracing_started_) {
    command.SendErrorResponse(Command::kInvalidRequest, "Tracing not started");
    return;
  }
  tracing_started_ = false;
  categories_.clear();
  command.SendResponse();

  script_debugger_->StopTracing();
}

void TracingAgent::Start(Command command) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (tracing_started_) {
    command.SendErrorResponse(Command::kInvalidRequest,
                              "Tracing already started");
    return;
  }

  JSONObject params = JSONParse(command.GetParams());

  // Parse comma-separated tracing categories parameter.
  categories_.clear();
  std::string category_param;
  if (params->GetString("categories", &category_param)) {
    for (size_t pos = 0, comma; pos < category_param.size(); pos = comma + 1) {
      comma = category_param.find(',', pos);
      if (comma == std::string::npos) comma = category_param.size();
      std::string category = category_param.substr(pos, comma - pos);
      categories_.push_back(category);
    }
  }

  tracing_started_ = true;
  script_debugger_->StartTracing(categories_, this);

  command.SendResponse();
}

void TracingAgent::AppendTraceEvent(const std::string& trace_event_json) {
  // We initialize a new list into which we collect events both when we start,
  // and after each time it's released in |SendDataCollectedEvent|.
  if (!collected_events_) {
    collected_events_.reset(new base::ListValue());
  }

  JSONObject event = JSONParse(trace_event_json);
  if (event) {
    collected_events_->Append(std::move(event));
    collected_size_ += trace_event_json.size();
  }

  if (collected_size_ >= kDataCollectedSize) {
    SendDataCollectedEvent();
  }
}

void TracingAgent::FlushTraceEvents() {
  SendDataCollectedEvent();
  dispatcher_->SendEvent(std::string(kInspectorDomain) + ".tracingComplete");
}

void TracingAgent::SendDataCollectedEvent() {
  if (collected_events_) {
    collected_size_ = 0;
    JSONObject params(new base::DictionaryValue());
    // Releasing the list into the value param avoids copying it.
    params->Set("value", std::move(collected_events_));
    dispatcher_->SendEvent(std::string(kInspectorDomain) + ".dataCollected",
                           params);
  }
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
