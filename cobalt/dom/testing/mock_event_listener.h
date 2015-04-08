/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_TESTING_MOCK_EVENT_LISTENER_H_
#define DOM_TESTING_MOCK_EVENT_LISTENER_H_

#include "cobalt/dom/event_listener.h"

#include "base/memory/ref_counted.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace dom {
namespace testing {

class MockEventListener : public EventListener {
 public:
  static scoped_refptr<MockEventListener> CreateAsAttribute() {
    return new ::testing::NiceMock<MockEventListener>(true);
  }

  static scoped_refptr<MockEventListener> CreateAsNonAttribute() {
    return new ::testing::NiceMock<MockEventListener>(false);
  }

  MOCK_METHOD1(HandleEvent, void(const scoped_refptr<Event>&));
  MOCK_METHOD1(EqualTo, bool(const EventListener&));
  MOCK_CONST_METHOD0(IsAttribute, bool());

 protected:
  explicit MockEventListener(bool is_attribute) {
    // We expect that EqualTo has its default behavior in most cases.
    ON_CALL(*this, EqualTo(::testing::_))
        .WillByDefault(::testing::Invoke(this, &MockEventListener::IsEqual));
    ON_CALL(*this, IsAttribute())
        .WillByDefault(::testing::Return(is_attribute));
  }

 private:
  bool IsEqual(const EventListener& that) const { return this == &that; }
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // DOM_TESTING_MOCK_EVENT_LISTENER_H_
