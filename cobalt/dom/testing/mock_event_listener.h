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

#ifndef COBALT_DOM_TESTING_MOCK_EVENT_LISTENER_H_
#define COBALT_DOM_TESTING_MOCK_EVENT_LISTENER_H_

#include <memory>

#include "cobalt/dom/event_listener.h"

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace dom {
namespace testing {

class MockEventListener : public EventListener {
 public:
  typedef base::Optional<bool> (*HandleEventFunction)(
      const scoped_refptr<script::Wrappable>&, const scoped_refptr<Event>&,
      bool*);

  static std::unique_ptr<MockEventListener> Create() {
    return std::unique_ptr<MockEventListener>(
        new ::testing::NiceMock<MockEventListener>());
  }

  MOCK_CONST_METHOD3(
      HandleEvent, base::Optional<bool>(const scoped_refptr<script::Wrappable>&,
                                        const scoped_refptr<Event>&, bool*));

  void ExpectHandleEventCall(const scoped_refptr<Event>& event,
                             const scoped_refptr<EventTarget>& target) {
    ExpectHandleEventCall(event, target, target, Event::kAtTarget, &DoNothing);
  }

  void ExpectHandleEventCall(const scoped_refptr<Event>& event,
                             const scoped_refptr<EventTarget>& target,
                             HandleEventFunction handle_event_function) {
    ExpectHandleEventCall(event, target, target, Event::kAtTarget,
                          handle_event_function);
  }

  void ExpectHandleEventCall(const scoped_refptr<Event>& event,
                             const scoped_refptr<EventTarget>& target,
                             const scoped_refptr<EventTarget>& current_target,
                             Event::EventPhase phase) {
    ExpectHandleEventCall(event, target, current_target, phase, &DoNothing);
  }

  void ExpectHandleEventCall(const scoped_refptr<Event>& event,
                             const scoped_refptr<EventTarget>& target,
                             const scoped_refptr<EventTarget>& current_target,
                             Event::EventPhase phase,
                             HandleEventFunction handle_event_function) {
    using ::testing::_;
    using ::testing::AllOf;
    using ::testing::Eq;
    using ::testing::Invoke;
    using ::testing::Pointee;
    using ::testing::Property;

    EXPECT_CALL(
        *this,
        HandleEvent(
            Eq(current_target.get()),
            AllOf(Eq(event.get()),
                  Pointee(Property(&Event::target, Eq(target.get()))),
                  Pointee(Property(&Event::current_target,
                                   Eq(current_target.get()))),
                  Pointee(Property(&Event::event_phase, Eq(phase))),
                  Pointee(Property(&Event::IsBeingDispatched, Eq(true)))),
            _))
        .WillOnce(Invoke(handle_event_function))
        .RetiresOnSaturation();
  }

  void ExpectHandleEventCall(const std::string type,
                             const scoped_refptr<EventTarget>& target) {
    using ::testing::_;
    using ::testing::AllOf;
    using ::testing::Eq;
    using ::testing::Invoke;
    using ::testing::Pointee;
    using ::testing::Property;

    EXPECT_CALL(
        *this,
        HandleEvent(
            Eq(target.get()),
            AllOf(Pointee(Property(&Event::type, Eq(type))),
                  Pointee(Property(&Event::target, Eq(target.get()))),
                  Pointee(Property(&Event::IsBeingDispatched, Eq(true)))),
            _))
        .WillOnce(Invoke(&DoNothing))
        .RetiresOnSaturation();
  }

  void ExpectNoHandleEventCall() {
    using ::testing::_;
    EXPECT_CALL(*this, HandleEvent(_, _, _)).Times(0);
  }

  static base::Optional<bool> DoNothing(const scoped_refptr<script::Wrappable>&,
                                        const scoped_refptr<Event>&,
                                        bool* had_exception) {
    *had_exception = false;
    return base::nullopt;
  }

  static base::Optional<bool> StopPropagation(
      const scoped_refptr<script::Wrappable>&,
      const scoped_refptr<Event>& event, bool* had_exception) {
    *had_exception = false;
    event->StopPropagation();
    return base::nullopt;
  }

  static base::Optional<bool> StopImmediatePropagation(
      const scoped_refptr<script::Wrappable>&,
      const scoped_refptr<Event>& event, bool* had_exception) {
    *had_exception = false;
    event->StopImmediatePropagation();
    return base::nullopt;
  }

  static base::Optional<bool> PreventDefault(
      const scoped_refptr<script::Wrappable>&,
      const scoped_refptr<Event>& event, bool* had_exception) {
    *had_exception = false;
    event->PreventDefault();
    return base::nullopt;
  }

  virtual ~MockEventListener() {}

 protected:
  MockEventListener() {
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;
    // No JavaScript exception, and no return value.
    ON_CALL(*this, HandleEvent(_, _, _))
        .WillByDefault(
            DoAll(SetArgPointee<2>(false), Return(base::Optional<bool>())));
  }
};
}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_MOCK_EVENT_LISTENER_H_
