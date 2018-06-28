// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/progress_event.h"

namespace cobalt {
namespace dom {

ProgressEvent::ProgressEvent(const std::string& type)
    : Event(base::Token(type), kNotBubbles, kNotCancelable),
      loaded_(0),
      total_(0),
      length_computable_(false) {}

ProgressEvent::ProgressEvent(base::Token type)
    : Event(type, kNotBubbles, kNotCancelable),
      loaded_(0),
      total_(0),
      length_computable_(false) {}

ProgressEvent::ProgressEvent(base::Token type, uint64 loaded, uint64 total,
                             bool length_computable)
    : Event(type, kNotBubbles, kNotCancelable),
      loaded_(loaded),
      total_(total),
      length_computable_(length_computable) {}

}  // namespace dom
}  // namespace cobalt
