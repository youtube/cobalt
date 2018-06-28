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

#ifndef COBALT_BASE_APPLICATION_EVENT_H_
#define COBALT_BASE_APPLICATION_EVENT_H_

#include "cobalt/base/event.h"
#include "starboard/event.h"

namespace base {

class ApplicationEvent : public base::Event {
 public:
  explicit ApplicationEvent(SbEventType type) : type_(type) {}
  SbEventType type() const { return type_; }

  BASE_EVENT_SUBCLASS(ApplicationEvent);

 private:
  SbEventType type_;
};

}  // namespace base

#endif  // COBALT_BASE_APPLICATION_EVENT_H_
