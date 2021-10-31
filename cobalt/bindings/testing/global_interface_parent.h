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

#ifndef COBALT_BINDINGS_TESTING_GLOBAL_INTERFACE_PARENT_H_
#define COBALT_BINDINGS_TESTING_GLOBAL_INTERFACE_PARENT_H_

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

class GlobalInterfaceParent : public script::Wrappable {
 public:
  virtual void ParentOperation() {}

  DEFINE_WRAPPABLE_TYPE(GlobalInterfaceParent);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_GLOBAL_INTERFACE_PARENT_H_
