// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_CONVERT_SIMPLE_OBJECT_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_CONVERT_SIMPLE_OBJECT_INTERFACE_H_

#include <string>
#include <unordered_map>

#include "cobalt/script/exception_state.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

class ConvertSimpleObjectInterface : public script::Wrappable {
 public:
  ConvertSimpleObjectInterface() = default;

  std::unordered_map<std::string, std::string> result() { return result_; }

  void ConvertSimpleObjectToMapTest(const script::ValueHandleHolder& value,
                                    script::ExceptionState* exception_state) {
    result_ = script::ConvertSimpleObjectToMap(value, exception_state);
  }

  DEFINE_WRAPPABLE_TYPE(ConvertSimpleObjectInterface);

 private:
  std::unordered_map<std::string, std::string> result_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_CONVERT_SIMPLE_OBJECT_INTERFACE_H_
