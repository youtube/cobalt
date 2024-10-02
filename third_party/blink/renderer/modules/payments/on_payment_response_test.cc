// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::OnPaymentResponse().

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_response.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/payments/payment_address.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_response.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"

namespace blink {
namespace {

// If the merchant requests shipping information, but the browser does not
// provide the shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingShippingOption) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser does not
// provide a shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingAddress) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests a payer name, but the browser does not provide it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingName) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests an email address, but the browser does not provide
// it, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingEmail) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests a phone number, but the browser does not provide it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingPhone) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser provides an
// empty string for shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyShippingOption) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser provides an
// empty shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyAddress) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests a payer name, but the browser provides an empty
// string for name, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyName) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->name = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests an email, but the browser provides an empty string
// for email, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyEmail) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests a phone number, but the browser provides an empty
// string for the phone number, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyPhone) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant does not request shipping information, but the browser
// provides a shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedAddress) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant does not request shipping information, but the browser
// provides a shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedShippingOption) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant does not request a payer name, but the browser provides it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedName) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->name = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant does not request an email, but the browser provides it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedEmail) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant does not request a phone number, but the browser provides it,
// reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedPhone) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = "";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser provides an
// invalid shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectInvalidAddress) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "Atlantis";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
}

class PaymentResponseFunction : public ScriptFunction::Callable {
 public:
  explicit PaymentResponseFunction(ScriptValue* out_value) : value_(out_value) {
    DCHECK(value_);
  }

  ScriptValue Call(ScriptState*, ScriptValue value) override {
    DCHECK(!value.IsEmpty());
    *value_ = value;
    return value;
  }

 private:
  ScriptValue* const value_;
};

// If the merchant requests shipping information, the resolved show() promise
// should contain a shipping option and an address.
TEST(OnPaymentResponseTest, CanRequestShippingInformation) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->shipping_option = "standardShipping";
  response->shipping_address = payments::mojom::blink::PaymentAddress::New();
  response->shipping_address->country = "US";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* resp = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());
  EXPECT_EQ("standardShipping", resp->shippingOption());
}

// If the merchant requests a payer name, the resolved show() promise should
// contain a payer name.
TEST(OnPaymentResponseTest, CanRequestName) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer = payments::mojom::blink::PayerDetail::New();
  response->payer->name = "Jon Doe";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* pr = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());
  EXPECT_EQ("Jon Doe", pr->payerName());
}

// If the merchant requests an email address, the resolved show() promise should
// contain an email address.
TEST(OnPaymentResponseTest, CanRequestEmail) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = "abc@gmail.com";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* pr = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());
  EXPECT_EQ("abc@gmail.com", pr->payerEmail());
}

// If the merchant requests a phone number, the resolved show() promise should
// contain a phone number.
TEST(OnPaymentResponseTest, CanRequestPhone) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = "0123";

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));
  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* pr = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());

  EXPECT_EQ("0123", pr->payerPhone());
}

// If the merchant does not request shipping information, the resolved show()
// promise should contain null shipping option and address.
TEST(OnPaymentResponseTest, ShippingInformationNotRequired) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());

  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* resp = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());
  EXPECT_TRUE(resp->shippingOption().IsNull());
  EXPECT_EQ(nullptr, resp->shippingAddress());
}

// If the merchant does not request a phone number, the resolved show() promise
// should contain null phone number.
TEST(OnPaymentResponseTest, PhoneNotRequired) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerPhone(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->phone = String();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* pr = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());
  EXPECT_TRUE(pr->payerPhone().IsNull());
}

// If the merchant does not request a payer name, the resolved show() promise
// should contain null payer name.
TEST(OnPaymentResponseTest, NameNotRequired) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerName(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->name = String();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* pr = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());
  EXPECT_TRUE(pr->payerName().IsNull());
}

// If the merchant does not request an email address, the resolved show()
// promise should contain null email address.
TEST(OnPaymentResponseTest, EmailNotRequired) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestPayerEmail(false);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  payments::mojom::blink::PaymentResponsePtr response =
      BuildPaymentResponseForTest();
  response->payer->email = String();

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptValue out_value;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                scope.GetScriptState(),
                MakeGarbageCollected<PaymentResponseFunction>(&out_value))
                ->V8Function(),
            funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(std::move(response));

  scope.PerformMicrotaskCheckpoint();
  PaymentResponse* pr = V8PaymentResponse::ToImplWithTypeCheck(
      scope.GetIsolate(), out_value.V8Value());
  EXPECT_TRUE(pr->payerEmail().IsNull());
}

}  // namespace
}  // namespace blink
