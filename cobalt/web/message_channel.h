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

#ifndef COBALT_WEB_MESSAGE_CHANNEL_H_
#define COBALT_WEB_MESSAGE_CHANNEL_H_

#include "base/memory/ref_counted.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/message_port.h"

namespace cobalt {
namespace web {

class MessageChannel : public script::Wrappable {
 public:
  explicit MessageChannel(script::EnvironmentSettings* settings);
  ~MessageChannel();

  MessageChannel(const MessageChannel&) = delete;
  MessageChannel& operator=(const MessageChannel&) = delete;

  const scoped_refptr<MessagePort>& port1() { return port1_; }
  const scoped_refptr<MessagePort>& port2() { return port2_; }

  DEFINE_WRAPPABLE_TYPE(MessageChannel);

 private:
  scoped_refptr<MessagePort> port1_;
  scoped_refptr<MessagePort> port2_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_MESSAGE_CHANNEL_H_
