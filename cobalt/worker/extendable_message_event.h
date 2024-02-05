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
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "cobalt/base/token.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/client.h"
#include "cobalt/worker/extendable_event.h"
#include "cobalt/worker/extendable_message_event_init.h"
#include "cobalt/worker/service_worker.h"

namespace cobalt {
namespace worker {

class ExtendableMessageEvent : public ExtendableEvent {
 public:
  using SourceType =
      cobalt::script::UnionType3<scoped_refptr<Client>,
                                 scoped_refptr<ServiceWorker>,
                                 scoped_refptr<web::MessagePort>>;

  explicit ExtendableMessageEvent(script::EnvironmentSettings* settings,
                                  const std::string& type)
      : ExtendableEvent(settings, type) {}
  explicit ExtendableMessageEvent(script::EnvironmentSettings* settings,
                                  base_token::Token type)
      : ExtendableEvent(settings, type) {}
  ExtendableMessageEvent(script::EnvironmentSettings* settings,
                         const std::string& type,
                         const ExtendableMessageEventInit& init_dict)
      : ExtendableMessageEvent(settings, base_token::Token(type), init_dict) {}
  ExtendableMessageEvent(script::EnvironmentSettings* settings,
                         base_token::Token type,
                         const ExtendableMessageEventInit& init_dict);
  ExtendableMessageEvent(
      script::EnvironmentSettings* settings, base_token::Token type,
      const ExtendableMessageEventInit& init_dict,
      std::unique_ptr<script::StructuredClone> structured_clone)
      : ExtendableMessageEvent(settings, type, init_dict) {
    structured_clone_ = std::move(structured_clone);
  }

  script::Handle<script::ValueHandle> data(
      script::EnvironmentSettings* settings = nullptr);

  const std::string& origin() const { return origin_; }
  const std::string& last_event_id() const { return last_event_id_; }
  SourceType source() const { return source_; }
  script::Sequence<scoped_refptr<MessagePort>> ports() const { return ports_; }

  // These helper functions are custom, and not in any spec.
  void set_origin(const std::string& origin) { origin_ = origin; }
  void set_last_event_id(const std::string& last_event_id) {
    last_event_id_ = last_event_id;
  }
  void set_source(const SourceType& source) { source_ = source; }
  void set_ports(script::Sequence<scoped_refptr<MessagePort>> ports) {
    ports_ = ports;
  }

  DEFINE_WRAPPABLE_TYPE(ExtendableMessageEvent);

 protected:
  ~ExtendableMessageEvent() override {}

 private:
  std::string origin_;
  std::string last_event_id_;
  SourceType source_;
  script::Sequence<scoped_refptr<MessagePort>> ports_;
  std::unique_ptr<script::StructuredClone> structured_clone_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_EXTENDABLE_MESSAGE_EVENT_H_
