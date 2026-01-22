// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_platform_service/h_5_vcc_platform_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

WTF::Vector<uint8_t> ToWTFVector(DOMArrayBuffer* array_buffer) {
  if (!array_buffer || array_buffer->IsDetached()) {
    return WTF::Vector<uint8_t>();
  }

  size_t byte_length = array_buffer->ByteLength();
  if (byte_length == 0) {
    return WTF::Vector<uint8_t>();
  }

  const uint8_t* data = static_cast<const uint8_t*>(array_buffer->Data());
  DCHECK(data);

  WTF::Vector<uint8_t> vector;
  vector.Append(data, base::checked_cast<wtf_size_t>(byte_length));
  return vector;
}

DOMArrayBuffer* ToDOMArrayBuffer(const WTF::Vector<uint8_t>& vector) {
  if (vector.empty()) {
    // Create a zero-byte ArrayBuffer using the (void*, size_t) overload.
    return DOMArrayBuffer::Create(nullptr, static_cast<size_t>(0));
  }

  return DOMArrayBuffer::Create(vector.data(), vector.size());
}

}  // namespace

using ServiceManager =
    h5vcc_platform_service::mojom::blink::H5vccPlatformServiceManager;
using Service = h5vcc_platform_service::mojom::blink::PlatformService;
using ServiceObserver =
    h5vcc_platform_service::mojom::blink::PlatformServiceObserver;

// static
bool H5vccPlatformService::has(ScriptState* script_state,
                               const WTF::String& service_name) {
  if (!script_state || !script_state->ContextIsValid()) {
    return false;
  }
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context) {
    return false;
  }

  mojo::Remote<ServiceManager> manager;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      manager.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kNetworking)));

  bool exists = false;
  if (!manager->Has(service_name, &exists)) {
    DLOG(ERROR) << "H5vccPlatformService::has - Mojo call failed";
    return false;
  }

  return exists;
}

// static
H5vccPlatformService* H5vccPlatformService::open(
    ScriptState* script_state,
    const WTF::String& service_name,
    V8ReceiveCallback* receive_callback) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window || !receive_callback || !script_state->ContextIsValid()) {
    return nullptr;
  }

  // Perform a synchronous Mojo call so that a nullptr can be returned to the
  // web client if the platform does not have the service.
  if (!H5vccPlatformService::has(script_state, service_name)) {
    LOG(WARNING) << "H5vccPlatformService::open: " << service_name
                 << " could not be opened";
    return nullptr;
  }

  H5vccPlatformService* instance = MakeGarbageCollected<H5vccPlatformService>(
      *window, service_name, receive_callback);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Get the manager interface
  mojo::Remote<ServiceManager> manager;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      manager.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kNetworking)));
  manager.set_disconnect_handler(
      WTF::BindOnce(&H5vccPlatformService::OnManagerConnectionError,
                    WrapWeakPersistent(instance)));

  // Set up pipes for the instance-specific PlatformService
  mojo::PendingRemote<ServiceObserver> observer_remote =
      instance->observer_receiver_.BindNewPipeAndPassRemote(
          execution_context->GetTaskRunner(TaskType::kNetworking));

  mojo::PendingRemote<Service> service_remote;
  auto service_receiver = service_remote.InitWithNewPipeAndPassReceiver();

  // Bind the instance's remote to the new pipe
  instance->platform_service_remote_.Bind(
      std::move(service_remote),
      execution_context->GetTaskRunner(TaskType::kNetworking));
  instance->platform_service_remote_.set_disconnect_handler(
      WTF::BindOnce(&H5vccPlatformService::OnServiceConnectionError,
                    WrapWeakPersistent(instance)));

  // Open the service on the browser side, transferring the pipes
  manager->Open(service_name, std::move(observer_remote),
                std::move(service_receiver));

  // Note: there's no direct confirmation here that Open succeeded in the
  // browser. Errors will be handled by pipe disconnections.
  instance->service_opened_ = true;

  return instance;
}

H5vccPlatformService::H5vccPlatformService(LocalDOMWindow& window,
                                           const WTF::String& service_name,
                                           V8ReceiveCallback* receive_callback)
    : ExecutionContextLifecycleObserver(&window),
      service_name_(service_name),
      receive_callback_(receive_callback),
      platform_service_remote_(GetExecutionContext()),
      observer_receiver_(this, GetExecutionContext()) {}

H5vccPlatformService::~H5vccPlatformService() {
  // Ensures mojo pipes are closed if not done already
  close();
}

void H5vccPlatformService::OnManagerConnectionError() {
  DLOG(ERROR) << "H5vccPlatformServiceManager connection error";
  // If the manager connection drops, it doesn't necessarily mean the
  // individual service connection is bad, but it's suspicious.
  // We'll rely on the platform_service_remote_'s disconnect handler.
}

void H5vccPlatformService::OnServiceConnectionError() {
  DLOG(ERROR) << "PlatformService connection error for " << service_name_;
  close();
}

DOMArrayBuffer* H5vccPlatformService::send(DOMArrayBuffer* data,
                                           ExceptionState& exception_state) {
  if (!service_opened_ || !platform_service_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Service is diconnected or not open.");

    // Since the API is marked [RaisesException] and this error path throws a
    // DOM exception, no value is actually returned to the JavaScript client.
    return nullptr;
  }

  WTF::Vector<uint8_t> input_data = ToWTFVector(data);
  std::optional<WTF::Vector<uint8_t>> response_data;

  bool mojo_result = platform_service_remote_->Send(input_data, &response_data);

  if (!mojo_result) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Mojo call failed, connection lost.");
    close();

    // Since the API is marked [RaisesException] and this error path throws a
    // DOM exception, no value is actually returned to the JavaScript client.
    return nullptr;
  }

  if (!response_data.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Browser side could not send data to the service.");

    // Since the API is marked [RaisesException] and this error path throws a
    // DOM exception, no value is actually returned to the JavaScript client.
    return nullptr;
  }

  return ToDOMArrayBuffer(response_data.value());
}

void H5vccPlatformService::close() {
  if (service_opened_) {
    DLOG(INFO) << "Closing H5vccPlatformService: " << service_name_;
  }
  service_opened_ = false;
  platform_service_remote_.reset();
  observer_receiver_.reset();
  receive_callback_ = nullptr;
}

void H5vccPlatformService::ContextDestroyed() {
  close();
}

void H5vccPlatformService::OnDataReceived(const WTF::Vector<uint8_t>& data) {
  if (!receive_callback_) {
    DLOG(WARNING) << "H5vccPlatformService::OnDataReceived: dropping the "
                  << "message for " << service_name_ << " because the JS "
                  << "callback is not set. The service was probably closed.";
    return;
  }

  if (!GetExecutionContext()) {
    DLOG(WARNING) << "H5vccPlatformService::OnDataReceived: dropping the "
                  << "message for " << service_name_ << " because the "
                  << "ExecutionContext is no longer valid.";
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(GetExecutionContext());
  if (!script_state || !script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);  // For RAII

  DOMArrayBuffer* dom_buffer = ToDOMArrayBuffer(data);
  v8::Local<v8::Value> v8_data =
      ToV8Traits<DOMArrayBuffer>::ToV8(script_state, dom_buffer);
  ScriptValue script_data(script_state->GetIsolate(), v8_data);

  // nullptr is passed since the JS callback is not on any object instance.
  if (receive_callback_->Invoke(nullptr, this, script_data).IsNothing()) {
    DLOG(WARNING) << "The JavaScript callback provided for " << service_name_
                  << " threw an error";
  }
}

void H5vccPlatformService::Trace(Visitor* visitor) const {
  visitor->Trace(receive_callback_);
  visitor->Trace(platform_service_remote_);
  visitor->Trace(observer_receiver_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
