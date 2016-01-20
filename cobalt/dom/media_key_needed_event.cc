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

#include "cobalt/dom/media_key_needed_event.h"

#include "cobalt/base/tokens.h"

namespace cobalt {
namespace dom {

MediaKeyNeededEvent::MediaKeyNeededEvent(
    const std::string& key_system, const std::string& session_id,
    const scoped_refptr<Uint8Array>& init_data)
    : Event(base::Tokens::needkey(), kNotBubbles, kNotCancelable),
      key_system_(key_system),
      session_id_(session_id),
      init_data_(init_data) {}

}  // namespace dom
}  // namespace cobalt
