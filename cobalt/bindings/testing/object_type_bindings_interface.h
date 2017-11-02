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

#ifndef COBALT_BINDINGS_TESTING_OBJECT_TYPE_BINDINGS_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_OBJECT_TYPE_BINDINGS_INTERFACE_H_

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/base_interface.h"
#include "cobalt/bindings/testing/derived_interface.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class ObjectTypeBindingsInterface : public script::Wrappable {
 public:
  MOCK_METHOD0(arbitrary_object, scoped_refptr<ArbitraryInterface>());
  MOCK_METHOD1(set_arbitrary_object,
               void(const scoped_refptr<ArbitraryInterface>&));
  MOCK_METHOD0(base_interface, scoped_refptr<BaseInterface>());
  MOCK_METHOD0(derived_interface, scoped_refptr<DerivedInterface>());
  MOCK_METHOD1(set_derived_interface,
               void(const scoped_refptr<DerivedInterface>&));
  MOCK_METHOD0(object_property, const script::ValueHandleHolder*());
  MOCK_METHOD1(set_object_property, void(const script::ValueHandleHolder&));

  DEFINE_WRAPPABLE_TYPE(ObjectTypeBindingsInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_OBJECT_TYPE_BINDINGS_INTERFACE_H_
