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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_circular_buffer/h_5_vcc_circular_buffer.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

H5vccCircularBuffer* H5vccCircularBuffer::Create(ExecutionContext* context,
                                                 const String& identifier) {
  return MakeGarbageCollected<H5vccCircularBuffer>(*To<LocalDOMWindow>(context),
                                                   identifier);
}

H5vccCircularBuffer::H5vccCircularBuffer(LocalDOMWindow& window,
                                         const String& identifier)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_circular_buffer_(window.GetExecutionContext()),
      identifier_(identifier) {}

void H5vccCircularBuffer::ContextDestroyed() {
  remote_h5vcc_circular_buffer_.reset();
}

void H5vccCircularBuffer::append(const String& data,
                                 ExceptionState& exception_state) {
  EnsureReceiverIsBound();
  remote_h5vcc_circular_buffer_->Append(data, WTF::BindOnce([]() {
                                          // Append completed callback
                                        }));
}

String H5vccCircularBuffer::peek(ExceptionState& exception_state) {
  EnsureReceiverIsBound();
  String data;
  bool success = false;
  remote_h5vcc_circular_buffer_->Peek(&data, &success);
  if (!success) {
    return String();
  }
  return data;
}

void H5vccCircularBuffer::pop(ExceptionState& exception_state) {
  EnsureReceiverIsBound();
  bool success = false;
  remote_h5vcc_circular_buffer_->Pop(&success);
}

void H5vccCircularBuffer::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_circular_buffer_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_circular_buffer_.BindNewPipeAndPassReceiver(task_runner));

  remote_h5vcc_circular_buffer_->Init(
      identifier_, WTF::BindOnce([](bool success) {
        if (!success) {
          LOG(ERROR) << "Failed to initialize circular buffer on browser side.";
        }
      }));
}

void H5vccCircularBuffer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_circular_buffer_);
}

}  // namespace blink
