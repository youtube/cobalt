// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/eme/media_encrypted_event.h"

#include "cobalt/base/tokens.h"
#include "cobalt/web/environment_settings_helper.h"

namespace cobalt {
namespace dom {
namespace eme {

// See step 5 in https://www.w3.org/TR/encrypted-media/#initdata-encountered.
MediaEncryptedEvent::MediaEncryptedEvent(
    script::EnvironmentSettings* environment_settings, const std::string& type)
    : Event(base_token::Token(type), kNotBubbles, kNotCancelable) {}

// See step 5 in https://www.w3.org/TR/encrypted-media/#initdata-encountered.
MediaEncryptedEvent::MediaEncryptedEvent(
    script::EnvironmentSettings* environment_settings, const std::string& type,
    const MediaEncryptedEventInit& event_init_dict)
    : Event(base_token::Token(type), kNotBubbles, kNotCancelable),
      init_data_type_(event_init_dict.init_data_type()) {
  if (event_init_dict.init_data() && !event_init_dict.init_data()->IsNull()) {
    auto* global_wrappable = web::get_global_wrappable(environment_settings);
    init_data_reference_.reset(
        new script::ScriptValue<script::ArrayBuffer>::Reference(
            global_wrappable, *event_init_dict.init_data()));
  }
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
