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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_COBALT_LIFECYCLE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_COBALT_LIFECYCLE_CONTROLLER_H_

#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom-blink.h"
#include "cobalt/browser/lifecycle/public/mojom/cobalt_lifecycle.mojom-blink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class CobaltLifecycleController
    : public GarbageCollected<CobaltLifecycleController>,
      public cobalt::mojom::blink::CobaltLifecycleController,
      public ExecutionContextLifecycleStateObserver,
      public PageVisibilityObserver,
      public FocusChangedObserver,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static CobaltLifecycleController* From(LocalDOMWindow& window);

  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingReceiver<cobalt::mojom::blink::CobaltLifecycleController>
          receiver);

  void BindMojoReceiver(
      mojo::PendingReceiver<cobalt::mojom::blink::CobaltLifecycleController>
          receiver);

  explicit CobaltLifecycleController(
      LocalDOMWindow& window,
      mojo::PendingReceiver<cobalt::mojom::blink::CobaltLifecycleController>
          receiver);
  ~CobaltLifecycleController() override;

  // ExecutionContextLifecycleObserver impl.
  void ContextDestroyed() override;

  // PageVisibilityObserver impl.
  void PageVisibilityChanged() override;

  // FocusChangedObserver impl.
  void FocusedFrameChanged() override;

  // ExecutionContextLifecycleStateObserver impl.
  void ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState state) override;

  // CobaltLifecycleController impl.
  void SetObserver(
      mojo::PendingRemote<cobalt::mojom::blink::CobaltLifecycleObserver>
          observer) override;

  void Trace(Visitor* visitor) const override;

 private:
  void EnsureRemoteIsBound();

  mojo::Receiver<cobalt::mojom::blink::CobaltLifecycleController> receiver_;
  HeapMojoRemote<cobalt::mojom::blink::CobaltLifecycleObserver>
      remote_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_COBALT_LIFECYCLE_CONTROLLER_H_
