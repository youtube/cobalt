// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_

#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_delegation.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExceptionState;
class PaymentInstruments;
class ScriptPromiseResolver;
class ScriptPromise;
class ScriptState;
class ServiceWorkerRegistration;

class MODULES_EXPORT PaymentManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PaymentManager(ServiceWorkerRegistration*);

  PaymentManager(const PaymentManager&) = delete;
  PaymentManager& operator=(const PaymentManager&) = delete;

  PaymentInstruments* instruments();

  const String& userHint();
  void setUserHint(const String&);

  void Trace(Visitor*) const override;

  ScriptPromise enableDelegations(
      ScriptState*,
      const Vector<V8PaymentDelegation>& delegations,
      ExceptionState&);

 private:
  void OnServiceConnectionError();

  void OnEnableDelegationsResponse(
      payments::mojom::blink::PaymentHandlerStatus status);

  Member<ServiceWorkerRegistration> registration_;
  HeapMojoRemote<payments::mojom::blink::PaymentManager> manager_;
  Member<PaymentInstruments> instruments_;
  String user_hint_;
  Member<ScriptPromiseResolver> enable_delegations_resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_
