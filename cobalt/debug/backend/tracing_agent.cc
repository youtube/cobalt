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

#include <iostream>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "cobalt/script/script_debugger.h"
#include "starboard/common/string.h"

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
// constexpr size_t kDataCollectedSize = 24 * 1024;
}  // namespace

TracingAgent::TracingAgent(DebugDispatcher* dispatcher,
                           script::ScriptDebugger* script_debugger)
    : dispatcher_(dispatcher),
      script_debugger_(script_debugger),
      tracing_started_(false),
      collected_size_(0),
      commands_(kInspectorDomain) {
  DCHECK(dispatcher_);

  trace_buffer_.SetOutputCallback(json_output_.GetCallback());

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

  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();

  trace_log->SetDisabled(base::trace_event::TraceLog::RECORDING_MODE);

  base::WaitableEvent waitable_event;
  // trace_log->Flush(
  //     base::Bind(&TracingAgent::OnTraceDataCollected, base::Unretained(this),
  //     base::Unretained(&waitable_event)));

  base::Thread thread("json_outputter");
  thread.Start();

  base::trace_event::TraceLog::GetInstance()
      ->SetCurrentThreadBlocksMessageLoop();

  auto output_callback =
      base::Bind(&TracingAgent::OutputTraceData, base::Unretained(this),
                 base::BindRepeating(&base::WaitableEvent::Signal,
                                     base::Unretained(&waitable_event)));
  // auto output_callback =
  //     base::Bind(&TracingAgent::OnTraceDataCollected, base::Unretained(this),
  //                base::BindRepeating(&base::WaitableEvent::Signal,
  //                                     base::Unretained(&waitable_event)));
  //  Write out the actual data by calling Flush().  Within Flush(), this
  //  will call OutputTraceData(), possibly multiple times.  We have to do this
  //  on a thread as there will be task posted to the current thread for data
  //  writing.
  thread.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindRepeating(&base::trace_event::TraceLog::Flush,
                          base::Unretained(trace_log), output_callback, false));

  waitable_event.Wait();


  // AppendTraceEvent(json_output_.json_output);
  FlushTraceEvents();


  // script_debugger_->StopTracing();
}


void TracingAgent::OutputTraceData(
    base::OnceClosure finished_cb,
    const scoped_refptr<base::RefCountedString>& event_string,
    bool has_more_events) {
  json_output_.json_output.clear();
  trace_buffer_.Start();
  trace_buffer_.AddFragment(event_string->data());
  trace_buffer_.Finish();

  std::unique_ptr<base::Value> root =
      base::JSONReader::Read(json_output_.json_output, base::JSON_PARSE_RFC);

  if (!root.get()) {
    LOG(ERROR) << json_output_.json_output;
  }

  base::ListValue* root_list = nullptr;
  root->GetAsList(&root_list);

  // Move items into our aggregate collection
  while (root_list->GetSize()) {
    if (!collected_events_) {
      collected_events_.reset(new base::ListValue());
    }
    std::unique_ptr<base::Value> item;
    root_list->Remove(0, &item);
    collected_events_->Append(std::move(item));
    collected_size_ += event_string->size();
    // if (collected_size_ >= kDataCollectedSize) {
    //   SendDataCollectedEvent();
    // }
  }

  if (!has_more_events) {
    trace_buffer_.Finish();
    std::move(finished_cb).Run();
  }
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

  // trace_parsed_.Clear();
  collected_events_.reset(new base::ListValue());
  base::trace_event::TraceLog* tracelog =
      base::trace_event::TraceLog::GetInstance();
  // tracelog->SetOutputCallback(
  //     base::Bind(&TracingAgent::OnTraceDataCollected,
  //                base::Unretained(this)));
  json_output_.json_output.clear();
  trace_buffer_.SetOutputCallback(json_output_.GetCallback());

  // script_debugger_->StartTracing(categories_, this);
  DCHECK(!tracelog->IsEnabled());
  tracelog->SetEnabled(base::trace_event::TraceConfig(),
                       base::trace_event::TraceLog::RECORDING_MODE);

  // LOG(INFO) << "YO THOR - TRACE BUFFER START";
  // trace_buffer_.Start();

  tracing_started_ = true;
  command.SendResponse();
}

void TracingAgent::AppendTraceEvent(const std::string& trace_event_json) {
  //  // We initialize a new list into which we collect events both when we
  //  start,
  //  // and after each time it's released in |SendDataCollectedEvent|.
  //  LOG(INFO) << "YO THOR _ TRACING AGENT - APPEND EVENT: GOT" <<
  //  trace_event_json;
  //
  //  int errcode;
  //  JSONObject event = JSONParse(trace_event_json, &errcode);
  //  LOG(INFO) << "YO THOR - NOW ITS JSON PARSED:" << JSONStringify(event);
  //  if (errcode) {
  //    LOG(INFO) << "YO THOR - GOT ERR CODE:" << errcode;
  //  }
  //  if (event) {
  //    collected_events_->Append(std::move(event));
  //    collected_size_ += trace_event_json.size();
  //  }
  //
  //  if (collected_size_ >= kDataCollectedSize) {
  //    SendDataCollectedEvent();
  //  }
}

void TracingAgent::FlushTraceEvents() {
  SendDataCollectedEvent();
  dispatcher_->SendEvent(std::string(kInspectorDomain) + ".tracingComplete");
}

void TracingAgent::SendDataCollectedEvent() {
  if (collected_events_) {
    std::string events;
    collected_events_->GetString(0, &events);
    LOG(INFO) << "YO THOR ! SEND DATA COLLECTED! num:"
              << collected_events_->GetSize() << " Events:" << events;
    // if (trace_parsed_) {
    // collected_size_ = 0;
    JSONObject params(new base::DictionaryValue());
    // Releasing the list into the value param avoids copying it.
    params->Set("value", std::move(collected_events_));
    // params->Set("value", std::move(trace_parsed_));
    // params->Set("value", std::make_unique<base::ListValue>(trace_parsed_));
    dispatcher_->SendEvent(std::string(kInspectorDomain) + ".dataCollected",
                           params);
    collected_size_ = 0;
  }
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
