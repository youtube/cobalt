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

#ifndef COBALT_BINDINGS_TESTING_NULLABLE_TYPES_TEST_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_NULLABLE_TYPES_TEST_INTERFACE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/test_dictionary.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class NullableTypesTestInterface : public script::Wrappable {
 public:
  MOCK_METHOD0(nullable_boolean_property, base::Optional<bool>());
  MOCK_METHOD1(set_nullable_boolean_property, void(base::Optional<bool>));

  MOCK_METHOD0(nullable_numeric_property, base::Optional<int32_t>());
  MOCK_METHOD1(set_nullable_numeric_property, void(base::Optional<int32_t>));

  MOCK_METHOD0(nullable_string_property, base::Optional<std::string>());
  MOCK_METHOD1(set_nullable_string_property, void(base::Optional<std::string>));

  MOCK_METHOD0(nullable_dictionary_property, base::Optional<TestDictionary>());
  MOCK_METHOD1(set_nullable_dictionary_property,
               void(base::Optional<TestDictionary>));

  MOCK_METHOD0(nullable_object_property, scoped_refptr<ArbitraryInterface>());
  MOCK_METHOD1(set_nullable_object_property,
               void(scoped_refptr<ArbitraryInterface>));

  MOCK_METHOD0(NullableBooleanOperation, base::Optional<bool>());
  MOCK_METHOD0(NullableNumericOperation, base::Optional<int32_t>());
  MOCK_METHOD0(NullableStringOperation, base::Optional<std::string>());
  MOCK_METHOD0(NullableDictionaryOperation, base::Optional<TestDictionary>());
  MOCK_METHOD0(NullableObjectOperation, scoped_refptr<ArbitraryInterface>());

  MOCK_METHOD1(NullableBooleanArgument, void(base::Optional<bool>));
  MOCK_METHOD1(NullableNumericArgument, void(base::Optional<int32_t>));
  MOCK_METHOD1(NullableStringArgument, void(base::Optional<std::string>));
  MOCK_METHOD1(NullableDictionaryArgument,
               void(base::Optional<TestDictionary>));
  MOCK_METHOD1(NullableObjectArgument, void(scoped_refptr<ArbitraryInterface>));

  DEFINE_WRAPPABLE_TYPE(NullableTypesTestInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_NULLABLE_TYPES_TEST_INTERFACE_H_
