// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial.h"

#include <inttypes.h>
#include <utility>

#include "base/unguessable_token.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_port_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_port_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

const char kContextGone[] = "Script context has shut down.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"serial\" is disallowed by permissions policy.";
const char kNoPortSelected[] = "No port selected by the user.";

String TokenToString(const base::UnguessableToken& token) {
  // TODO(crbug.com/918702): Implement HashTraits for UnguessableToken.
  return String::Format("%016" PRIX64 "%016" PRIX64,
                        token.GetHighForSerialization(),
                        token.GetLowForSerialization());
}

// Carries out basic checks for the web-exposed APIs, to make sure the minimum
// requirements for them to be served are met. Returns true if any conditions
// fail to be met, generating an appropriate exception as well. Otherwise,
// returns false to indicate the call should be allowed.
bool ShouldBlockSerialServiceCall(LocalDOMWindow* window,
                                  ExecutionContext* context,
                                  ExceptionState* exception_state) {
  if (!context) {
    if (exception_state) {
      exception_state->ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                         kContextGone);
    }

    return true;
  }

  // Rejects if the top-level frame has an opaque origin.
  const SecurityOrigin* security_origin = nullptr;
  if (context->IsWindow()) {
    security_origin =
        window->GetFrame()->Top()->GetSecurityContext()->GetSecurityOrigin();
  } else if (context->IsDedicatedWorkerGlobalScope()) {
    security_origin = static_cast<WorkerGlobalScope*>(context)
                          ->top_level_frame_security_origin();
  } else {
    NOTREACHED_NORETURN();
  }

  if (security_origin->IsOpaque()) {
    if (exception_state) {
      exception_state->ThrowSecurityError(
          "Access to the Web Serial API is denied from contexts where the "
          "top-level document has an opaque origin.");
    }
    return true;
  }

  if (!context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kSerial,
          ReportOptions::kReportOnFailure)) {
    if (exception_state) {
      exception_state->ThrowSecurityError(kFeaturePolicyBlocked);
    }
    return true;
  }

  return false;
}

}  // namespace

const char Serial::kSupplementName[] = "Serial";

Serial* Serial::serial(NavigatorBase& navigator) {
  Serial* serial = Supplement<NavigatorBase>::From<Serial>(navigator);
  if (!serial) {
    serial = MakeGarbageCollected<Serial>(navigator);
    ProvideTo(navigator, serial);
  }
  return serial;
}

Serial::Serial(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      service_(navigator.GetExecutionContext()),
      receiver_(this, navigator.GetExecutionContext()) {}

ExecutionContext* Serial::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& Serial::InterfaceName() const {
  return event_target_names::kSerial;
}

void Serial::ContextDestroyed() {
  for (auto& entry : port_cache_)
    entry.value->ContextDestroyed();
}

void Serial::OnPortAdded(mojom::blink::SerialPortInfoPtr port_info) {
  SerialPort* port = GetOrCreatePort(std::move(port_info));
  port->DispatchEvent(*Event::CreateBubble(event_type_names::kConnect));
}

void Serial::OnPortRemoved(mojom::blink::SerialPortInfoPtr port_info) {
  SerialPort* port = GetOrCreatePort(std::move(port_info));
  port->DispatchEvent(*Event::CreateBubble(event_type_names::kDisconnect));
}

ScriptPromise Serial::getPorts(ScriptState* script_state,
                               ExceptionState& exception_state) {
  if (ShouldBlockSerialServiceCall(GetSupplementable()->DomWindow(),
                                   GetExecutionContext(), &exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  get_ports_promises_.insert(resolver);

  EnsureServiceConnection();
  service_->GetPorts(WTF::BindOnce(&Serial::OnGetPorts, WrapPersistent(this),
                                   WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise Serial::requestPort(ScriptState* script_state,
                                  const SerialPortRequestOptions* options,
                                  ExceptionState& exception_state) {
  if (ShouldBlockSerialServiceCall(GetSupplementable()->DomWindow(),
                                   GetExecutionContext(), &exception_state)) {
    return ScriptPromise();
  }

  if (!LocalFrame::HasTransientUserActivation(DomWindow()->GetFrame())) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a permission request.");
    return ScriptPromise();
  }

  Vector<mojom::blink::SerialPortFilterPtr> filters;
  if (options && options->hasFilters()) {
    for (const auto& filter : options->filters()) {
      auto mojo_filter = mojom::blink::SerialPortFilter::New();

      mojo_filter->has_vendor_id = filter->hasUsbVendorId();
      if (mojo_filter->has_vendor_id) {
        mojo_filter->vendor_id = filter->usbVendorId();
      } else {
        exception_state.ThrowTypeError(
            "A filter must provide a property to filter by.");
        return ScriptPromise();
      }

      mojo_filter->has_product_id = filter->hasUsbProductId();
      if (mojo_filter->has_product_id) {
        if (!mojo_filter->has_vendor_id) {
          exception_state.ThrowTypeError(
              "A filter containing a usbProductId must also specify a "
              "usbVendorId.");
          return ScriptPromise();
        }
        mojo_filter->product_id = filter->usbProductId();
      }

      filters.push_back(std::move(mojo_filter));
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  request_port_promises_.insert(resolver);

  EnsureServiceConnection();
  service_->RequestPort(std::move(filters),
                        resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                            &Serial::OnRequestPort, WrapPersistent(this))));

  return resolver->Promise();
}

void Serial::OpenPort(
    const base::UnguessableToken& token,
    device::mojom::blink::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<device::mojom::blink::SerialPortClient> client,
    mojom::blink::SerialService::OpenPortCallback callback) {
  EnsureServiceConnection();
  service_->OpenPort(token, std::move(options), std::move(client),
                     std::move(callback));
}

void Serial::ForgetPort(
    const base::UnguessableToken& token,
    mojom::blink::SerialService::ForgetPortCallback callback) {
  EnsureServiceConnection();
  service_->ForgetPort(token, std::move(callback));
}

void Serial::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(receiver_);
  visitor->Trace(get_ports_promises_);
  visitor->Trace(request_port_promises_);
  visitor->Trace(port_cache_);
  EventTargetWithInlineData::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void Serial::AddedEventListener(const AtomicString& event_type,
                                RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);

  if (event_type != event_type_names::kConnect &&
      event_type != event_type_names::kDisconnect) {
    return;
  }

  if (ShouldBlockSerialServiceCall(GetSupplementable()->DomWindow(),
                                   GetExecutionContext(), nullptr)) {
    return;
  }

  EnsureServiceConnection();
}

void Serial::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (service_.is_bound())
    return;

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(WTF::BindOnce(
      &Serial::OnServiceConnectionError, WrapWeakPersistent(this)));

  service_->SetClient(receiver_.BindNewPipeAndPassRemote(task_runner));
}

void Serial::OnServiceConnectionError() {
  service_.reset();
  receiver_.reset();

  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> get_ports_promises;
  get_ports_promises_.swap(get_ports_promises);
  for (ScriptPromiseResolver* resolver : get_ports_promises)
    resolver->Resolve(HeapVector<Member<SerialPort>>());

  HeapHashSet<Member<ScriptPromiseResolver>> request_port_promises;
  request_port_promises_.swap(request_port_promises);
  for (ScriptPromiseResolver* resolver : request_port_promises) {
    ScriptState* resolver_script_state = resolver->GetScriptState();
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       resolver_script_state)) {
      continue;
    }
    ScriptState::Scope script_state_scope(resolver_script_state);
    resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                     kNoPortSelected);
  }
}

SerialPort* Serial::GetOrCreatePort(mojom::blink::SerialPortInfoPtr info) {
  auto it = port_cache_.find(TokenToString(info->token));
  if (it != port_cache_.end()) {
    return it->value;
  }

  SerialPort* port = MakeGarbageCollected<SerialPort>(this, std::move(info));
  port_cache_.insert(TokenToString(port->token()), port);
  return port;
}

void Serial::OnGetPorts(ScriptPromiseResolver* resolver,
                        Vector<mojom::blink::SerialPortInfoPtr> port_infos) {
  DCHECK(get_ports_promises_.Contains(resolver));
  get_ports_promises_.erase(resolver);

  HeapVector<Member<SerialPort>> ports;
  for (auto& port_info : port_infos)
    ports.push_back(GetOrCreatePort(std::move(port_info)));

  resolver->Resolve(ports);
}

void Serial::OnRequestPort(ScriptPromiseResolver* resolver,
                           mojom::blink::SerialPortInfoPtr port_info) {
  DCHECK(request_port_promises_.Contains(resolver));
  request_port_promises_.erase(resolver);

  if (!port_info) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                     kNoPortSelected);
    return;
  }

  resolver->Resolve(GetOrCreatePort(std::move(port_info)));
}

}  // namespace blink
