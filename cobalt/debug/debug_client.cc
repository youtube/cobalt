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

#include "cobalt/debug/debug_client.h"

#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"

namespace cobalt {
namespace debug {

DebugClient::Delegate::~Delegate() {}

DebugClient::DebugClient(backend::DebugDispatcher* dispatcher,
                         Delegate* delegate)
    : dispatcher_(dispatcher), delegate_(delegate) {
  DCHECK(dispatcher_);
  dispatcher_->AddClient(this);
}

DebugClient::~DebugClient() {
  base::AutoLock auto_lock(dispatcher_lock_);
  if (dispatcher_) {
    dispatcher_->RemoveClient(this);
  }
}

bool DebugClient::IsAttached() {
  base::AutoLock auto_lock(dispatcher_lock_);
  return dispatcher_ != NULL;
}

void DebugClient::OnEvent(const std::string& method,
                          const std::string& json_params) {
  DCHECK(delegate_);
  delegate_->OnDebugClientEvent(method, json_params);
}

void DebugClient::SetDispatcher(backend::DebugDispatcher* dispatcher) {
  base::AutoLock auto_lock(dispatcher_lock_);
  dispatcher_ = dispatcher;
}

void DebugClient::OnDetach(const std::string& reason) {
  DCHECK(delegate_);
  base::AutoLock auto_lock(dispatcher_lock_);
  delegate_->OnDebugClientDetach(reason);
  dispatcher_ = NULL;
}

void DebugClient::SendCommand(const std::string& method,
                              const std::string& json_params,
                              const DebugClient::ResponseCallback& callback) {
  base::AutoLock auto_lock(dispatcher_lock_);
  if (!dispatcher_) {
    DLOG(WARNING) << "Debug client is not attached to dispatcher.";
    return;
  }
  dispatcher_->SendCommand(Command(method, json_params, callback));
}

}  // namespace debug
}  // namespace cobalt
