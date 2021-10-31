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

#ifndef COBALT_BINDINGS_TESTING_UNION_TYPES_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_UNION_TYPES_INTERFACE_H_

#include <string>

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/base_interface.h"
#include "cobalt/bindings/testing/derived_dictionary.h"
#include "cobalt/bindings/testing/test_dictionary.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class UnionTypesInterface : public script::Wrappable {
 public:
  typedef script::UnionType4<std::string, bool,
                             scoped_refptr<ArbitraryInterface>,
                             int32_t> UnionPropertyType;
  typedef base::Optional<script::UnionType2<double, std::string> >
      NullableUnionPropertyType;
  typedef script::UnionType2<scoped_refptr<BaseInterface>, std::string>
      UnionBasePropertyType;

  using UnionDictPropertyType =
      script::UnionType3<std::string, double, TestDictionary>;
  using UnionObjectsPropertyType =
      script::UnionType2<DerivedDictionary, scoped_refptr<ArbitraryInterface>>;

  MOCK_METHOD0(union_property, UnionPropertyType());
  MOCK_METHOD1(set_union_property, void(const UnionPropertyType&));

  MOCK_METHOD0(union_with_nullable_member_property,
               NullableUnionPropertyType());
  MOCK_METHOD1(set_union_with_nullable_member_property,
               void(const NullableUnionPropertyType&));

  MOCK_METHOD0(nullable_union_property, NullableUnionPropertyType());
  MOCK_METHOD1(set_nullable_union_property,
               void(const NullableUnionPropertyType&));

  MOCK_METHOD0(union_base_property, UnionBasePropertyType());
  MOCK_METHOD1(set_union_base_property, void(const UnionBasePropertyType&));

  MOCK_METHOD0(union_with_dictionary_property, UnionDictPropertyType());
  MOCK_METHOD1(set_union_with_dictionary_property,
               void(const UnionDictPropertyType &));

  MOCK_METHOD0(union_dicts_objects_property, UnionObjectsPropertyType());
  MOCK_METHOD1(set_union_dicts_objects_property,
               void(const UnionObjectsPropertyType &));

  DEFINE_WRAPPABLE_TYPE(UnionTypesInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_UNION_TYPES_INTERFACE_H_
