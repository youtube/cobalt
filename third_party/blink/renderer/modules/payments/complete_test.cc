// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::complete().

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

TEST(CompleteTest, CannotCallCompleteTwice) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());
  request->Complete(scope.GetScriptState(),
                    PaymentStateResolver::PaymentComplete::kFail,
                    ASSERT_NO_EXCEPTION);

  request->Complete(scope.GetScriptState(),
                    PaymentStateResolver::PaymentComplete::kSuccess,
                    scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST(CompleteTest, ResolveCompletePromiseOnUnknownError) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());

  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->Complete(scope.GetScriptState(),
                        PaymentStateResolver::PaymentComplete::kSuccess,
                        ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::UNKNOWN, "Unknown error.");
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST(CompleteTest, ResolveCompletePromiseOnUserClosingUI) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());

  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->Complete(scope.GetScriptState(),
                        PaymentStateResolver::PaymentComplete::kSuccess,
                        ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::USER_CANCEL,
      "User closed the UI.");
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

// If user cancels the transaction during processing, the complete() promise
// should be rejected.
TEST(CompleteTest, RejectCompletePromiseAfterError) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::USER_CANCEL,
      "User closed the UI.");

  request->Complete(scope.GetScriptState(),
                    PaymentStateResolver::PaymentComplete::kSuccess,
                    scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST(CompleteTest, ResolvePromiseOnComplete) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());

  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->Complete(scope.GetScriptState(),
                        PaymentStateResolver::PaymentComplete::kSuccess,
                        ASSERT_NO_EXCEPTION));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnComplete();
  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST(CompleteTest, RejectCompletePromiseOnUpdateDetailsFailure) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());

  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->Complete(scope.GetScriptState(),
                        PaymentStateResolver::PaymentComplete::kSuccess,
                        ASSERT_NO_EXCEPTION));

  request->OnUpdatePaymentDetailsFailure("oops");

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ("AbortError: oops", promise_tester.ValueAsString());
}

TEST(CompleteTest, RejectCompletePromiseAfterTimeout) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());
  request->OnCompleteTimeoutForTesting();

  request->Complete(scope.GetScriptState(),
                    PaymentStateResolver::PaymentComplete::kSuccess,
                    scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("Timed out after 60 seconds, complete() called too late",
            scope.GetExceptionState().Message());
}

}  // namespace
}  // namespace blink
