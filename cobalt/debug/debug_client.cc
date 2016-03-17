/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/debug/debug_client.h"

#include "cobalt/debug/debug_server.h"

namespace cobalt {
namespace debug {

DebugClient::Delegate::~Delegate() {}

DebugClient::DebugClient(DebugServer* server, Delegate* delegate)
    : server_(server), delegate_(delegate) {
  DCHECK(server_);
  server_->AddClient(this);
}

DebugClient::~DebugClient() {
  base::AutoLock auto_lock(server_lock_);
  if (server_) {
    server_->RemoveClient(this);
  }
}

bool DebugClient::IsAttached() {
  base::AutoLock auto_lock(server_lock_);
  return server_ != NULL;
}

void DebugClient::OnEvent(const std::string& method,
                          const base::optional<std::string>& json_params) {
  DCHECK(delegate_);
  delegate_->OnDebugClientEvent(method, json_params);
}

void DebugClient::OnDetach(const std::string& reason) {
  DCHECK(delegate_);
  base::AutoLock auto_lock(server_lock_);
  delegate_->OnDebugClientDetach(reason);
  server_ = NULL;
}

void DebugClient::SendCommand(const std::string& method,
                              const std::string& json_params,
                              const DebugServer::CommandCallback& callback) {
  base::AutoLock auto_lock(server_lock_);
  if (!server_) {
    DLOG(WARNING) << "Debug client is not attached to server.";
    return;
  }
  server_->SendCommand(method, json_params, callback);
}

}  // namespace debug
}  // namespace cobalt
