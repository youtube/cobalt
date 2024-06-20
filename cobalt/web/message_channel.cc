// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/message_channel.h"

#include "cobalt/web/event_target.h"
#include "cobalt/web/message_port.h"

namespace cobalt {
namespace web {

MessageChannel::MessageChannel(script::EnvironmentSettings* settings) {
  port1_ = new MessagePort(settings);
  port2_ = new MessagePort(settings);
  port1_->EntangleWithEventTarget(port2_);
  port2_->EntangleWithEventTarget(port1_);
  port1_->Start();
  port2_->Start();
}

MessageChannel::~MessageChannel() {
  port1_->Close();
  port2_->Close();
}


}  // namespace web
}  // namespace cobalt
