// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

ServiceWorkerClient::ServiceWorkerClient(
    const mojom::blink::ServiceWorkerClientInfo& info)
    : uuid_(info.client_uuid),
      url_(info.url.GetString()),
      type_(info.client_type),
      frame_type_(info.frame_type),
      lifecycle_state_(info.lifecycle_state) {}

ServiceWorkerClient::~ServiceWorkerClient() = default;

String ServiceWorkerClient::type() const {
  switch (type_) {
    case mojom::ServiceWorkerClientType::kWindow:
      return "window";
    case mojom::ServiceWorkerClientType::kDedicatedWorker:
      return "worker";
    case mojom::ServiceWorkerClientType::kSharedWorker:
      return "sharedworker";
    case mojom::ServiceWorkerClientType::kAll:
      NOTREACHED();
      return String();
  }

  NOTREACHED();
  return String();
}

String ServiceWorkerClient::frameType(ScriptState* script_state) const {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kServiceWorkerClientFrameType);
  switch (frame_type_) {
    case mojom::RequestContextFrameType::kAuxiliary:
      return "auxiliary";
    case mojom::RequestContextFrameType::kNested:
      return "nested";
    case mojom::RequestContextFrameType::kNone:
      return "none";
    case mojom::RequestContextFrameType::kTopLevel:
      return "top-level";
  }

  NOTREACHED();
  return String();
}

String ServiceWorkerClient::lifecycleState() const {
  switch (lifecycle_state_) {
    case mojom::ServiceWorkerClientLifecycleState::kActive:
      return "active";
    case mojom::ServiceWorkerClientLifecycleState::kFrozen:
      return "frozen";
  }

  NOTREACHED();
  return String();
}

void ServiceWorkerClient::postMessage(ScriptState* script_state,
                                      const ScriptValue& message,
                                      HeapVector<ScriptValue>& transfer,
                                      ExceptionState& exception_state) {
  PostMessageOptions* options = PostMessageOptions::Create();
  if (!transfer.empty())
    options->setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void ServiceWorkerClient::postMessage(ScriptState* script_state,
                                      const ScriptValue& message,
                                      const PostMessageOptions* options,
                                      ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  Transferables transferables;

  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByCopy(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);

  BlinkTransferableMessage msg;
  msg.message = serialized_message;
  msg.sender_origin = context->GetSecurityOrigin()->IsolatedCopy();
  msg.ports = MessagePort::DisentanglePorts(
      context, transferables.message_ports, exception_state);
  if (exception_state.HadException())
    return;

  msg.sender_agent_cluster_id = context->GetAgentClusterID();
  msg.locked_to_sender_agent_cluster = msg.message->IsLockedToAgentCluster();

  To<ServiceWorkerGlobalScope>(context)
      ->GetServiceWorkerHost()
      ->PostMessageToClient(uuid_, std::move(msg));
}

}  // namespace blink
