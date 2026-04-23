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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_CIRCULAR_BUFFER_H_5_VCC_CIRCULAR_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_CIRCULAR_BUFFER_H_5_VCC_CIRCULAR_BUFFER_H_

#include "cobalt/browser/h5vcc_circular_buffer/public/mojom/h5vcc_circular_buffer.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class ScriptState;

class H5vccCircularBuffer;
using H5VccCircularBuffer = H5vccCircularBuffer;

class H5vccCircularBuffer final : public ScriptWrappable,
                                  public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static H5vccCircularBuffer* Create(ExecutionContext* context,
                                     const String& identifier);

  H5vccCircularBuffer(LocalDOMWindow& window, const String& identifier);

  void append(const String& data, ExceptionState& exception_state);
  String peek(ExceptionState& exception_state);
  void pop(ExceptionState& exception_state);

  void ContextDestroyed() override;
  void Trace(Visitor* visitor) const override;

 private:
  void EnsureReceiverIsBound();

  HeapMojoRemote<h5vcc_circular_buffer::mojom::blink::H5vccCircularBuffer>
      remote_h5vcc_circular_buffer_;
  String identifier_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_CIRCULAR_BUFFER_H_5_VCC_CIRCULAR_BUFFER_H_
