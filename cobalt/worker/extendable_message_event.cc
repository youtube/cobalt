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

#include "cobalt/worker/extendable_message_event.h"

#include <string>
#include <utility>

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/worker/extendable_message_event_init.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace worker {

ExtendableMessageEvent::ExtendableMessageEvent(
    base::Token type, const ExtendableMessageEventInit& init_dict)
    : ExtendableEvent(type, init_dict) {
  if (init_dict.has_data() && init_dict.data()) {
    DCHECK(init_dict.data());
    data_ = script::SerializeScriptValue(*(init_dict.data()));
  }
  if (init_dict.has_origin()) {
    origin_ = init_dict.origin();
  }
  if (init_dict.has_last_event_id()) {
    last_event_id_ = init_dict.last_event_id();
  }
  if (init_dict.has_source() && init_dict.source().has_value()) {
    source_ = init_dict.source().value();
  }
  if (init_dict.has_ports()) {
    ports_ = init_dict.ports();
  }
}

script::Handle<script::ValueHandle> ExtendableMessageEvent::data(
    script::EnvironmentSettings* settings) const {
  if (!settings) return script::Handle<script::ValueHandle>();
  script::GlobalEnvironment* global_environment =
      base::polymorphic_downcast<web::EnvironmentSettings*>(settings)
          ->context()
          ->global_environment();
  DCHECK(global_environment);
  v8::Isolate* isolate = global_environment->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  DCHECK(isolate);
  if (!data_) return script::Handle<script::ValueHandle>();
  return script::Handle<script::ValueHandle>(
      std::move(script::DeserializeScriptValue(isolate, *data_)));
}

}  // namespace worker
}  // namespace cobalt
