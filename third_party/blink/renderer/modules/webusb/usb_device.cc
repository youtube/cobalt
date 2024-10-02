// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_device.h"

#include <utility>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_control_transfer_parameters.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/webusb/usb.h"
#include "third_party/blink/renderer/modules/webusb/usb_configuration.h"
#include "third_party/blink/renderer/modules/webusb/usb_in_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_isochronous_in_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_isochronous_out_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_out_transfer_result.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::UsbClaimInterfaceResult;
using device::mojom::blink::UsbControlTransferParamsPtr;
using device::mojom::blink::UsbControlTransferRecipient;
using device::mojom::blink::UsbControlTransferType;
using device::mojom::blink::UsbDevice;
using device::mojom::blink::UsbDeviceInfoPtr;
using device::mojom::blink::UsbIsochronousPacketPtr;
using device::mojom::blink::UsbOpenDeviceError;
using device::mojom::blink::UsbTransferDirection;
using device::mojom::blink::UsbTransferStatus;

namespace blink {

namespace {

const char kAccessDeniedError[] = "Access denied.";
const char kBufferTooBig[] = "The data buffer exceeded its maximum size.";
const char kDetachedBuffer[] = "The data buffer has been detached.";
const char kDeviceStateChangeInProgress[] =
    "An operation that changes the device state is in progress.";
const char kDeviceDisconnected[] = "The device was disconnected.";
const char kInterfaceNotFound[] =
    "The interface number provided is not supported by the device in its "
    "current configuration.";
const char kInterfaceStateChangeInProgress[] =
    "An operation that changes interface state is in progress.";
const char kOpenRequired[] = "The device must be opened first.";
const char kProtectedInterfaceClassError[] =
    "The requested interface implements a protected class.";
const char kTransferPermissionDeniedError[] = "The transfer was not allowed.";

bool CheckFatalTransferStatus(ScriptPromiseResolver* resolver,
                              const UsbTransferStatus& status) {
  switch (status) {
    case UsbTransferStatus::TRANSFER_ERROR:
      resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                       "A transfer error has occurred.");
      return true;
    case UsbTransferStatus::PERMISSION_DENIED:
      resolver->RejectWithSecurityError(kTransferPermissionDeniedError,
                                        kTransferPermissionDeniedError);
      return true;
    case UsbTransferStatus::TIMEOUT:
      resolver->RejectWithDOMException(DOMExceptionCode::kTimeoutError,
                                       "The transfer timed out.");
      return true;
    case UsbTransferStatus::CANCELLED:
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                       "The transfer was cancelled.");
      return true;
    case UsbTransferStatus::DISCONNECT:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                       kDeviceDisconnected);
      return true;
    case UsbTransferStatus::COMPLETED:
    case UsbTransferStatus::STALLED:
    case UsbTransferStatus::BABBLE:
    case UsbTransferStatus::SHORT_PACKET:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

String ConvertTransferStatus(const UsbTransferStatus& status) {
  switch (status) {
    case UsbTransferStatus::COMPLETED:
    case UsbTransferStatus::SHORT_PACKET:
      return "ok";
    case UsbTransferStatus::STALLED:
      return "stall";
    case UsbTransferStatus::BABBLE:
      return "babble";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

USBDevice::USBDevice(USB* parent,
                     UsbDeviceInfoPtr device_info,
                     mojo::PendingRemote<UsbDevice> device,
                     ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      parent_(parent),
      device_info_(std::move(device_info)),
      device_(context),
      opened_(false),
      device_state_change_in_progress_(false),
      configuration_index_(kNotFound) {
  device_.Bind(std::move(device),
               context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  if (device_.is_bound()) {
    device_.set_disconnect_handler(
        WTF::BindOnce(&USBDevice::OnConnectionError, WrapWeakPersistent(this)));
  }

  for (wtf_size_t i = 0; i < Info().configurations.size(); ++i)
    configurations_.push_back(USBConfiguration::Create(this, i));

  wtf_size_t configuration_index =
      FindConfigurationIndex(Info().active_configuration);
  if (configuration_index != kNotFound)
    OnConfigurationSelected(true /* success */, configuration_index);
}

USBDevice::~USBDevice() {
  // |m_device| may still be valid but there should be no more outstanding
  // requests because each holds a persistent handle to this object.
  DCHECK(device_requests_.empty());
}

bool USBDevice::IsInterfaceClaimed(wtf_size_t configuration_index,
                                   wtf_size_t interface_index) const {
  return configuration_index_ != kNotFound &&
         configuration_index_ == configuration_index &&
         claimed_interfaces_[interface_index];
}

wtf_size_t USBDevice::SelectedAlternateInterfaceIndex(
    wtf_size_t interface_index) const {
  return selected_alternate_indices_[interface_index];
}

USBConfiguration* USBDevice::configuration() const {
  if (configuration_index_ == kNotFound)
    return nullptr;
  DCHECK_LT(configuration_index_, configurations_.size());
  return configurations_[configuration_index_];
}

HeapVector<Member<USBConfiguration>> USBDevice::configurations() const {
  return configurations_;
}

ScriptPromise USBDevice::open(ScriptState* script_state,
                              ExceptionState& exception_state) {
  EnsureNoDeviceOrInterfaceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (opened_)
    return ScriptPromise::CastUndefined(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_state_change_in_progress_ = true;
  device_requests_.insert(resolver);
  device_->Open(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&USBDevice::AsyncOpen, WrapPersistent(this))));
  return promise;
}

ScriptPromise USBDevice::close(ScriptState* script_state,
                               ExceptionState& exception_state) {
  EnsureNoDeviceOrInterfaceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (!opened_)
    return ScriptPromise::CastUndefined(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_state_change_in_progress_ = true;
  device_requests_.insert(resolver);
  device_->Close(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&USBDevice::AsyncClose, WrapPersistent(this))));
  return promise;
}

ScriptPromise USBDevice::forget(ScriptState* script_state,
                                ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Script context has shut down.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  parent_->ForgetDevice(device_info_->guid,
                        resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                            &USBDevice::AsyncForget, WrapPersistent(this))));

  return promise;
}

ScriptPromise USBDevice::selectConfiguration(ScriptState* script_state,
                                             uint8_t configuration_value,
                                             ExceptionState& exception_state) {
  EnsureNoDeviceOrInterfaceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (!opened_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOpenRequired);
    return ScriptPromise();
  }

  wtf_size_t configuration_index = FindConfigurationIndex(configuration_value);
  if (configuration_index == kNotFound) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The configuration value provided is not supported by the device.");
    return ScriptPromise();
  }

  if (configuration_index_ == configuration_index)
    return ScriptPromise::CastUndefined(script_state);

  device_state_change_in_progress_ = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->SetConfiguration(configuration_value,
                            resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                                &USBDevice::AsyncSelectConfiguration,
                                WrapPersistent(this), configuration_index)));
  return promise;
}

ScriptPromise USBDevice::claimInterface(ScriptState* script_state,
                                        uint8_t interface_number,
                                        ExceptionState& exception_state) {
  EnsureDeviceConfigured(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  wtf_size_t interface_index = FindInterfaceIndex(interface_number);
  if (interface_index == kNotFound) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      kInterfaceNotFound);
    return ScriptPromise();
  }

  if (interface_state_change_in_progress_[interface_index]) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInterfaceStateChangeInProgress);
    return ScriptPromise();
  }

  if (claimed_interfaces_[interface_index])
    return ScriptPromise::CastUndefined(script_state);

  interface_state_change_in_progress_[interface_index] = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->ClaimInterface(interface_number,
                          resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                              &USBDevice::AsyncClaimInterface,
                              WrapPersistent(this), interface_index)));
  return promise;
}

ScriptPromise USBDevice::releaseInterface(ScriptState* script_state,
                                          uint8_t interface_number,
                                          ExceptionState& exception_state) {
  EnsureDeviceConfigured(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  wtf_size_t interface_index = FindInterfaceIndex(interface_number);
  if (interface_index == kNotFound) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The interface number provided is not supported by the device in its "
        "current configuration.");
    return ScriptPromise();
  }

  if (interface_state_change_in_progress_[interface_index]) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInterfaceStateChangeInProgress);
    return ScriptPromise();
  }

  if (!claimed_interfaces_[interface_index])
    return ScriptPromise::CastUndefined(script_state);

  // Mark this interface's endpoints unavailable while its state is
  // changing.
  SetEndpointsForInterface(interface_index, false);
  interface_state_change_in_progress_[interface_index] = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->ReleaseInterface(interface_number,
                            resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                                &USBDevice::AsyncReleaseInterface,
                                WrapPersistent(this), interface_index)));
  return promise;
}

ScriptPromise USBDevice::selectAlternateInterface(
    ScriptState* script_state,
    uint8_t interface_number,
    uint8_t alternate_setting,
    ExceptionState& exception_state) {
  EnsureInterfaceClaimed(interface_number, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // TODO(reillyg): This is duplicated work.
  wtf_size_t interface_index = FindInterfaceIndex(interface_number);
  DCHECK_NE(interface_index, kNotFound);
  wtf_size_t alternate_index =
      FindAlternateIndex(interface_index, alternate_setting);
  if (alternate_index == kNotFound) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The alternate setting provided is not supported by the device in its "
        "current configuration.");
    return ScriptPromise();
  }

  // Mark this old alternate interface's endpoints unavailable while
  // the change is in progress.
  SetEndpointsForInterface(interface_index, false);
  interface_state_change_in_progress_[interface_index] = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->SetInterfaceAlternateSetting(
      interface_number, alternate_setting,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &USBDevice::AsyncSelectAlternateInterface, WrapPersistent(this),
          interface_index, alternate_index)));
  return promise;
}

ScriptPromise USBDevice::controlTransferIn(
    ScriptState* script_state,
    const USBControlTransferParameters* setup,
    unsigned length,
    ExceptionState& exception_state) {
  EnsureNoDeviceOrInterfaceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (!opened_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOpenRequired);
    return ScriptPromise();
  }

  auto parameters = ConvertControlTransferParameters(setup, exception_state);
  if (!parameters)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->ControlTransferIn(
      std::move(parameters), length, 0,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &USBDevice::AsyncControlTransferIn, WrapPersistent(this))));
  return promise;
}

ScriptPromise USBDevice::controlTransferOut(
    ScriptState* script_state,
    const USBControlTransferParameters* setup,
    ExceptionState& exception_state) {
  EnsureNoDeviceOrInterfaceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (!opened_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOpenRequired);
    return ScriptPromise();
  }

  auto parameters = ConvertControlTransferParameters(setup, exception_state);
  if (!parameters)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->ControlTransferOut(
      std::move(parameters), Vector<uint8_t>(), 0,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &USBDevice::AsyncControlTransferOut, WrapPersistent(this), 0)));
  return promise;
}

ScriptPromise USBDevice::controlTransferOut(
    ScriptState* script_state,
    const USBControlTransferParameters* setup,
    const DOMArrayPiece& data,
    ExceptionState& exception_state) {
  DCHECK(!data.IsNull());

  EnsureNoDeviceOrInterfaceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (!opened_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOpenRequired);
    return ScriptPromise();
  }

  auto parameters = ConvertControlTransferParameters(setup, exception_state);
  if (!parameters)
    return ScriptPromise();

  if (data.IsDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDetachedBuffer);
    return ScriptPromise();
  }

  if (data.ByteLength() > std::numeric_limits<uint32_t>::max()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      kBufferTooBig);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  auto transfer_length = static_cast<uint32_t>(data.ByteLength());
  device_requests_.insert(resolver);
  device_->ControlTransferOut(std::move(parameters),
                              base::make_span(data.Bytes(), data.ByteLength()),
                              0,
                              resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                                  &USBDevice::AsyncControlTransferOut,
                                  WrapPersistent(this), transfer_length)));
  return promise;
}

ScriptPromise USBDevice::clearHalt(ScriptState* script_state,
                                   String direction,
                                   uint8_t endpoint_number,
                                   ExceptionState& exception_state) {
  UsbTransferDirection mojo_direction = direction == "in"
                                            ? UsbTransferDirection::INBOUND
                                            : UsbTransferDirection::OUTBOUND;
  EnsureEndpointAvailable(direction == "in", endpoint_number, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->ClearHalt(mojo_direction, endpoint_number,
                     resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                         &USBDevice::AsyncClearHalt, WrapPersistent(this))));
  return promise;
}

ScriptPromise USBDevice::transferIn(ScriptState* script_state,
                                    uint8_t endpoint_number,
                                    unsigned length,
                                    ExceptionState& exception_state) {
  EnsureEndpointAvailable(/*in_transfer=*/true, endpoint_number,
                          exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->GenericTransferIn(
      endpoint_number, length, 0,
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&USBDevice::AsyncTransferIn, WrapPersistent(this))));
  return promise;
}

ScriptPromise USBDevice::transferOut(ScriptState* script_state,
                                     uint8_t endpoint_number,
                                     const DOMArrayPiece& data,
                                     ExceptionState& exception_state) {
  DCHECK(!data.IsNull());

  EnsureEndpointAvailable(/*in_transfer=*/false, endpoint_number,
                          exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (data.IsDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDetachedBuffer);
    return ScriptPromise();
  }

  if (data.ByteLength() > std::numeric_limits<uint32_t>::max()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      kBufferTooBig);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  auto transfer_length = static_cast<uint32_t>(data.ByteLength());
  device_requests_.insert(resolver);
  device_->GenericTransferOut(
      endpoint_number, base::make_span(data.Bytes(), data.ByteLength()), 0,
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&USBDevice::AsyncTransferOut, WrapPersistent(this),
                        transfer_length)));
  return promise;
}

ScriptPromise USBDevice::isochronousTransferIn(
    ScriptState* script_state,
    uint8_t endpoint_number,
    Vector<unsigned> packet_lengths,
    ExceptionState& exception_state) {
  EnsureEndpointAvailable(/*in_transfer=*/true, endpoint_number,
                          exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->IsochronousTransferIn(
      endpoint_number, packet_lengths, 0,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &USBDevice::AsyncIsochronousTransferIn, WrapPersistent(this))));
  return promise;
}

ScriptPromise USBDevice::isochronousTransferOut(
    ScriptState* script_state,
    uint8_t endpoint_number,
    const DOMArrayPiece& data,
    Vector<unsigned> packet_lengths,
    ExceptionState& exception_state) {
  DCHECK(!data.IsNull());

  EnsureEndpointAvailable(/*in_transfer=*/false, endpoint_number,
                          exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (data.IsDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDetachedBuffer);
    return ScriptPromise();
  }

  if (data.ByteLength() > std::numeric_limits<wtf_size_t>::max()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      kBufferTooBig);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->IsochronousTransferOut(
      endpoint_number, base::make_span(data.Bytes(), data.ByteLength()),
      packet_lengths, 0,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &USBDevice::AsyncIsochronousTransferOut, WrapPersistent(this))));
  return promise;
}

ScriptPromise USBDevice::reset(ScriptState* script_state,
                               ExceptionState& exception_state) {
  EnsureNoDeviceOrInterfaceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (!opened_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOpenRequired);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  device_requests_.insert(resolver);
  device_->Reset(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&USBDevice::AsyncReset, WrapPersistent(this))));
  return promise;
}

void USBDevice::ContextDestroyed() {
  device_requests_.clear();
}

void USBDevice::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(device_);
  visitor->Trace(device_requests_);
  visitor->Trace(configurations_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

wtf_size_t USBDevice::FindConfigurationIndex(
    uint8_t configuration_value) const {
  const auto& configurations = Info().configurations;
  for (wtf_size_t i = 0; i < configurations.size(); ++i) {
    if (configurations[i]->configuration_value == configuration_value)
      return i;
  }
  return kNotFound;
}

wtf_size_t USBDevice::FindInterfaceIndex(uint8_t interface_number) const {
  DCHECK_NE(configuration_index_, kNotFound);
  const auto& interfaces =
      Info().configurations[configuration_index_]->interfaces;
  for (wtf_size_t i = 0; i < interfaces.size(); ++i) {
    if (interfaces[i]->interface_number == interface_number)
      return i;
  }
  return kNotFound;
}

wtf_size_t USBDevice::FindAlternateIndex(uint32_t interface_index,
                                         uint8_t alternate_setting) const {
  DCHECK_NE(configuration_index_, kNotFound);
  const auto& alternates = Info()
                               .configurations[configuration_index_]
                               ->interfaces[interface_index]
                               ->alternates;
  for (wtf_size_t i = 0; i < alternates.size(); ++i) {
    if (alternates[i]->alternate_setting == alternate_setting)
      return i;
  }
  return kNotFound;
}

void USBDevice::EnsureNoDeviceChangeInProgress(
    ExceptionState& exception_state) const {
  if (!device_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      kDeviceDisconnected);
    return;
  }

  if (device_state_change_in_progress_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDeviceStateChangeInProgress);
    return;
  }
}

void USBDevice::EnsureNoDeviceOrInterfaceChangeInProgress(
    ExceptionState& exception_state) const {
  EnsureNoDeviceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return;

  if (AnyInterfaceChangeInProgress()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInterfaceStateChangeInProgress);
    return;
  }
}

void USBDevice::EnsureDeviceConfigured(ExceptionState& exception_state) const {
  EnsureNoDeviceChangeInProgress(exception_state);
  if (exception_state.HadException())
    return;

  if (!opened_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOpenRequired);
    return;
  }

  if (configuration_index_ == kNotFound) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The device must have a configuration selected.");
    return;
  }
}

void USBDevice::EnsureInterfaceClaimed(uint8_t interface_number,
                                       ExceptionState& exception_state) const {
  EnsureDeviceConfigured(exception_state);
  if (exception_state.HadException())
    return;

  wtf_size_t interface_index = FindInterfaceIndex(interface_number);
  if (interface_index == kNotFound) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      kInterfaceNotFound);
    return;
  }

  if (interface_state_change_in_progress_[interface_index]) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInterfaceStateChangeInProgress);
    return;
  }

  if (!claimed_interfaces_[interface_index]) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The specified interface has not been claimed.");
    return;
  }
}

void USBDevice::EnsureEndpointAvailable(bool in_transfer,
                                        uint8_t endpoint_number,
                                        ExceptionState& exception_state) const {
  EnsureDeviceConfigured(exception_state);
  if (exception_state.HadException())
    return;

  if (endpoint_number == 0 || endpoint_number >= kEndpointsBitsNumber) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The specified endpoint number is out of range.");
    return;
  }

  auto& bit_vector = in_transfer ? in_endpoints_ : out_endpoints_;
  if (!bit_vector[endpoint_number - 1]) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The specified endpoint is not part of a claimed and selected "
        "alternate interface.");
    return;
  }
}

bool USBDevice::AnyInterfaceChangeInProgress() const {
  for (wtf_size_t i = 0; i < interface_state_change_in_progress_.size(); ++i) {
    if (interface_state_change_in_progress_[i])
      return true;
  }
  return false;
}

UsbControlTransferParamsPtr USBDevice::ConvertControlTransferParameters(
    const USBControlTransferParameters* parameters,
    ExceptionState& exception_state) const {
  auto mojo_parameters = device::mojom::blink::UsbControlTransferParams::New();

  switch (parameters->requestType().AsEnum()) {
    case V8USBRequestType::Enum::kStandard:
      mojo_parameters->type = UsbControlTransferType::STANDARD;
      break;
    case V8USBRequestType::Enum::kClass:
      mojo_parameters->type = UsbControlTransferType::CLASS;
      break;
    case V8USBRequestType::Enum::kVendor:
      mojo_parameters->type = UsbControlTransferType::VENDOR;
      break;
  }

  switch (parameters->recipient().AsEnum()) {
    case V8USBRecipient::Enum::kDevice:
      mojo_parameters->recipient = UsbControlTransferRecipient::DEVICE;
      break;
    case V8USBRecipient::Enum::kInterface: {
      uint8_t interface_number = parameters->index() & 0xff;
      EnsureInterfaceClaimed(interface_number, exception_state);
      if (exception_state.HadException())
        return nullptr;

      mojo_parameters->recipient = UsbControlTransferRecipient::INTERFACE;
      break;
    }
    case V8USBRecipient::Enum::kEndpoint: {
      bool in_transfer = parameters->index() & 0x80;
      uint8_t endpoint_number = parameters->index() & 0x0f;
      EnsureEndpointAvailable(in_transfer, endpoint_number, exception_state);
      if (exception_state.HadException())
        return nullptr;

      mojo_parameters->recipient = UsbControlTransferRecipient::ENDPOINT;
      break;
    }
    case V8USBRecipient::Enum::kOther:
      mojo_parameters->recipient = UsbControlTransferRecipient::OTHER;
      break;
  }

  mojo_parameters->request = parameters->request();
  mojo_parameters->value = parameters->value();
  mojo_parameters->index = parameters->index();
  return mojo_parameters;
}

void USBDevice::SetEndpointsForInterface(wtf_size_t interface_index, bool set) {
  const auto& configuration = *Info().configurations[configuration_index_];
  const auto& interface = *configuration.interfaces[interface_index];
  const auto& alternate =
      *interface.alternates[selected_alternate_indices_[interface_index]];
  for (const auto& endpoint : alternate.endpoints) {
    uint8_t endpoint_number = endpoint->endpoint_number;
    if (endpoint_number == 0 || endpoint_number >= kEndpointsBitsNumber)
      continue;  // Ignore endpoints with invalid indices.
    auto& bit_vector = endpoint->direction == UsbTransferDirection::INBOUND
                           ? in_endpoints_
                           : out_endpoints_;
    if (set)
      bit_vector.set(endpoint_number - 1);
    else
      bit_vector.reset(endpoint_number - 1);
  }
}

void USBDevice::AsyncOpen(ScriptPromiseResolver* resolver,
                          device::mojom::blink::UsbOpenDeviceResultPtr result) {
  MarkRequestComplete(resolver);

  if (result->is_success()) {
    OnDeviceOpenedOrClosed(/*opened=*/true);
    resolver->Resolve();
    return;
  }

  DCHECK(result->is_error());
  switch (result->get_error()) {
    case UsbOpenDeviceError::ACCESS_DENIED:
      OnDeviceOpenedOrClosed(false /* not opened */);
      resolver->RejectWithSecurityError(kAccessDeniedError, kAccessDeniedError);
      break;
    case UsbOpenDeviceError::ALREADY_OPEN:
      // This class keeps track of open state and won't try to open a device
      // that is already open.
      NOTREACHED();
      break;
  }
}

void USBDevice::AsyncClose(ScriptPromiseResolver* resolver) {
  MarkRequestComplete(resolver);

  OnDeviceOpenedOrClosed(false /* closed */);
  resolver->Resolve();
}

void USBDevice::AsyncForget(ScriptPromiseResolver* resolver) {
  resolver->Resolve();
}

void USBDevice::OnDeviceOpenedOrClosed(bool opened) {
  opened_ = opened;
  if (!opened_) {
    claimed_interfaces_.Fill(false);
    selected_alternate_indices_.Fill(0);
    in_endpoints_.reset();
    out_endpoints_.reset();
  }
  device_state_change_in_progress_ = false;
}

void USBDevice::AsyncSelectConfiguration(wtf_size_t configuration_index,
                                         ScriptPromiseResolver* resolver,
                                         bool success) {
  MarkRequestComplete(resolver);

  OnConfigurationSelected(success, configuration_index);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Unable to set device configuration.");
  }
}

void USBDevice::OnConfigurationSelected(bool success,
                                        wtf_size_t configuration_index) {
  if (success) {
    configuration_index_ = configuration_index;
    wtf_size_t num_interfaces =
        Info().configurations[configuration_index_]->interfaces.size();
    claimed_interfaces_.resize(num_interfaces);
    claimed_interfaces_.Fill(false);
    interface_state_change_in_progress_.resize(num_interfaces);
    interface_state_change_in_progress_.Fill(false);
    selected_alternate_indices_.resize(num_interfaces);
    selected_alternate_indices_.Fill(0);
    in_endpoints_.reset();
    out_endpoints_.reset();
  }
  device_state_change_in_progress_ = false;
}

void USBDevice::AsyncClaimInterface(
    wtf_size_t interface_index,
    ScriptPromiseResolver* resolver,
    device::mojom::blink::UsbClaimInterfaceResult result) {
  MarkRequestComplete(resolver);

  OnInterfaceClaimedOrUnclaimed(result == UsbClaimInterfaceResult::kSuccess,
                                interface_index);

  switch (result) {
    case UsbClaimInterfaceResult::kSuccess:
      resolver->Resolve();
      break;
    case UsbClaimInterfaceResult::kProtectedClass:
      GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "An attempt to claim a USB device interface "
              "has been blocked because it "
              "implements a protected interface class."));
      resolver->RejectWithSecurityError(kProtectedInterfaceClassError,
                                        kProtectedInterfaceClassError);
      break;
    case UsbClaimInterfaceResult::kFailure:
      resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                       "Unable to claim interface.");
      break;
  }
}

void USBDevice::AsyncReleaseInterface(wtf_size_t interface_index,
                                      ScriptPromiseResolver* resolver,
                                      bool success) {
  MarkRequestComplete(resolver);

  OnInterfaceClaimedOrUnclaimed(!success, interface_index);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Unable to release interface.");
  }
}

void USBDevice::OnInterfaceClaimedOrUnclaimed(bool claimed,
                                              wtf_size_t interface_index) {
  if (claimed) {
    claimed_interfaces_[interface_index] = true;
  } else {
    claimed_interfaces_[interface_index] = false;
    selected_alternate_indices_[interface_index] = 0;
  }
  SetEndpointsForInterface(interface_index, claimed);
  interface_state_change_in_progress_[interface_index] = false;
}

void USBDevice::AsyncSelectAlternateInterface(wtf_size_t interface_index,
                                              wtf_size_t alternate_index,
                                              ScriptPromiseResolver* resolver,
                                              bool success) {
  MarkRequestComplete(resolver);

  if (success)
    selected_alternate_indices_[interface_index] = alternate_index;
  SetEndpointsForInterface(interface_index, success);
  interface_state_change_in_progress_[interface_index] = false;

  if (success) {
    resolver->Resolve();
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Unable to set device interface.");
  }
}

void USBDevice::AsyncControlTransferIn(ScriptPromiseResolver* resolver,
                                       UsbTransferStatus status,
                                       base::span<const uint8_t> data) {
  MarkRequestComplete(resolver);

  if (CheckFatalTransferStatus(resolver, status))
    return;

  resolver->Resolve(
      USBInTransferResult::Create(ConvertTransferStatus(status), data));
}

void USBDevice::AsyncControlTransferOut(uint32_t transfer_length,
                                        ScriptPromiseResolver* resolver,
                                        UsbTransferStatus status) {
  MarkRequestComplete(resolver);

  if (CheckFatalTransferStatus(resolver, status))
    return;

  resolver->Resolve(USBOutTransferResult::Create(ConvertTransferStatus(status),
                                                 transfer_length));
}

void USBDevice::AsyncClearHalt(ScriptPromiseResolver* resolver, bool success) {
  MarkRequestComplete(resolver);

  if (success) {
    resolver->Resolve();
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Unable to clear endpoint.");
  }
}

void USBDevice::AsyncTransferIn(ScriptPromiseResolver* resolver,
                                UsbTransferStatus status,
                                base::span<const uint8_t> data) {
  MarkRequestComplete(resolver);

  if (CheckFatalTransferStatus(resolver, status))
    return;

  resolver->Resolve(
      USBInTransferResult::Create(ConvertTransferStatus(status), data));
}

void USBDevice::AsyncTransferOut(uint32_t transfer_length,
                                 ScriptPromiseResolver* resolver,
                                 UsbTransferStatus status) {
  MarkRequestComplete(resolver);

  if (CheckFatalTransferStatus(resolver, status))
    return;

  resolver->Resolve(USBOutTransferResult::Create(ConvertTransferStatus(status),
                                                 transfer_length));
}

void USBDevice::AsyncIsochronousTransferIn(
    ScriptPromiseResolver* resolver,
    base::span<const uint8_t> data,
    Vector<UsbIsochronousPacketPtr> mojo_packets) {
  MarkRequestComplete(resolver);

  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(data.data(), data.size());
  HeapVector<Member<USBIsochronousInTransferPacket>> packets;
  packets.reserve(mojo_packets.size());
  uint32_t byte_offset = 0;
  for (const auto& packet : mojo_packets) {
    if (CheckFatalTransferStatus(resolver, packet->status))
      return;

    DOMDataView* data_view = nullptr;
    if (buffer) {
      data_view =
          DOMDataView::Create(buffer, byte_offset, packet->transferred_length);
    }
    packets.push_back(USBIsochronousInTransferPacket::Create(
        ConvertTransferStatus(packet->status),
        NotShared<DOMDataView>(data_view)));
    byte_offset += packet->length;
  }
  resolver->Resolve(USBIsochronousInTransferResult::Create(buffer, packets));
}

void USBDevice::AsyncIsochronousTransferOut(
    ScriptPromiseResolver* resolver,
    Vector<UsbIsochronousPacketPtr> mojo_packets) {
  MarkRequestComplete(resolver);

  HeapVector<Member<USBIsochronousOutTransferPacket>> packets;
  packets.reserve(mojo_packets.size());
  for (const auto& packet : mojo_packets) {
    if (CheckFatalTransferStatus(resolver, packet->status))
      return;

    packets.push_back(USBIsochronousOutTransferPacket::Create(
        ConvertTransferStatus(packet->status), packet->transferred_length));
  }
  resolver->Resolve(USBIsochronousOutTransferResult::Create(packets));
}

void USBDevice::AsyncReset(ScriptPromiseResolver* resolver, bool success) {
  MarkRequestComplete(resolver);

  if (success) {
    resolver->Resolve();
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Unable to reset the device.");
  }
}

void USBDevice::OnConnectionError() {
  device_.reset();
  opened_ = false;

  for (auto& resolver : device_requests_) {
    ScriptState* script_state = resolver->GetScriptState();
    if (IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                      script_state)) {
      ScriptState::Scope script_state_scope(script_state);
      resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                       kDeviceDisconnected);
    }
  }
}

void USBDevice::MarkRequestComplete(ScriptPromiseResolver* resolver) {
  auto request_entry = device_requests_.find(resolver);
  // Since all callbacks are wrapped with a check that the execution context is
  // still valid we can guarantee that `device_requests_` hasn't been cleared
  // yet if we are in this function.
  DCHECK(request_entry != device_requests_.end());
  device_requests_.erase(request_entry);
}

}  // namespace blink
