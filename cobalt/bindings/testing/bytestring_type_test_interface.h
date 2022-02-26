// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_BYTESTRING_TYPE_TEST_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_BYTESTRING_TYPE_TEST_INTERFACE_H_

#include <vector>

#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class BytestringTypeTestInterface : public script::Wrappable {
 public:
  std::vector<uint8_t> BytestringReturnOperation() {
    return std::vector<uint8_t>{1, 2, 3};
  }
  std::vector<uint8_t> BytestringArgumentOperation(std::vector<uint8_t> arg1) {
    return arg1;
  }

  DEFINE_WRAPPABLE_TYPE(BytestringTypeTestInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_BYTESTRING_TYPE_TEST_INTERFACE_H_
