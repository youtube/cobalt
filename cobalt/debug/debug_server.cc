/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/debug/debug_server.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"

namespace cobalt {
namespace debug {

namespace {
// Command "methods" (names) from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/index
const char kGetScriptSource[] = "Debugger.getScriptSource";
}  // namespace

DebugServer::DebugServer(
    script::GlobalObjectProxy* global_object_proxy,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const OnEventCallback& on_event_callback,
    const OnDetachCallback& on_detach_callback)
    : message_loop_proxy_(message_loop_proxy),
      on_event_callback_(on_event_callback),
      on_detach_callback_(on_detach_callback) {
  if (base::MessageLoopProxy::current() == message_loop_proxy_) {
    CreateJavaScriptDebugger(global_object_proxy);
  } else {
    // Create the debugger components on the message loop of their WebModule
    // and wait for them to be constructed before continuing.
    thread_checker_.DetachFromThread();
    base::WaitableEvent javascript_debugger_created(true, false);
    message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&DebugServer::CreateJavaScriptDebugger,
                              base::Unretained(this),
                              base::Unretained(global_object_proxy)));
    message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                              base::Unretained(&javascript_debugger_created)));
    javascript_debugger_created.Wait();
  }

  RegisterCommands();
}

DebugServer::~DebugServer() {}

void DebugServer::SendCommand(const std::string& method,
                              const std::string& json_params,
                              CommandCallback callback) {
  DLOG(INFO) << "Executing command: " << method << ": " << json_params;

  CommandCallbackInfo callback_info(callback);
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&DebugServer::DispatchCommand, base::Unretained(this), method,
                 json_params, callback_info));
}

void DebugServer::CreateJavaScriptDebugger(
    script::GlobalObjectProxy* global_object_proxy) {
  DCHECK(global_object_proxy);
  DCHECK(thread_checker_.CalledOnValidThread());

  // We receive event parameters from the JavaScript debugger as Dictionary
  // objects and use the OnEvent method of this class to serialize to JSON
  // strings for the external onEvent callback. The external onDetach callback
  // can be used directly.
  script::JavaScriptDebuggerInterface::OnEventCallback on_event_callback =
      base::Bind(&DebugServer::OnEvent, base::Unretained(this));

  javascript_debugger_ = script::JavaScriptDebuggerInterface::CreateDebugger(
      global_object_proxy, on_event_callback, on_detach_callback_);
}

void DebugServer::DispatchCommand(std::string method, std::string json_params,
                                  CommandCallbackInfo callback_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Find the command in the registry and parse the JSON parameter string.
  // Both of these could fail, so we check below.
  CommandRegistry::iterator iter = command_registry_.find(method);
  JSONObject params = JSONParse(json_params);
  JSONObject response;

  if (iter != command_registry_.end() && params) {
    // Everything is looking good so far - run the command function and take
    // ownership of the response object.
    response.reset(iter->second.Run(params).release());
  }

  if (!response) {
    // Something went wrong, generate an error response.
    response.reset(new base::DictionaryValue());
    response->SetString("error.message", "Could not execute request.");
  }

  // Serialize the response object and run the callback.
  DCHECK(response);
  std::string json_response = JSONStringify(response);
  callback_info.message_loop_proxy->PostTask(
      FROM_HERE, base::Bind(callback_info.callback, json_response));
}

void DebugServer::OnEvent(const std::string& method, const JSONObject& params) {
  // Serialize the event parameters and pass to the external callback.
  const std::string json_params = JSONStringify(params);
  on_event_callback_.Run(method, json_params);
}

void DebugServer::RegisterCommands() {
  command_registry_[kGetScriptSource] =
      base::Bind(&DebugServer::GetScriptSource, base::Unretained(this));
}

JSONObject DebugServer::GetScriptSource(const JSONObject& params) {
  return javascript_debugger_->GetScriptSource(params);
}

}  // namespace debug
}  // namespace cobalt
