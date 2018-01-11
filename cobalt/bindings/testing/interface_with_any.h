// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_INTERFACE_WITH_ANY_H_
#define COBALT_BINDINGS_TESTING_INTERFACE_WITH_ANY_H_

#include "base/compiler_specific.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

class InterfaceWithAny : public script::Wrappable {
 public:
  InterfaceWithAny() = default;

  const script::ValueHandleHolder* GetAny() {
    if (!value_) {
      return NULL;
    }

    return &(value_->referenced_value());
  }

  void SetAny(const script::ValueHandleHolder& value) {
    value_.reset(new script::ValueHandleHolder::Reference(this, value));
  }

  DEFINE_WRAPPABLE_TYPE(InterfaceWithAny);

 private:
  scoped_ptr<script::ValueHandleHolder::Reference> value_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_INTERFACE_WITH_ANY_H_
