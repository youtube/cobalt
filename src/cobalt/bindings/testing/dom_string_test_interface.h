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

#ifndef COBALT_BINDINGS_TESTING_DOM_STRING_TEST_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_DOM_STRING_TEST_INTERFACE_H_

#include <string>

#include "base/optional.h"
#include "cobalt/base/token.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class DOMStringTestInterface : public script::Wrappable {
 public:
  // attribute DOMString property
  MOCK_CONST_METHOD0(property, std::string());
  MOCK_METHOD1(set_property, void(const std::string&));

  // readonly attribute DOMString readOnlyProperty
  MOCK_CONST_METHOD0(read_only_property, std::string());

  // readonly attribute DOMString readOnlyTokenProperty
  MOCK_CONST_METHOD0(read_only_token_property, base::Token());

  MOCK_CONST_METHOD0(null_is_empty_property, std::string());
  MOCK_METHOD1(set_null_is_empty_property, void(const std::string&));

  MOCK_CONST_METHOD0(undefined_is_empty_property, std::string());
  MOCK_METHOD1(set_undefined_is_empty_property, void(const std::string&));

  MOCK_CONST_METHOD0(nullable_undefined_is_empty_property,
                     base::Optional<std::string>());
  MOCK_METHOD1(set_nullable_undefined_is_empty_property,
               void(const base::Optional<std::string>&));

  DEFINE_WRAPPABLE_TYPE(DOMStringTestInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_DOM_STRING_TEST_INTERFACE_H_
