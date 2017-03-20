// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_PROGRESS_EVENT_H_
#define COBALT_DOM_PROGRESS_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/dom/event.h"

namespace cobalt {
namespace dom {

class ProgressEvent : public Event {
 public:
  explicit ProgressEvent(const std::string& type);
  explicit ProgressEvent(base::Token type);
  ProgressEvent(base::Token type, uint64 loaded, uint64 total,
                bool length_computable);

  bool length_computable() const { return length_computable_; }
  uint64 loaded() const { return loaded_; }
  uint64 total() const { return total_; }

  DEFINE_WRAPPABLE_TYPE(ProgressEvent);

 private:
  uint64 loaded_;
  uint64 total_;
  bool length_computable_;

  DISALLOW_COPY_AND_ASSIGN(ProgressEvent);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PROGRESS_EVENT_H_
