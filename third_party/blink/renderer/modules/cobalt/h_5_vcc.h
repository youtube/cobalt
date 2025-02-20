// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H_5_VCC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H_5_VCC_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ScriptState;
class CrashLog;
class LocalDOMWindow;
class H5vccSystem;
class H5vccRuntime;

class MODULES_EXPORT H5vcc final : public ScriptWrappable,
                                   public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // For window.h5vcc
  static H5vcc* h5vcc(LocalDOMWindow&);

  explicit H5vcc(LocalDOMWindow&);

  CrashLog* crashLog() { return crash_log_; }

  H5vccSystem* system() { return system_; }

  H5vccRuntime* runtime() { return runtime_; }

  void Trace(Visitor*) const override;

 private:
  Member<CrashLog> crash_log_;
  Member<H5vccSystem> system_;
  Member<H5vccRuntime> runtime_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H_5_VCC_H_
