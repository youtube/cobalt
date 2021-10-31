// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_SINGLE_OPERATION_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_SINGLE_OPERATION_INTERFACE_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

class ArbitraryInterface;

class SingleOperationInterface {
 public:
  virtual base::Optional<int32_t> HandleCallback(
      const scoped_refptr<script::Wrappable>& callback_this,
      const scoped_refptr<ArbitraryInterface>& value,
      bool* had_exception) const = 0;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_SINGLE_OPERATION_INTERFACE_H_
