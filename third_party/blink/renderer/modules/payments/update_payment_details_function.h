// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_UPDATE_PAYMENT_DETAILS_FUNCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_UPDATE_PAYMENT_DETAILS_FUNCTION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

namespace blink {

class PaymentRequestDelegate;
class ScriptState;
class ScriptValue;

class UpdatePaymentDetailsFunction : public ScriptFunction::Callable {
 public:
  enum class ResolveType {
    kFulfill,
    kReject,
  };

  UpdatePaymentDetailsFunction(PaymentRequestDelegate*, ResolveType);
  void Trace(Visitor*) const override;
  ScriptValue Call(ScriptState*, ScriptValue) override;

 private:
  Member<PaymentRequestDelegate> delegate_;
  ResolveType resolve_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_UPDATE_PAYMENT_DETAILS_FUNCTION_H_
