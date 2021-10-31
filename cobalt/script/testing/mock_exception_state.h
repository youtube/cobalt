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

#ifndef COBALT_SCRIPT_TESTING_MOCK_EXCEPTION_STATE_H_
#define COBALT_SCRIPT_TESTING_MOCK_EXCEPTION_STATE_H_

#include "cobalt/script/exception_state.h"

#include "base/memory/ref_counted.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace script {
namespace testing {

class MockExceptionState : public ExceptionState {
 public:
  MOCK_METHOD1(SetException, void(const scoped_refptr<ScriptException>&));
  MOCK_METHOD3(SetSimpleExceptionVA,
               void(SimpleExceptionType, const char*, va_list &));
};

}  // namespace testing
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_TESTING_MOCK_EXCEPTION_STATE_H_
