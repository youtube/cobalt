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

#include <memory>

#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/node.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IgnoreResult;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Pointee;
using ::testing::Property;

namespace cobalt {
namespace dom {

using ::cobalt::script::testing::FakeScriptValue;
using web::testing::MockEventListener;

//////////////////////////////////////////////////////////////////////////
// NodeDispatchEventTest
//////////////////////////////////////////////////////////////////////////

class NodeDispatchEventTest : public ::testing::Test {
 protected:
  NodeDispatchEventTest();
  ~NodeDispatchEventTest() override;

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;

  scoped_refptr<Node> grand_parent_;
  scoped_refptr<Node> parent_;
  scoped_refptr<Node> child_;
  std::unique_ptr<MockEventListener> event_listener_capture_;
  std::unique_ptr<MockEventListener> event_listener_bubbling_;
};

NodeDispatchEventTest::NodeDispatchEventTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());

  document_ = new Document(&html_element_context_);

  grand_parent_ = new Element(document_);
  parent_ = grand_parent_->AppendChild(new Element(document_));
  child_ = parent_->AppendChild(new Element(document_));
  event_listener_capture_ = MockEventListener::Create();
  event_listener_bubbling_ = MockEventListener::Create();

  grand_parent_->AddEventListener(
      "fired",
      FakeScriptValue<web::EventListener>(event_listener_bubbling_.get()),
      false);
  grand_parent_->AddEventListener(
      "fired",
      FakeScriptValue<web::EventListener>(event_listener_capture_.get()), true);
  parent_->AddEventListener(
      "fired",
      FakeScriptValue<web::EventListener>(event_listener_bubbling_.get()),
      false);
  parent_->AddEventListener(
      "fired",
      FakeScriptValue<web::EventListener>(event_listener_capture_.get()), true);
  child_->AddEventListener(
      "fired",
      FakeScriptValue<web::EventListener>(event_listener_bubbling_.get()),
      false);
  child_->AddEventListener(
      "fired",
      FakeScriptValue<web::EventListener>(event_listener_capture_.get()), true);
}

NodeDispatchEventTest::~NodeDispatchEventTest() {
  grand_parent_ = NULL;
  parent_ = NULL;
  child_ = NULL;
  event_listener_capture_.reset();
  event_listener_bubbling_.reset();

  document_ = NULL;

  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(NodeDispatchEventTest, NonBubblingEventPropagation) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kNotBubbles,
                     web::Event::kNotCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(event, child_, grand_parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_capture_->ExpectHandleEventCall(event, child_, parent_,
                                                 web::Event::kCapturingPhase);
  // The event listeners called at target are always in the order of append.
  event_listener_bubbling_->ExpectHandleEventCall(event, child_, child_,
                                                  web::Event::kAtTarget);
  event_listener_capture_->ExpectHandleEventCall(event, child_, child_,
                                                 web::Event::kAtTarget);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, BubblingEventPropagation) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kBubbles,
                     web::Event::kNotCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(event, child_, grand_parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_capture_->ExpectHandleEventCall(event, child_, parent_,
                                                 web::Event::kCapturingPhase);
  // The event listeners called at target are always in the order of append.
  event_listener_bubbling_->ExpectHandleEventCall(event, child_, child_,
                                                  web::Event::kAtTarget);
  event_listener_capture_->ExpectHandleEventCall(event, child_, child_,
                                                 web::Event::kAtTarget);
  event_listener_bubbling_->ExpectHandleEventCall(event, child_, parent_,
                                                  web::Event::kBubblingPhase);
  event_listener_bubbling_->ExpectHandleEventCall(event, child_, grand_parent_,
                                                  web::Event::kBubblingPhase);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopPropagationAtCapturePhase) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kBubbles,
                     web::Event::kNotCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(event, child_, grand_parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_capture_->ExpectHandleEventCall(
      event, child_, parent_, web::Event::kCapturingPhase,
      &MockEventListener::StopPropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopPropagationAtAtTargetPhase) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kBubbles,
                     web::Event::kNotCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(event, child_, grand_parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_capture_->ExpectHandleEventCall(event, child_, parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_bubbling_->ExpectHandleEventCall(
      event, child_, child_, web::Event::kAtTarget,
      &MockEventListener::StopPropagation);
  event_listener_capture_->ExpectHandleEventCall(event, child_, child_,
                                                 web::Event::kAtTarget);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopPropagationAtBubblingPhase) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kBubbles,
                     web::Event::kNotCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(event, child_, grand_parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_capture_->ExpectHandleEventCall(event, child_, parent_,
                                                 web::Event::kCapturingPhase);
  // The event listeners called at target are always in the order of append.
  event_listener_bubbling_->ExpectHandleEventCall(event, child_, child_,
                                                  web::Event::kAtTarget);
  event_listener_capture_->ExpectHandleEventCall(event, child_, child_,
                                                 web::Event::kAtTarget);
  event_listener_bubbling_->ExpectHandleEventCall(
      event, child_, parent_, web::Event::kBubblingPhase,
      &MockEventListener::StopPropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, StopImmediatePropagation) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kBubbles,
                     web::Event::kNotCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(event, child_, grand_parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_capture_->ExpectHandleEventCall(event, child_, parent_,
                                                 web::Event::kCapturingPhase);
  event_listener_bubbling_->ExpectHandleEventCall(
      event, child_, child_, web::Event::kAtTarget,
      &MockEventListener::StopImmediatePropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, PreventDefaultOnNonCancelableEvent) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kBubbles,
                     web::Event::kNotCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(
      event, child_, grand_parent_, web::Event::kCapturingPhase,
      &MockEventListener::PreventDefault);
  event_listener_capture_->ExpectHandleEventCall(
      event, child_, parent_, web::Event::kCapturingPhase,
      &MockEventListener::StopPropagation);
  EXPECT_TRUE(child_->DispatchEvent(event));
}

TEST_F(NodeDispatchEventTest, PreventDefaultOnCancelableEvent) {
  scoped_refptr<web::Event> event =
      new web::Event(base_token::Token("fired"), web::Event::kBubbles,
                     web::Event::kCancelable);

  InSequence in_sequence;
  event_listener_capture_->ExpectHandleEventCall(
      event, child_, grand_parent_, web::Event::kCapturingPhase,
      &MockEventListener::PreventDefault);
  event_listener_capture_->ExpectHandleEventCall(
      event, child_, parent_, web::Event::kCapturingPhase,
      &MockEventListener::StopPropagation);
  EXPECT_FALSE(child_->DispatchEvent(event));
}

}  // namespace dom
}  // namespace cobalt
