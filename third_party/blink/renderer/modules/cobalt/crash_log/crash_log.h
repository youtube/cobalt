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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_CRASH_LOG_CRASH_LOG_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_CRASH_LOG_CRASH_LOG_H_

#include "cobalt/browser/crash_annotator/public/mojom/crash_annotator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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
class ScriptState;

class MODULES_EXPORT CrashLog final : public ScriptWrappable,
                                      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CrashLog(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:
  ScriptPromise<IDLBoolean> setString(ScriptState*,
                                      const String& key,
                                      const String& value,
                                      ExceptionState&);
  void triggerCrash();

  bool testValue();

  void Trace(Visitor*) const override;

 private:
  void OnSetString(ScriptPromiseResolver<IDLBoolean>*, bool);
  void EnsureReceiverIsBound();

  bool test_value_;

  // TODO(cobalt, b/383301493): consider renaming the web interface and
  // associated Blink types from "crash log" to the preferred "crash annotator"
  // name that is used elsewhere, for example in the corresponding Mojo
  // interface. We chose to keep the legacy "crash log" name for the initial
  // version of Chrobalt to ease the migration process for web clients.
  HeapMojoRemote<crash_annotator::mojom::blink::CrashAnnotator>
      remote_crash_annotator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_CRASH_LOG_CRASH_LOG_H_
