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

#include "cobalt/dom/node.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/stats.h"
#include "cobalt/dom/testing/fake_node.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::_;

namespace cobalt {
namespace dom {

using testing::FakeScriptObject;
using testing::MockEventListener;

namespace {

void ExpectHandleEventCall(const MockEventListener* listener,
                           const scoped_refptr<Event>& event,
                           const scoped_refptr<EventTarget>& target,
                           const scoped_refptr<EventTarget>& current_target,
                           Event::EventPhase phase) {
  // Note that we must pass the raw pointer to avoid reference counting issue.
  EXPECT_CALL(
      *listener,
      HandleEvent(AllOf(
          Eq(event.get()), Pointee(Property(&Event::target, Eq(target.get()))),
          Pointee(Property(&Event::current_target, Eq(current_target.get()))),
          Pointee(Property(&Event::event_phase, Eq(phase))),
          Pointee(Property(&Event::IsBeingDispatched, Eq(true))))))
      .RetiresOnSaturation();
}

void ExpectHandleEventCall(const MockEventListener* listener,
                           const scoped_refptr<Event>& event,
                           const scoped_refptr<EventTarget>& target,
                           const scoped_refptr<EventTarget>& current_target,
                           Event::EventPhase phase,
                           void (Event::*function)(void)) {
  // Note that we must pass the raw pointer to avoid reference counting issue.
  EXPECT_CALL(
      *listener,
      HandleEvent(AllOf(
          Eq(event.get()), Pointee(Property(&Event::target, Eq(target.get()))),
          Pointee(Property(&Event::current_target, Eq(current_target.get()))),
          Pointee(Property(&Event::event_phase, Eq(phase))),
          Pointee(Property(&Event::IsBeingDispatched, Eq(true))))))
      .WillOnce(InvokeWithoutArgs(event.get(), function))
      .RetiresOnSaturation();
}

}  // namespace

//////////////////////////////////////////////////////////////////////////
// NodeDispatchEventTest
//////////////////////////////////////////////////////////////////////////

class NodeDispatchEventTest : public ::testing::Test {
 protected:
  NodeDispatchEventTest();
  ~NodeDispatchEventTest() OVERRIDE;

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;

  scoped_refptr<Node> grand_parent_;
  scoped_refptr<Node> parent_;
  scoped_refptr<Node> child_;
  scoped_ptr<MockEventListener> event_listener_capture_;
  scoped_ptr<MockEventListener> event_listener_bubbling_;
};

NodeDispatchEventTest::NodeDispatchEventTest()
    : html_element_context_(NULL, NULL, NULL, NULL, NULL, NULL, NULL) {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());

  document_ = new Document(&html_element_context_, Document::Options());

  grand_parent_ = new FakeNode(document_);
  parent_ = grand_parent_->AppendChild(new FakeNode(document_));
  child_ = parent_->AppendChild(new FakeNode(document_));
  event_listener_capture_ = MockEventListener::CreateAsNonAttribute();
  event_listener_bubbling_ = MockEventListener::CreateAsNonAttribute();

  grand_parent_->AddEventListener(
      "fired", FakeScriptObject(event_listener_bubbling_.get()), false);
  grand_parent_->AddEventListener(
      "fired", FakeScriptObject(event_listener_capture_.get()), true);
  parent_->AddEventListener(
      "fired", FakeScriptObject(event_listener_bubbling_.get()), false);
  parent_->AddEventListener(
      "fired", FakeScriptObject(event_listener_capture_.get()), true);
  child_->AddEventListener(
      "fired", FakeScriptObject(event_listener_bubbling_.get()), false);
  child_->AddEventListener(
      "fired", FakeScriptObject(event_listener_capture_.get()), true);
}

NodeDispatchEventTest::~NodeDispatchEventTest() {
  grand_parent_ = NULL;
  parent_ = NULL;
  child_ = NULL;
  event_listener_capture_.reset();
  event_listener_bubbling_.reset();

  document_ = NULL;

  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(NodeDispatchEventTest, NonBubblingEventPropagation) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kNotBubbles, Event::kNotCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase);
  // The event listeners called at target are always in the order of append.
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_, child_,
                        Event::kAtTarget);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, child_,
                        Event::kAtTarget);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, BubblingEventPropagation) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kBubbles, Event::kNotCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase);
  // The event listeners called at target are always in the order of append.
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_, child_,
                        Event::kAtTarget);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, child_,
                        Event::kAtTarget);
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_, parent_,
                        Event::kBubblingPhase);
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_,
                        grand_parent_, Event::kBubblingPhase);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopPropagationAtCapturePhase) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kBubbles, Event::kNotCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase, &Event::StopPropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopPropagationAtAtTargetPhase) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kBubbles, Event::kNotCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_, child_,
                        Event::kAtTarget, &Event::StopPropagation);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, child_,
                        Event::kAtTarget);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopPropagationAtBubblingPhase) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kBubbles, Event::kNotCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase);
  // The event listeners called at target are always in the order of append.
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_, child_,
                        Event::kAtTarget);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, child_,
                        Event::kAtTarget);
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_, parent_,
                        Event::kBubblingPhase, &Event::StopPropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopImmediatePropagation) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kBubbles, Event::kNotCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase);
  ExpectHandleEventCall(event_listener_bubbling_.get(), event, child_, child_,
                        Event::kAtTarget, &Event::StopImmediatePropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, PreventDefaultOnNonCancelableEvent) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kBubbles, Event::kNotCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase,
                        &Event::PreventDefault);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase, &Event::StopPropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, PreventDefaultOnCancelableEvent) {
  scoped_refptr<Event> event =
      new Event("fired", Event::kBubbles, Event::kCancelable);

  InSequence in_sequence;
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_,
                        grand_parent_, Event::kCapturingPhase,
                        &Event::PreventDefault);
  ExpectHandleEventCall(event_listener_capture_.get(), event, child_, parent_,
                        Event::kCapturingPhase, &Event::StopPropagation);
  EXPECT_FALSE(child_->DispatchEvent(event));
}

}  // namespace dom
}  // namespace cobalt
