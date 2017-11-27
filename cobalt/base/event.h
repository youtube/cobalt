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

#ifndef COBALT_BASE_EVENT_H_
#define COBALT_BASE_EVENT_H_

#include "cobalt/base/type_id.h"

namespace base {

// Event is an abstract, generic class representing some system event that
// occured.  Event producers like a SystemWindow will produce these events
// and dispatch them to the registered callbacks.
// Derived classes should add the BASE_EVENT_SUBCLASS() macro to their public
// attributes.
class Event {
 public:
  virtual ~Event() {}
  virtual base::TypeId GetTypeId() const = 0;
};

#define BASE_EVENT_SUBCLASS(name)                                  \
  static base::TypeId TypeId() { return base::GetTypeId<name>(); } \
  base::TypeId GetTypeId() const override { return TypeId(); }

}  // namespace base

#endif  // COBALT_BASE_EVENT_H_
