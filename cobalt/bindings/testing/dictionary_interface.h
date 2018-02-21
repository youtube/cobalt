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

#ifndef COBALT_BINDINGS_TESTING_DICTIONARY_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_DICTIONARY_INTERFACE_H_

#include <string>

#include "cobalt/bindings/testing/derived_dictionary.h"
#include "cobalt/bindings/testing/dictionary_with_dictionary_member.h"
#include "cobalt/bindings/testing/test_dictionary.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class DictionaryInterface : public script::Wrappable {
 public:
  MOCK_METHOD1(DictionaryOperation,
               void(const TestDictionary& test_dictionary));
  MOCK_METHOD1(DerivedDictionaryOperation,
               void(const DerivedDictionary& derived_dictionary));

  MOCK_METHOD0(dictionary_sequence, TestDictionary());
  MOCK_METHOD1(set_dictionary_sequence,
               void(script::Sequence<TestDictionary> test_dictionary));

  void TestOperation(DictionaryWithDictionaryMember /* dict */) {}

  DEFINE_WRAPPABLE_TYPE(DictionaryInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_DICTIONARY_INTERFACE_H_
