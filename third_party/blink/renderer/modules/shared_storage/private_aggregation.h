// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-blink.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-blink.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExceptionState;
class PrivateAggregationDebugModeOptions;
class PrivateAggregationHistogramContribution;
class ScriptState;
class SharedStorageWorkletGlobalScope;

class MODULES_EXPORT PrivateAggregation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  struct OperationState : public GarbageCollected<OperationState> {
    explicit OperationState(ContextLifecycleNotifier* notifier)
        : private_aggregation_host(notifier) {}

    bool enable_debug_mode_called = false;

    // No need to be associated as message ordering (relative to shared storage
    // operations) is unimportant.
    HeapMojoRemote<mojom::blink::PrivateAggregationHost>
        private_aggregation_host;

    void Trace(Visitor* visitor) const {
      visitor->Trace(private_aggregation_host);
    }
  };

  explicit PrivateAggregation(SharedStorageWorkletGlobalScope* global_scope);

  ~PrivateAggregation() override;

  void Trace(Visitor*) const override;

  // PrivateAggregation IDL
  void contributeToHistogram(ScriptState*,
                             const PrivateAggregationHistogramContribution*,
                             ExceptionState&);
  void enableDebugMode(ScriptState*, ExceptionState&);
  void enableDebugMode(ScriptState*,
                       const PrivateAggregationDebugModeOptions*,
                       ExceptionState&);

  void OnOperationStarted(
      int64_t operation_id,
      mojo::PendingRemote<mojom::blink::PrivateAggregationHost>
          private_aggregation_host);
  void OnOperationFinished(int64_t operation_id);

  void OnWorkletDestroyed();

 private:
  void EnsureGeneralUseCountersAreRecorded();
  void EnsureEnableDebugModeUseCounterIsRecorded();

  bool has_recorded_general_use_counters_ = false;
  bool has_recorded_enable_debug_mode_use_counter_ = false;

  Member<SharedStorageWorkletGlobalScope> global_scope_;
  HeapHashMap<int64_t,
              Member<OperationState>,
              IntWithZeroKeyHashTraits<int64_t>>
      operation_states_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_
