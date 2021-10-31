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

#ifndef COBALT_BINDINGS_TESTING_CONDITIONAL_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_CONDITIONAL_INTERFACE_H_

#if defined(ENABLE_CONDITIONAL_INTERFACE)

#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class ConditionalInterface : public script::Wrappable {
 public:
#if defined(ENABLE_CONDITIONAL_PROPERTY)
  void EnabledOperation() {}
  int32 enabled_attribute() { return 0; }
  void set_enabled_attribute(int32 value) {}
#endif  // defined(ENABLE_CONDITIONAL_PROPERTY)

#if defined(NO_ENABLE_CONDITIONAL_PROPERTY)
  void DisabledOperation() {}
  int32 disabled_attribute() { return 0; }
  void set_disabled_attribute(int32 value) {}
#endif  // defined(NO_ENABLE_CONDITIONAL_PROPERTY)

  DEFINE_WRAPPABLE_TYPE(ConditionalInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // defined(ENABLE_CONDITIONAL_INTERFACE)

#endif  // COBALT_BINDINGS_TESTING_CONDITIONAL_INTERFACE_H_
