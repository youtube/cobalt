// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_EXTENDABLE_MESSAGE_EVENT_H_
#define COBALT_WORKER_EXTENDABLE_MESSAGE_EVENT_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "cobalt/base/token.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/extendable_event.h"
#include "cobalt/worker/extendable_message_event_init.h"

namespace cobalt {
namespace worker {

class ExtendableMessageEvent : public ExtendableEvent {
 public:
  typedef scoped_refptr<script::Wrappable> SourceType;

  explicit ExtendableMessageEvent(const std::string& type)
      : ExtendableEvent(type) {}
  explicit ExtendableMessageEvent(base::Token type) : ExtendableEvent(type) {}
  ExtendableMessageEvent(const std::string& type,
                         const ExtendableMessageEventInit& init_dict)
      : ExtendableEvent(type, init_dict) {}


  const script::ValueHandleHolder* data() const {
    if (!data_) {
      return NULL;
    }

    return &(data_->referenced_value());
  }

  std::string origin() const { return origin_; }
  std::string last_event_id() const { return last_event_id_; }

  base::Optional<SourceType> source() { return source_; }

  script::Sequence<scoped_refptr<MessagePort>> ports() const { return ports_; }

  DEFINE_WRAPPABLE_TYPE(ExtendableMessageEvent);

 protected:
  ~ExtendableMessageEvent() override {}

 private:
  std::unique_ptr<script::ValueHandleHolder::Reference> data_;

  std::string origin_ = "Origin Stub Value";
  std::string last_event_id_ = "Last Event Id Stub Value";
  base::Optional<SourceType> source_;
  script::Sequence<scoped_refptr<MessagePort>> ports_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_EXTENDABLE_MESSAGE_EVENT_H_
