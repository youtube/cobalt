// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_H_5_VCC_SYSTEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_H_5_VCC_SYSTEM_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT H5vccSystem final
    : public ScriptWrappable,
      public Supplement<LocalDOMWindow>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // For window.h5vccSystem
  static H5vccSystem* h5vccSystem(LocalDOMWindow&);

  explicit H5vccSystem(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:
  const String advertisingId() const;

  void Trace(Visitor*) const override;

 private:
  String advertising_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_H_5_VCC_SYSTEM_H_
