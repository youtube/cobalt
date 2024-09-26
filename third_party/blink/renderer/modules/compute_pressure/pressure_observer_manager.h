// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_

#include "services/device/public/mojom/pressure_manager.mojom-blink.h"
#include "services/device/public/mojom/pressure_update.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_client_impl.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;

// This class keeps track of PressureClientImpls and the connection to the
// PressureManager remote.
class MODULES_EXPORT PressureObserverManager final
    : public GarbageCollected<PressureObserverManager>,
      public ExecutionContextLifecycleStateObserver,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static PressureObserverManager* From(ExecutionContext*);

  explicit PressureObserverManager(ExecutionContext*);
  ~PressureObserverManager() override;

  PressureObserverManager(const PressureObserverManager&) = delete;
  PressureObserverManager& operator=(const PressureObserverManager&) = delete;

  void AddObserver(V8PressureSource::Enum, PressureObserver*);
  void RemoveObserver(V8PressureSource::Enum, PressureObserver*);
  void RemoveObserverFromAllSources(PressureObserver*);

  // ContextLifecycleStateimplementation.
  void ContextDestroyed() override;
  void ContextLifecycleStateChanged(mojom::blink::FrameLifecycleState) override;

  // GarbageCollected implementation.
  void Trace(Visitor*) const override;

 private:
  void EnsureServiceConnection();

  // Called when `pressure_manager_` is disconnected.
  void OnServiceConnectionError();

  // Called to reset `pressure_manager_` when all PressureClientImpl are reset.
  void ResetPressureManagerIfNeeded();

  // Called to reset for all PressureSources.
  void Reset();

  void DidAddClient(V8PressureSource::Enum,
                    device::mojom::blink::PressureStatus);

  // Connection to the services side implementation.
  HeapMojoRemote<device::mojom::blink::PressureManager> pressure_manager_;

  HeapHashMap<V8PressureSource::Enum, Member<PressureClientImpl>>
      source_to_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_
