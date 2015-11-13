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

#include "cobalt/debug/debugger.h"

namespace cobalt {
namespace debug {

Debugger::Debugger(
    const CreateDebugServerCallback& create_debug_server_callback)
    : create_debug_server_callback_(create_debug_server_callback) {}

Debugger::~Debugger() {}

void Debugger::Attach(const AttachCallbackArg& callback) {
  DLOG(INFO) << "Debugger::Attach";
  debug_server_.reset();
  debug_server_ = create_debug_server_callback_.Run();
  AttachCallbackArg::Reference callback_reference(this, callback);
  callback_reference.value().Run();
}

void Debugger::Detach(const AttachCallbackArg& callback) {
  DLOG(INFO) << "Debugger::Detach";
  debug_server_.reset();
  AttachCallbackArg::Reference callback_reference(this, callback);
  callback_reference.value().Run();
}

void Debugger::SendCommand(const std::string& method,
                           const script::OpaqueHandleHolder& params,
                           const CommandCallbackArg& callback) {
  UNREFERENCED_PARAMETER(params);
  UNREFERENCED_PARAMETER(callback);

  if (!debug_server_) {
    DLOG(WARNING) << "Debugger is not connected - call attach first.";
    return;
  }

  DLOG(INFO) << "Debugger::SendCommand";
  DLOG(INFO) << "method: " << method;
}

}  // namespace debug
}  // namespace cobalt
