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

#ifndef COBALT_BINDINGS_TESTING_SCRIPT_OBJECT_OWNER_H_
#define COBALT_BINDINGS_TESTING_SCRIPT_OBJECT_OWNER_H_

#include "base/optional.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

template <typename T>
class ScriptObjectOwner {
 public:
  explicit ScriptObjectOwner(script::Wrappable* owner) : owner_(owner) {}
  void TakeOwnership(const T& object) {
    if (!object.IsNull()) {
      reference_.emplace(owner_, object);
    } else {
      reference_ = base::nullopt;
    }
  }
  typename T::Reference& reference() { return reference_.value(); }
  bool IsSet() { return static_cast<bool>(reference_); }

 private:
  script::Wrappable* owner_;
  base::Optional<typename T::Reference> reference_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_SCRIPT_OBJECT_OWNER_H_
