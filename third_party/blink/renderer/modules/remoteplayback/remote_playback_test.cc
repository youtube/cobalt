// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_playback_availability_callback.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/remoteplayback/html_media_element_remote_playback.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockFunction : public ScriptFunction::Callable {
 public:
  MockFunction() = default;
  MOCK_METHOD2(Call, ScriptValue(ScriptState*, ScriptValue));
};

class MockEventListenerForRemotePlayback : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext* executionContext, Event*));
};

class MockPresentationController final : public PresentationController {
 public:
  explicit MockPresentationController(LocalDOMWindow& window)
      : PresentationController(window) {}
  ~MockPresentationController() override = default;

  MOCK_METHOD1(AddAvailabilityObserver,
               void(PresentationAvailabilityObserver*));
  MOCK_METHOD1(RemoveAvailabilityObserver,
               void(PresentationAvailabilityObserver*));
};
}  // namespace

class RemotePlaybackTest : public testing::Test,
                           private ScopedRemotePlaybackBackendForTest {
 public:
  RemotePlaybackTest() : ScopedRemotePlaybackBackendForTest(true) {}

 protected:
  void CancelPrompt(RemotePlayback& remote_playback) {
    remote_playback.PromptCancelled();
  }

  void SetState(RemotePlayback& remote_playback,
                mojom::blink::PresentationConnectionState state) {
    remote_playback.StateChanged(state);
  }

  bool IsListening(RemotePlayback& remote_playback) {
    return remote_playback.is_listening_;
  }
};

TEST_F(RemotePlaybackTest, PromptCancelledRejectsWithNotAllowedError) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  MockFunctionScope funcs(scope.GetScriptState());

  LocalFrame::NotifyUserActivation(
      &page_holder->GetFrame(), mojom::UserActivationNotificationType::kTest);
  remote_playback.prompt(scope.GetScriptState(), scope.GetExceptionState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
  CancelPrompt(remote_playback);

  // Runs pending promises.
  scope.PerformMicrotaskCheckpoint();
}

TEST_F(RemotePlaybackTest, PromptConnectedRejectsWhenCancelled) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  MockFunctionScope funcs(scope.GetScriptState());

  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTED);

  LocalFrame::NotifyUserActivation(
      &page_holder->GetFrame(), mojom::UserActivationNotificationType::kTest);
  remote_playback.prompt(scope.GetScriptState(), scope.GetExceptionState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
  CancelPrompt(remote_playback);

  // Runs pending promises.
  scope.PerformMicrotaskCheckpoint();
}

TEST_F(RemotePlaybackTest, PromptConnectedResolvesWhenDisconnected) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  MockFunctionScope funcs(scope.GetScriptState());

  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTED);

  LocalFrame::NotifyUserActivation(
      &page_holder->GetFrame(), mojom::UserActivationNotificationType::kTest);
  remote_playback.prompt(scope.GetScriptState(), scope.GetExceptionState())
      .Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  SetState(remote_playback, mojom::blink::PresentationConnectionState::CLOSED);

  // Runs pending promises.
  scope.PerformMicrotaskCheckpoint();
}

TEST_F(RemotePlaybackTest, StateChangeEvents) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  auto* connecting_handler = MakeGarbageCollected<
      testing::StrictMock<MockEventListenerForRemotePlayback>>();
  auto* connect_handler = MakeGarbageCollected<
      testing::StrictMock<MockEventListenerForRemotePlayback>>();
  auto* disconnect_handler = MakeGarbageCollected<
      testing::StrictMock<MockEventListenerForRemotePlayback>>();

  remote_playback.addEventListener(event_type_names::kConnecting,
                                   connecting_handler);
  remote_playback.addEventListener(event_type_names::kConnect, connect_handler);
  remote_playback.addEventListener(event_type_names::kDisconnect,
                                   disconnect_handler);

  // Verify a state changes when a route is connected and closed.
  EXPECT_CALL(*connecting_handler, Invoke(testing::_, testing::_)).Times(1);
  EXPECT_CALL(*connect_handler, Invoke(testing::_, testing::_)).Times(1);
  EXPECT_CALL(*disconnect_handler, Invoke(testing::_, testing::_)).Times(1);

  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTING);
  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTING);
  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTED);
  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTED);
  SetState(remote_playback, mojom::blink::PresentationConnectionState::CLOSED);
  SetState(remote_playback, mojom::blink::PresentationConnectionState::CLOSED);

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(connecting_handler);
  testing::Mock::VerifyAndClear(connect_handler);
  testing::Mock::VerifyAndClear(disconnect_handler);

  // Verify a state changes when a route is connected and terminated.
  EXPECT_CALL(*connecting_handler, Invoke(testing::_, testing::_)).Times(1);
  EXPECT_CALL(*connect_handler, Invoke(testing::_, testing::_)).Times(1);
  EXPECT_CALL(*disconnect_handler, Invoke(testing::_, testing::_)).Times(1);

  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTING);
  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTED);
  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::TERMINATED);

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(connecting_handler);
  testing::Mock::VerifyAndClear(connect_handler);
  testing::Mock::VerifyAndClear(disconnect_handler);

  // Verify we can connect after a route termination.
  EXPECT_CALL(*connecting_handler, Invoke(testing::_, testing::_)).Times(1);
  SetState(remote_playback,
           mojom::blink::PresentationConnectionState::CONNECTING);
  testing::Mock::VerifyAndClear(connecting_handler);
}

TEST_F(RemotePlaybackTest,
       DisableRemotePlaybackRejectsPromptWithInvalidStateError) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  MockFunctionScope funcs(scope.GetScriptState());

  LocalFrame::NotifyUserActivation(
      &page_holder->GetFrame(), mojom::UserActivationNotificationType::kTest);
  remote_playback.prompt(scope.GetScriptState(), scope.GetExceptionState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());
  HTMLMediaElementRemotePlayback::SetBooleanAttribute(
      *element, html_names::kDisableremoteplaybackAttr, true);

  // Runs pending promises.
  scope.PerformMicrotaskCheckpoint();
}

TEST_F(RemotePlaybackTest, DisableRemotePlaybackCancelsAvailabilityCallbacks) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  MockFunctionScope funcs(scope.GetScriptState());

  V8RemotePlaybackAvailabilityCallback* availability_callback =
      V8RemotePlaybackAvailabilityCallback::Create(funcs.ExpectNoCall());

  remote_playback
      .watchAvailability(scope.GetScriptState(), availability_callback,
                         scope.GetExceptionState())
      .Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  HTMLMediaElementRemotePlayback::SetBooleanAttribute(
      *element, html_names::kDisableremoteplaybackAttr, true);

  // Runs pending promises.
  scope.PerformMicrotaskCheckpoint();
}

TEST_F(RemotePlaybackTest, CallingWatchAvailabilityFromAvailabilityCallback) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  MockFunction* callback_function = MakeGarbageCollected<MockFunction>();
  V8RemotePlaybackAvailabilityCallback* availability_callback =
      V8RemotePlaybackAvailabilityCallback::Create(
          MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(),
                                               callback_function)
              ->V8Function());

  const int kNumberCallbacks = 10;
  for (int i = 0; i < kNumberCallbacks; ++i) {
    remote_playback.watchAvailability(scope.GetScriptState(),
                                      availability_callback,
                                      scope.GetExceptionState());
  }

  auto add_callback_lambda = [&]() {
    remote_playback.watchAvailability(scope.GetScriptState(),
                                      availability_callback,
                                      scope.GetExceptionState());
    return blink::ScriptValue::CreateNull(scope.GetScriptState()->GetIsolate());
  };

  // When the availability changes, we should get exactly kNumberCallbacks
  // calls, due to the kNumberCallbacks initial current callbacks. The extra
  // callbacks we are adding should not be executed.
  EXPECT_CALL(*callback_function, Call(testing::_, testing::_))
      .Times(kNumberCallbacks)
      .WillRepeatedly(testing::InvokeWithoutArgs(add_callback_lambda));

  remote_playback.AvailabilityChangedForTesting(true);

  scope.PerformMicrotaskCheckpoint();
  testing::Mock::VerifyAndClear(callback_function);

  // We now have twice as many callbacks as we started with, and should get
  // twice as many calls, but no more.
  EXPECT_CALL(*callback_function, Call(testing::_, testing::_))
      .Times(kNumberCallbacks * 2)
      .WillRepeatedly(testing::InvokeWithoutArgs(add_callback_lambda));

  remote_playback.AvailabilityChangedForTesting(false);

  scope.PerformMicrotaskCheckpoint();

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(callback_function);
}

TEST_F(RemotePlaybackTest, PromptThrowsWhenBackendDisabled) {
  ScopedRemotePlaybackBackendForTest remote_playback_backend(false);
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  LocalFrame::NotifyUserActivation(
      &page_holder->GetFrame(), mojom::UserActivationNotificationType::kTest);
  remote_playback.prompt(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST_F(RemotePlaybackTest, WatchAvailabilityWorksWhenBackendDisabled) {
  ScopedRemotePlaybackBackendForTest remote_playback_backend(false);
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  MockFunctionScope funcs(scope.GetScriptState());

  V8RemotePlaybackAvailabilityCallback* availability_callback =
      V8RemotePlaybackAvailabilityCallback::Create(funcs.ExpectNoCall());

  remote_playback
      .watchAvailability(scope.GetScriptState(), availability_callback,
                         scope.GetExceptionState())
      .Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  // Runs pending promises.
  scope.PerformMicrotaskCheckpoint();
}

TEST_F(RemotePlaybackTest, IsListening) {
  V8TestingScope scope;

  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  LocalDOMWindow& window = *page_holder->GetFrame().DomWindow();
  MockPresentationController* mock_controller =
      MakeGarbageCollected<MockPresentationController>(window);
  Supplement<LocalDOMWindow>::ProvideTo(
      window, static_cast<PresentationController*>(mock_controller));

  EXPECT_CALL(*mock_controller,
              AddAvailabilityObserver(testing::Eq(&remote_playback)))
      .Times(2);
  EXPECT_CALL(*mock_controller,
              RemoveAvailabilityObserver(testing::Eq(&remote_playback)))
      .Times(2);

  MockFunction* callback_function = MakeGarbageCollected<MockFunction>();
  V8RemotePlaybackAvailabilityCallback* availability_callback =
      V8RemotePlaybackAvailabilityCallback::Create(
          MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(),
                                               callback_function)
              ->V8Function());

  // The initial call upon registering will not happen as it's posted on the
  // message loop.
  EXPECT_CALL(*callback_function, Call(testing::_, testing::_)).Times(2);

  remote_playback.watchAvailability(
      scope.GetScriptState(), availability_callback, scope.GetExceptionState());

  ASSERT_TRUE(remote_playback.Urls().empty());
  ASSERT_FALSE(IsListening(remote_playback));

  remote_playback.SourceChanged(WebURL(KURL("http://www.example.com")), true);
  ASSERT_EQ((size_t)1, remote_playback.Urls().size());
  ASSERT_TRUE(IsListening(remote_playback));
  remote_playback.AvailabilityChanged(mojom::ScreenAvailability::AVAILABLE);

  remote_playback.cancelWatchAvailability(scope.GetScriptState(),
                                          scope.GetExceptionState());
  ASSERT_EQ((size_t)1, remote_playback.Urls().size());
  ASSERT_FALSE(IsListening(remote_playback));

  remote_playback.watchAvailability(
      scope.GetScriptState(), availability_callback, scope.GetExceptionState());
  ASSERT_EQ((size_t)1, remote_playback.Urls().size());
  ASSERT_TRUE(IsListening(remote_playback));
  remote_playback.AvailabilityChanged(mojom::ScreenAvailability::AVAILABLE);

  remote_playback.SourceChanged(WebURL(), false);
  ASSERT_TRUE(remote_playback.Urls().empty());
  ASSERT_FALSE(IsListening(remote_playback));

  remote_playback.SourceChanged(WebURL(KURL("@$@#@#")), true);
  ASSERT_TRUE(remote_playback.Urls().empty());
  ASSERT_FALSE(IsListening(remote_playback));

  // Runs pending promises.
  scope.PerformMicrotaskCheckpoint();

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(callback_function);
  testing::Mock::VerifyAndClear(mock_controller);
}

TEST_F(RemotePlaybackTest, NullContextDoesntCrash) {
  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  remote_playback.SetExecutionContext(nullptr);
  remote_playback.PromptInternal();
}

}  // namespace blink
