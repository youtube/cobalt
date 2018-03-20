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

#ifndef COBALT_BINDINGS_TESTING_PROMISE_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_PROMISE_INTERFACE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class PromiseInterface : public script::Wrappable {
 public:
  MOCK_METHOD0(ReturnVoidPromise, script::Handle<script::Promise<void>>());
  MOCK_METHOD0(ReturnBooleanPromise, script::Handle<script::Promise<bool>>());
  MOCK_METHOD0(ReturnStringPromise,
               script::Handle<script::Promise<std::string>>());
  MOCK_METHOD0(ReturnInterfacePromise,
               script::Handle<script::Promise<scoped_refptr<Wrappable>>>());

  MOCK_METHOD0(OnSuccess, void());

  DEFINE_WRAPPABLE_TYPE(PromiseInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_PROMISE_INTERFACE_H_
