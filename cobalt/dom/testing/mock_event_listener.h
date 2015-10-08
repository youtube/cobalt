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
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace dom {
namespace testing {

using ::testing::Return;

class MockEventListener : public EventListener {
 public:
  static scoped_ptr<MockEventListener> CreateAsAttribute() {
    return make_scoped_ptr<MockEventListener>(
        new ::testing::NiceMock<MockEventListener>(true));
  }

  static scoped_ptr<MockEventListener> CreateAsNonAttribute() {
    return make_scoped_ptr<MockEventListener>(
        new ::testing::NiceMock<MockEventListener>(false));
  }

  MOCK_CONST_METHOD1(HandleEvent, void(const scoped_refptr<Event>&));
  MOCK_CONST_METHOD1(EqualTo, bool(const EventListener&));
  MOCK_CONST_METHOD0(IsAttribute, bool());

  virtual ~MockEventListener() {}

 protected:
  explicit MockEventListener(bool is_attribute) {
    // We expect that EqualTo has its default behavior in most cases.
    ON_CALL(*this, EqualTo(::testing::_))
        .WillByDefault(::testing::Invoke(this, &MockEventListener::IsEqual));
    ON_CALL(*this, IsAttribute()).WillByDefault(Return(is_attribute));
  }

 private:
  bool IsEqual(const EventListener& that) const { return this == &that; }
};

class FakeScriptObject : public script::ScriptObject<EventListener> {
 public:
  typedef script::ScriptObject<EventListener> BaseClass;
  explicit FakeScriptObject(const MockEventListener* listener)
      : mock_listener_(listener) {}

  void RegisterOwner(const script::Wrappable*) OVERRIDE {}
  void DeregisterOwner(const script::Wrappable*) OVERRIDE {}
  const EventListener* GetScriptObject(void) const OVERRIDE {
    return mock_listener_;
  }
  scoped_ptr<BaseClass> MakeCopy() const OVERRIDE {
    return make_scoped_ptr<BaseClass>(new FakeScriptObject(mock_listener_));
  }

 private:
  const MockEventListener* mock_listener_;
};
}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // DOM_TESTING_MOCK_EVENT_LISTENER_H_
