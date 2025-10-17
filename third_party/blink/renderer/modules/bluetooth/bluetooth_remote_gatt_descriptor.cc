// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_descriptor.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_service.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BluetoothRemoteGATTDescriptor::BluetoothRemoteGATTDescriptor(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,
    BluetoothRemoteGATTCharacteristic* characteristic)
    : descriptor_(std::move(descriptor)), characteristic_(characteristic) {}

void BluetoothRemoteGATTDescriptor::ReadValueCallback(
    ScriptPromiseResolver<NotShared<DOMDataView>>* resolver,
    mojom::blink::WebBluetoothResult result,
    base::span<const uint8_t> value) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    value_ = BluetoothRemoteGATTUtils::ConvertSpanToDataView(value);
    resolver->Resolve(value_);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise<NotShared<DOMDataView>> BluetoothRemoteGATTDescriptor::readValue(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!GetGatt()->connected() || !GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kGATT));
    return ScriptPromise<NotShared<DOMDataView>>();
  }

  if (!GetGatt()->device()->IsValidDescriptor(descriptor_->instance_id)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      CreateInvalidDescriptorErrorMessage());
    return ScriptPromise<NotShared<DOMDataView>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<NotShared<DOMDataView>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);
  GetBluetooth()->Service()->RemoteDescriptorReadValue(
      descriptor_->instance_id,
      WTF::BindOnce(&BluetoothRemoteGATTDescriptor::ReadValueCallback,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void BluetoothRemoteGATTDescriptor::WriteValueCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    DOMDataView* new_value,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the resolver is not in the set of ActiveAlgorithms then the frame
  // disconnected so we reject.
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    value_ = NotShared(new_value);
    resolver->Resolve();
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise<IDLUndefined> BluetoothRemoteGATTDescriptor::writeValue(
    ScriptState* script_state,
    base::span<const uint8_t> value,
    ExceptionState& exception_state) {
  if (!GetGatt()->connected() || !GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kGATT));
    return EmptyPromise();
  }

  if (!GetGatt()->device()->IsValidDescriptor(descriptor_->instance_id)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      CreateInvalidDescriptorErrorMessage());
    return EmptyPromise();
  }

  // Partial implementation of writeValue algorithm:
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothremotegattdescriptor-writevalue

  // If bytes is more than 512 bytes long (the maximum length of an attribute
  // value, per Long Attribute Values) return a promise rejected with an
  // InvalidModificationError and abort.
  if (value.size() > 512) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Value can't exceed 512 bytes.");
    return EmptyPromise();
  }

  // Let newValue be a copy of the bytes held by value.
  NotShared<DOMDataView> new_value =
      BluetoothRemoteGATTUtils::ConvertSpanToDataView(value);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);
  GetBluetooth()->Service()->RemoteDescriptorWriteValue(
      descriptor_->instance_id, value,
      WTF::BindOnce(&BluetoothRemoteGATTDescriptor::WriteValueCallback,
                    WrapPersistent(this), WrapPersistent(resolver),
                    WrapPersistent(new_value.Get())));

  return promise;
}

String BluetoothRemoteGATTDescriptor::CreateInvalidDescriptorErrorMessage() {
  return "Descriptor with UUID " + uuid() +
         " is no longer valid. Remember to retrieve the Descriptor again "
         "after reconnecting.";
}

void BluetoothRemoteGATTDescriptor::Trace(Visitor* visitor) const {
  visitor->Trace(characteristic_);
  visitor->Trace(value_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
