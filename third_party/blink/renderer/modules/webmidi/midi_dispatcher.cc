// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webmidi/midi_dispatcher.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {
// The maximum number of bytes which we're allowed to send to the browser
// before getting acknowledgement back from the browser that they've been
// successfully sent.
static const size_t kMaxUnacknowledgedBytesSent = 10 * 1024 * 1024;  // 10 MB.
}  // namespace

MIDIDispatcher::MIDIDispatcher(ExecutionContext* execution_context)
    : midi_session_(execution_context),
      receiver_(this, execution_context),
      midi_session_provider_(execution_context) {
  TRACE_EVENT0("midi", "MIDIDispatcher::MIDIDispatcher");
  // See https://bit.ly/2S0zRAS for task types.
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      midi_session_provider_.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI)));
  midi_session_provider_->StartSession(
      midi_session_.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI)),
      receiver_.BindNewPipeAndPassRemote(
          execution_context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI)));
}

MIDIDispatcher::~MIDIDispatcher() = default;

void MIDIDispatcher::SendMIDIData(uint32_t port,
                                  const uint8_t* data,
                                  wtf_size_t length,
                                  base::TimeTicks timestamp) {
  if ((kMaxUnacknowledgedBytesSent - unacknowledged_bytes_sent_) < length) {
    // TODO(toyoshim): buffer up the data to send at a later time.
    // For now we're just dropping these bytes on the floor.
    return;
  }

  unacknowledged_bytes_sent_ += length;
  Vector<uint8_t> v;
  v.Append(data, length);
  midi_session_->SendData(port, std::move(v), timestamp);
}

void MIDIDispatcher::AddInputPort(midi::mojom::blink::PortInfoPtr info) {
  DCHECK(client_);
  inputs_.push_back(*info);
  if (initialized_) {
    client_->DidAddInputPort(info->id, info->manufacturer, info->name,
                             info->version, info->state);
  }
}

void MIDIDispatcher::AddOutputPort(midi::mojom::blink::PortInfoPtr info) {
  DCHECK(client_);
  outputs_.push_back(*info);
  if (initialized_) {
    client_->DidAddOutputPort(info->id, info->manufacturer, info->name,
                              info->version, info->state);
  }
}

void MIDIDispatcher::SetInputPortState(uint32_t port,
                                       midi::mojom::blink::PortState state) {
  DCHECK(client_);
  if (inputs_[port].state == state)
    return;
  inputs_[port].state = state;
  if (initialized_)
    client_->DidSetInputPortState(port, state);
}

void MIDIDispatcher::SetOutputPortState(uint32_t port,
                                        midi::mojom::blink::PortState state) {
  DCHECK(client_);
  if (outputs_[port].state == state)
    return;
  outputs_[port].state = state;
  if (initialized_)
    client_->DidSetOutputPortState(port, state);
}

void MIDIDispatcher::SessionStarted(midi::mojom::blink::Result result) {
  TRACE_EVENT0("midi", "MIDIDispatcher::OnSessionStarted");

  // We always have a valid instance in `client_` in the production code, but
  // just in case to be robust for mojo injections and code changes in the
  // future. Other methods protect accesses to `client_` by `initialized_` flag
  // that is set below.
  SECURITY_CHECK(client_);

  DCHECK(!initialized_);
  initialized_ = true;

  if (result == midi::mojom::blink::Result::OK) {
    // Add the accessor's input and output ports.
    for (const auto& info : inputs_) {
      client_->DidAddInputPort(info.id, info.manufacturer, info.name,
                               info.version, info.state);
    }

    for (const auto& info : outputs_) {
      client_->DidAddOutputPort(info.id, info.manufacturer, info.name,
                                info.version, info.state);
    }
  }
  client_->DidStartSession(result);
}

void MIDIDispatcher::AcknowledgeSentData(uint32_t bytes_sent) {
  DCHECK_GE(unacknowledged_bytes_sent_, bytes_sent);
  if (unacknowledged_bytes_sent_ >= bytes_sent)
    unacknowledged_bytes_sent_ -= bytes_sent;
}

void MIDIDispatcher::DataReceived(uint32_t port,
                                  const Vector<uint8_t>& data,
                                  base::TimeTicks timestamp) {
  DCHECK(client_);
  TRACE_EVENT0("midi", "MIDIDispatcher::DataReceived");
  DCHECK(!data.empty());

  if (initialized_)
    client_->DidReceiveMIDIData(port, &data[0], data.size(), timestamp);
}

void MIDIDispatcher::Trace(Visitor* visitor) const {
  visitor->Trace(midi_session_);
  visitor->Trace(receiver_);
  visitor->Trace(midi_session_provider_);
}

}  // namespace blink
