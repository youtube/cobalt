// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/debug/debugger.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace cobalt {
namespace debug {

Debugger::Debugger(const GetDebugServerCallback& get_debug_server_callback)
    : get_debug_server_callback_(get_debug_server_callback),
      on_event_(new DebuggerEventTarget()) {}

Debugger::~Debugger() {}

void Debugger::Attach(const AttachCallbackArg& callback) {
  last_error_ = base::nullopt;
  DebugServer* debug_server = get_debug_server_callback_.Run();

  // |debug_server| may be NULL if the WebModule is not available at this time.
  if (debug_server) {
    debug_client_.reset(new DebugClient(debug_server, this));
  } else {
    DLOG(WARNING) << "Debug server unavailable.";
    last_error_ = "Debug server unavailable.";
  }

  AttachCallbackArg::Reference callback_reference(this, callback);
  callback_reference.value().Run();
}

void Debugger::Detach(const AttachCallbackArg& callback) {
  last_error_ = base::nullopt;
  debug_client_.reset(NULL);
  AttachCallbackArg::Reference callback_reference(this, callback);
  callback_reference.value().Run();
}

void Debugger::SendCommand(const std::string& method,
                           const std::string& json_params,
                           const CommandCallbackArg& callback) {
  last_error_ = base::nullopt;
  if (!debug_client_ || !debug_client_->IsAttached()) {
    scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue);
    response->SetString("error.message",
                        "Debugger is not connected - call attach first.");
    std::string json_response;
    base::JSONWriter::Write(response.get(), &json_response);
    CommandCallbackArg::Reference callback_ref(this, callback);
    callback_ref.value().Run(json_response);
    return;
  }

  scoped_refptr<CommandCallbackInfo> callback_info(
      new CommandCallbackInfo(this, callback));
  debug_client_->SendCommand(
      method, json_params,
      base::Bind(&Debugger::OnCommandResponse, this, callback_info));
}

void Debugger::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(on_event_);
}

void Debugger::OnCommandResponse(
    const scoped_refptr<CommandCallbackInfo>& callback_info,
    const base::optional<std::string>& response) const {
  // Run the script callback on the message loop the command was sent from.
  callback_info->message_loop_proxy->PostTask(
      FROM_HERE,
      base::Bind(&Debugger::RunCommandCallback, this, callback_info, response));
}

void Debugger::OnDebugClientEvent(const std::string& method,
                                  const base::optional<std::string>& params) {
  // Pass to the onEvent handler. The handler will notify the JavaScript
  // listener on the message loop the listener was registered on.
  on_event_->DispatchEvent(method, params);
}

void Debugger::OnDebugClientDetach(const std::string& reason) {
  DLOG(INFO) << "Debugger detached: " + reason;
  const std::string method = "Inspector.detached";
  JSONObject params(new base::DictionaryValue());
  params->SetString("reason", reason);
  on_event_->DispatchEvent(method, JSONStringify(params));
}

void Debugger::RunCommandCallback(
    const scoped_refptr<CommandCallbackInfo>& callback_info,
    base::optional<std::string> response) const {
  DCHECK_EQ(base::MessageLoopProxy::current(),
            callback_info->message_loop_proxy);
  callback_info->callback.value().Run(response);
}
}  // namespace debug
}  // namespace cobalt

#endif  // ENABLE_DEBUG_CONSOLE
