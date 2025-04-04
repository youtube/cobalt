// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_H_
#define V8_HEAP_MARK_COMPACT_H_

#include <atomic>
#include <vector>

#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/heap/base/worklist.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/index-generator.h"
#include "src/heap/marking-state.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/marking.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/parallel-work-item.h"
#include "src/heap/spaces.h"
#include "src/heap/sweeper.h"

namespace v8 {
namespace internal {

// Forward declarations.
class EvacuationJobTraits;
class HeapObjectVisitor;
class ItemParallelJob;
class LargeObjectSpace;
class LargePage;
class MigrationObserver;
class PagedNewSpace;
class ReadOnlySpace;
class RecordMigratedSlotVisitor;
class UpdatingItem;
class YoungGenerationMarkingTask;

class LiveObjectVisitor : AllStatic {
 public:
  // Visits black objects on a MemoryChunk until the Visitor returns |false| for
  // an object.
  template <class Visitor, typename MarkingState>
  static bool VisitBlackObjects(MemoryChunk* chunk, MarkingState* state,
                                Visitor* visitor, HeapObject* failed_object);

  // Visits black objects on a MemoryChunk. The visitor is not allowed to fail
  // visitation for an object.
  template <class Visitor, typename MarkingState>
  static void VisitBlackObjectsNoFail(MemoryChunk* chunk, MarkingState* state,
                                      Visitor* visitor);

  template <typename MarkingState>
  static void RecomputeLiveBytes(MemoryChunk* chunk, MarkingState* state);
};

enum class PromoteUnusablePages { kYes, kNo };
enum class MemoryReductionMode { kNone, kShouldReduceMemory };
enum PageEvacuationMode { NEW_TO_NEW, NEW_TO_OLD };
enum class RememberedSetUpdatingMode { ALL, OLD_TO_NEW_ONLY };

// This visitor is used for marking on the main thread. It is cheaper than
// the concurrent marking visitor because it does not snapshot JSObjects.
template <typename MarkingState>
class MainMarkingVisitor final
    : public MarkingVisitorBase<MainMarkingVisitor<MarkingState>,
                                MarkingState> {
 public:
  MainMarkingVisitor(MarkingState* marking_state,
                     MarkingWorklists::Local* local_marking_worklists,
                     WeakObjects::Local* local_weak_objects, Heap* heap,
                     unsigned mark_compact_epoch,
                     base::EnumSet<CodeFlushMode> code_flush_mode,
                     bool trace_embedder_fields,
                     bool should_keep_ages_unchanged)
      : MarkingVisitorBase<MainMarkingVisitor<MarkingState>, MarkingState>(
            local_marking_worklists, local_weak_objects, heap,
            mark_compact_epoch, code_flush_mode, trace_embedder_fields,
            should_keep_ages_unchanged),
        marking_state_(marking_state) {}

 private:
  // Functions required by MarkingVisitorBase.

  template <typename TSlot>
  void RecordSlot(HeapObject object, TSlot slot, HeapObject target);

  void RecordRelocSlot(InstructionStream host, RelocInfo* rinfo,
                       HeapObject target);

  MarkingState* marking_state() { return marking_state_; }

  TraceRetainingPathMode retaining_path_mode() {
    return (V8_UNLIKELY(v8_flags.track_retaining_path))
               ? TraceRetainingPathMode::kEnabled
               : TraceRetainingPathMode::kDisabled;
  }

  MarkingState* const marking_state_;

  friend class MarkingVisitorBase<MainMarkingVisitor<MarkingState>,
                                  MarkingState>;
};

// Marking state that keeps live bytes locally in a fixed-size hashmap. Hashmap
// entries are evicted to the global counters on collision.
class YoungGenerationMarkingState final
    : public MarkingStateBase<YoungGenerationMarkingState, AccessMode::ATOMIC> {
 public:
  explicit YoungGenerationMarkingState(PtrComprCageBase cage_base)
      : MarkingStateBase(cage_base) {}
  V8_INLINE ~YoungGenerationMarkingState();

  const MarkingBitmap* bitmap(const MemoryChunk* chunk) const {
    return chunk->marking_bitmap();
  }

  V8_INLINE void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by);

 private:
  static constexpr size_t kNumEntries = 128;
  static constexpr size_t kEntriesMask = kNumEntries - 1;
  std::array<std::pair<MemoryChunk*, size_t>, kNumEntries> live_bytes_data_;
};

class YoungGenerationMainMarkingVisitor final
    : public YoungGenerationMarkingVisitorBase<
          YoungGenerationMainMarkingVisitor, MarkingState> {
 public:
  YoungGenerationMainMarkingVisitor(Isolate* isolate,
                                    MarkingWorklists::Local* worklists_local);

  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end);

  YoungGenerationMarkingState* marking_state() { return &marking_state_; }

  enum class ObjectVisitationMode {
    kVisitDirectly,
    kPushToWorklist,
  };

  enum class SlotTreatmentMode {
    kReadOnly,
    kReadWrite,
  };

  // Returns whether a young generation object was found in slot.
  template <ObjectVisitationMode visitation_mode,
            SlotTreatmentMode slot_treatment_mode, typename TSlot>
  V8_INLINE bool VisitObjectViaSlot(TSlot slot);

 private:
  V8_INLINE bool ShortCutStrings(HeapObjectSlot slot, HeapObject* heap_object);

  YoungGenerationMarkingState marking_state_;
  const bool shortcut_strings_;

  friend class YoungGenerationMarkingVisitorBase<
      YoungGenerationMainMarkingVisitor, MarkingState>;
};

class CollectorBase {
 public:
  GarbageCollector garbage_collector() { return garbage_collector_; }

  virtual void SetUp() {}
  virtual void TearDown() {}
  virtual void CollectGarbage() = 0;
  virtual void Prepare() = 0;
  virtual void StartMarking() = 0;

  MarkingWorklists* marking_worklists() { return &marking_worklists_; }

  MarkingWorklists::Local* local_marking_worklists() const {
    return local_marking_worklists_.get();
  }

  // Drains the main thread marking worklist until the specified number of
  // bytes are processed. If the number of bytes is zero, then the worklist
  // is drained until it is empty.
  virtual std::pair<size_t, size_t> ProcessMarkingWorklist(
      size_t bytes_to_process) = 0;

  virtual void Finish() = 0;

  bool IsMajorMC();

 private:
  std::vector<Page*> new_space_evacuation_pages_;

 protected:
  using ResizeNewSpaceMode = Heap::ResizeNewSpaceMode;

  inline Heap* heap() const { return heap_; }
  inline Isolate* isolate();

  MarkingState* marking_state() { return marking_state_; }

  NonAtomicMarkingState* non_atomic_marking_state() {
    return non_atomic_marking_state_;
  }

  void StartSweepSpace(PagedSpace* space);

  bool IsCppHeapMarkingFinished() const;

  Heap* heap_;
  GarbageCollector garbage_collector_;
  MarkingWorklists marking_worklists_;

  std::unique_ptr<MarkingWorklists::Local> local_marking_worklists_;

  MarkingState* const marking_state_;
  NonAtomicMarkingState* const non_atomic_marking_state_;

  ResizeNewSpaceMode resize_new_space_ = ResizeNewSpaceMode::kNone;

  explicit CollectorBase(Heap* heap, GarbageCollector collector);
  virtual ~CollectorBase() = default;
};

// Collector for young and old generation.
class MarkCompactCollector final : public CollectorBase {
 public:
  using MarkingVisitor = MainMarkingVisitor<MarkingState>;

  class CustomRootBodyMarkingVisitor;
  class ClientCustomRootBodyMarkingVisitor;
  class SharedHeapObjectVisitor;
  class RootMarkingVisitor;

  enum class StartCompactionMode {
    kIncremental,
    kAtomic,
  };

  enum class MarkingWorklistProcessingMode {
    kDefault,
    kTrackNewlyDiscoveredObjects
  };

  static MarkCompactCollector* From(CollectorBase* collector) {
    return static_cast<MarkCompactCollector*>(collector);
  }

  // Callback function for telling whether the object *p is an unmarked
  // heap object.
  static bool IsUnmarkedHeapObject(Heap* heap, FullObjectSlot p);
  static bool IsUnmarkedSharedHeapObject(Heap* heap, FullObjectSlot p);

  std::pair<size_t, size_t> ProcessMarkingWorklist(
      size_t bytes_to_process) final;

  std::pair<size_t, size_t> ProcessMarkingWorklist(
      size_t bytes_to_process, MarkingWorklistProcessingMode mode);

  void SetUp() final;
  void TearDown() final;

  // Performs a global garbage collection.
  void CollectGarbage() final;

  void CollectEvacuationCandidates(PagedSpace* space);

  void AddEvacuationCandidate(Page* p);

  // Prepares for GC by resetting relocation info in old and map spaces and
  // choosing spaces to compact.
  void Prepare() final;

  // Stop concurrent marking (either by preempting it right away or waiting for
  // it to complete as requested by |stop_request|).
  void FinishConcurrentMarking();

  // Returns whether compaction is running.
  bool StartCompaction(StartCompactionMode mode);

  void AbortCompaction();

  void StartMarking() final;

  static inline bool IsOnEvacuationCandidate(Object obj) {
    return Page::FromAddress(obj.ptr())->IsEvacuationCandidate();
  }

  static bool IsOnEvacuationCandidate(MaybeObject obj);

  struct RecordRelocSlotInfo {
    MemoryChunk* memory_chunk;
    SlotType slot_type;
    uint32_t offset;
  };

  static bool ShouldRecordRelocSlot(InstructionStream host, RelocInfo* rinfo,
                                    HeapObject target);
  static RecordRelocSlotInfo ProcessRelocInfo(InstructionStream host,
                                              RelocInfo* rinfo,
                                              HeapObject target);

  static void RecordRelocSlot(InstructionStream host, RelocInfo* rinfo,
                              HeapObject target);
  V8_INLINE static void RecordSlot(HeapObject object, ObjectSlot slot,
                                   HeapObject target);
  V8_INLINE static void RecordSlot(HeapObject object, HeapObjectSlot slot,
                                   HeapObject target);
  V8_INLINE static void RecordSlot(MemoryChunk* source_page,
                                   HeapObjectSlot slot, HeapObject target);

  bool is_compacting() const { return compacting_; }

  inline void AddTransitionArray(TransitionArray array);

  void RecordStrongDescriptorArraysForWeakening(
      GlobalHandleVector<DescriptorArray> strong_descriptor_arrays);

#ifdef DEBUG
  // Checks whether performing mark-compact collection.
  bool in_use() { return state_ > PREPARE_GC; }
  bool are_map_pointers_encoded() { return state_ == UPDATE_POINTERS; }
#endif

  void VerifyMarking();
#ifdef VERIFY_HEAP
  void VerifyMarkbitsAreClean();
  void VerifyMarkbitsAreClean(PagedSpaceBase* space);
  void VerifyMarkbitsAreClean(NewSpace* space);
  void VerifyMarkbitsAreClean(LargeObjectSpace* space);
#endif

  unsigned epoch() const { return epoch_; }

  base::EnumSet<CodeFlushMode> code_flush_mode() const {
    return code_flush_mode_;
  }

  WeakObjects* weak_objects() { return &weak_objects_; }
  WeakObjects::Local* local_weak_objects() { return local_weak_objects_.get(); }

  void AddNewlyDiscovered(HeapObject object) {
    if (ephemeron_marking_.newly_discovered_overflowed) return;

    if (ephemeron_marking_.newly_discovered.size() <
        ephemeron_marking_.newly_discovered_limit) {
      ephemeron_marking_.newly_discovered.push_back(object);
    } else {
      ephemeron_marking_.newly_discovered_overflowed = true;
    }
  }

  void ResetNewlyDiscovered() {
    ephemeron_marking_.newly_discovered_overflowed = false;
    ephemeron_marking_.newly_discovered.clear();
  }

  explicit MarkCompactCollector(Heap* heap);
  ~MarkCompactCollector() final;

 private:
  class ClientRootMarkingVisitor;

  Sweeper* sweeper() { return sweeper_; }

  void ComputeEvacuationHeuristics(size_t area_size,
                                   int* target_fragmentation_percent,
                                   size_t* max_evacuated_bytes);

  void RecordObjectStats();

  // Finishes GC, performs heap verification if enabled.
  void Finish() final;

  // Free unmarked ArrayBufferExtensions.
  void SweepArrayBufferExtensions();

  void MarkLiveObjects();

  // Marks the object grey and adds it to the marking work list.
  // This is for non-incremental marking only.
  V8_INLINE void MarkObject(HeapObject host, HeapObject obj);

  // Marks the object grey and adds it to the marking work list.
  // This is for non-incremental marking only.
  V8_INLINE void MarkRootObject(Root root, HeapObject obj);

  // Mark the heap roots and all objects reachable from them.
  void MarkRoots(RootVisitor* root_visitor);

  // Mark the stack roots and all objects reachable from them.
  void MarkRootsFromConservativeStack(RootVisitor* root_visitor);

  // Mark all objects that are directly referenced from one of the clients
  // heaps.
  void MarkObjectsFromClientHeaps();
  void MarkObjectsFromClientHeap(Isolate* client);

  // Mark the entry in the external pointer table for the given isolates
  // WaiterQueueNode.
  void MarkWaiterQueueNode(Isolate* isolate);

  // Updates pointers to shared objects from client heaps.
  void UpdatePointersInClientHeaps();
  void UpdatePointersInClientHeap(Isolate* client);

  // Marks object reachable from harmony weak maps and wrapper tracing.
  void MarkTransitiveClosure();
  void VerifyEphemeronMarking();

  // If the call-site of the top optimized code was not prepared for
  // deoptimization, then treat embedded pointers in the code as strong as
  // otherwise they can die and try to deoptimize the underlying code.
  void ProcessTopOptimizedFrame(ObjectVisitor* visitor, Isolate* isolate);

  // Implements ephemeron semantics: Marks value if key is already reachable.
  // Returns true if value was actually marked.
  bool ProcessEphemeron(HeapObject key, HeapObject value);

  // Marks the transitive closure by draining the marking worklist iteratively,
  // applying ephemerons semantics and invoking embedder tracing until a
  // fixpoint is reached. Returns false if too many iterations have been tried
  // and the linear approach should be used.
  bool MarkTransitiveClosureUntilFixpoint();

  // Marks the transitive closure applying ephemeron semantics and invoking
  // embedder tracing with a linear algorithm for ephemerons. Only used if
  // fixpoint iteration doesn't finish within a few iterations.
  void MarkTransitiveClosureLinear();

  // Drains ephemeron and marking worklists. Single iteration of the
  // fixpoint iteration.
  bool ProcessEphemerons();

  // Perform Wrapper Tracing if in use.
  void PerformWrapperTracing();

  // Retain dying maps for `v8_flags.retain_maps_for_n_gc` garbage collections
  // to increase chances of reusing of map transition tree in future.
  void RetainMaps();

  // Clear non-live references in weak cells, transition and descriptor arrays,
  // and deoptimize dependent code of non-live maps.
  void ClearNonLiveReferences();
  void MarkDependentCodeForDeoptimization();
  // Checks if the given weak cell is a simple transition from the parent map
  // of the given dead target. If so it clears the transition and trims
  // the descriptor array of the parent if needed.
  void ClearPotentialSimpleMapTransition(Map dead_target);
  void ClearPotentialSimpleMapTransition(Map map, Map dead_target);

  // Flushes a weakly held bytecode array from a shared function info.
  void FlushBytecodeFromSFI(SharedFunctionInfo shared_info);

  // Clears bytecode arrays / baseline code that have not been executed for
  // multiple collections.
  void ProcessOldCodeCandidates();
  void ProcessFlushedBaselineCandidates();

  // Resets any JSFunctions which have had their bytecode flushed.
  void ClearFlushedJsFunctions();

  // Compact every array in the global list of transition arrays and
  // trim the corresponding descriptor array if a transition target is non-live.
  void ClearFullMapTransitions();
  void TrimDescriptorArray(Map map, DescriptorArray descriptors);
  void TrimEnumCache(Map map, DescriptorArray descriptors);
  bool CompactTransitionArray(Map map, TransitionArray transitions,
                              DescriptorArray descriptors);
  bool TransitionArrayNeedsCompaction(TransitionArray transitions,
                                      int num_transitions);
  void WeakenStrongDescriptorArrays();

  // After all reachable objects have been marked those weak map entries
  // with an unreachable key are removed from all encountered weak maps.
  // The linked list of all encountered weak maps is destroyed.
  void ClearWeakCollections();

  // Goes through the list of encountered weak references and clears those with
  // dead values. If the value is a dead map and the parent map transitions to
  // the dead map via weak cell, then this function also clears the map
  // transition.
  void ClearWeakReferences();

  // Goes through the list of encountered JSWeakRefs and WeakCells and clears
  // those with dead values.
  void ClearJSWeakRefs();

  // Starts sweeping of spaces by contributing on the main thread and setting
  // up other pages for sweeping. Does not start sweeper tasks.
  void Sweep();

  void EvacuatePrologue();
  void EvacuateEpilogue();
  void Evacuate();
  void EvacuatePagesInParallel();
  void UpdatePointersAfterEvacuation();

  void ReleaseEvacuationCandidates();
  // Returns number of aborted pages.
  size_t PostProcessAbortedEvacuationCandidates();
  void ReportAbortedEvacuationCandidateDueToOOM(Address failed_start,
                                                Page* page);
  void ReportAbortedEvacuationCandidateDueToFlags(Address failed_start,
                                                  Page* page);

  static const int kEphemeronChunkSize = 8 * KB;

  int NumberOfParallelEphemeronVisitingTasks(size_t elements);

  void RightTrimDescriptorArray(DescriptorArray array, int descriptors_to_trim);

  V8_INLINE bool ShouldMarkObject(HeapObject) const;

  void StartSweepNewSpace();
  void SweepLargeSpace(LargeObjectSpace* space);

  base::Mutex mutex_;
  base::Semaphore page_parallel_job_semaphore_{0};

#ifdef DEBUG
  enum CollectorState{IDLE,
                      PREPARE_GC,
                      MARK_LIVE_OBJECTS,
                      SWEEP_SPACES,
                      ENCODE_FORWARDING_ADDRESSES,
                      UPDATE_POINTERS,
                      RELOCATE_OBJECTS};

  // The current stage of the collector.
  CollectorState state_;
#endif

  const bool uses_shared_heap_;
  const bool is_shared_space_isolate_;

  // True if we are collecting slots to perform evacuation from evacuation
  // candidates.
  bool compacting_ = false;
  bool black_allocation_ = false;
  bool have_code_to_deoptimize_ = false;
  bool parallel_marking_ = false;

  WeakObjects weak_objects_;
  EphemeronMarking ephemeron_marking_;

  std::unique_ptr<MarkingVisitor> marking_visitor_;
  std::unique_ptr<WeakObjects::Local> local_weak_objects_;
  NativeContextInferrer native_context_inferrer_;
  NativeContextStats native_context_stats_;

  std::vector<GlobalHandleVector<DescriptorArray>> strong_descriptor_arrays_;
  base::Mutex strong_descriptor_arrays_mutex_;

  // Candidates for pages that should be evacuated.
  std::vector<Page*> evacuation_candidates_;
  // Pages that are actually processed during evacuation.
  std::vector<Page*> old_space_evacuation_pages_;
  std::vector<Page*> new_space_evacuation_pages_;
  std::vector<std::pair<Address, Page*>>
      aborted_evacuation_candidates_due_to_oom_;
  std::vector<std::pair<Address, Page*>>
      aborted_evacuation_candidates_due_to_flags_;
  std::vector<LargePage*> promoted_large_pages_;

  Sweeper* const sweeper_;

  // Counts the number of major mark-compact collections. The counter is
  // incremented right after marking. This is used for:
  // - marking descriptor arrays. See NumberOfMarkedDescriptors. Only the lower
  //   two bits are used, so it is okay if this counter overflows and wraps
  //   around.
  unsigned epoch_ = 0;

  // Bytecode flushing is disabled when the code coverage mode is changed. Since
  // that can happen while a GC is happening and we need the
  // code_flush_mode_ to remain the same through out a GC, we record this at
  // the start of each GC.
  base::EnumSet<CodeFlushMode> code_flush_mode_;

  std::vector<Page*> empty_new_space_pages_to_be_swept_;

  friend class Evacuator;
  friend class RecordMigratedSlotVisitor;
};

// Collector for young-generation only.
class MinorMarkCompactCollector final : public CollectorBase {
 public:
  static constexpr size_t kMaxParallelTasks = 8;

  static MinorMarkCompactCollector* From(CollectorBase* collector) {
    return static_cast<MinorMarkCompactCollector*>(collector);
  }

  explicit MinorMarkCompactCollector(Heap* heap);
  ~MinorMarkCompactCollector() final;

  std::pair<size_t, size_t> ProcessMarkingWorklist(
      size_t bytes_to_process) final;

  void SetUp() final;
  void TearDown() final;
  void CollectGarbage() final;
  void Prepare() final {}
  void StartMarking() final;

  void MakeIterable(Page* page, FreeSpaceTreatmentMode free_space_mode);

  void Finish() final;

  // Perform Wrapper Tracing if in use.
  void PerformWrapperTracing();

 private:
  class RootMarkingVisitor;

  static const int kNumMarkers = 8;
  static const int kMainMarker = 0;

  Sweeper* sweeper() { return sweeper_; }

  void MarkLiveObjects();
  void MarkLiveObjectsInParallel(RootMarkingVisitor* root_visitor,
                                 bool was_marked_incrementally);
  void DrainMarkingWorklist();
  void TraceFragmentation();
  void ClearNonLiveReferences();

  void Sweep();

  void FinishConcurrentMarking();

  // 'StartSweepNewSpace' and 'SweepNewLargeSpace' return true if any pages were
  // promoted.
  bool StartSweepNewSpace();
  bool SweepNewLargeSpace();

  std::unique_ptr<YoungGenerationMainMarkingVisitor> main_marking_visitor_;

  Sweeper* const sweeper_;

  friend class YoungGenerationMarkingTask;
  friend class YoungGenerationMarkingJob;
  friend class YoungGenerationMainMarkingVisitor;
};

class PageMarkingItem : public ParallelWorkItem {
 public:
  enum class SlotsType { kRegularSlots, kTypedSlots };

  PageMarkingItem(MemoryChunk* chunk, SlotsType slots_type)
      : chunk_(chunk), slots_type_(slots_type) {}
  ~PageMarkingItem() = default;

  void Process(YoungGenerationMarkingTask* task);

 private:
  inline Heap* heap() { return chunk_->heap(); }

  void MarkUntypedPointers(YoungGenerationMarkingTask* task);
  void MarkTypedPointers(YoungGenerationMarkingTask* task);
  template <typename TSlot>
  V8_INLINE SlotCallbackResult
  CheckAndMarkObject(YoungGenerationMarkingTask* task, TSlot slot);

  MemoryChunk* chunk_;
  const SlotsType slots_type_;
};

enum class YoungMarkingJobType { kAtomic, kIncremental };

class YoungGenerationMarkingJob : public v8::JobTask {
 public:
  YoungGenerationMarkingJob(
      Isolate* isolate, Heap* heap, MarkingWorklists* global_worklists,
      std::vector<PageMarkingItem> marking_items,
      YoungMarkingJobType young_marking_job_type,
      const std::vector<std::unique_ptr<YoungGenerationMarkingTask>>& tasks)
      : isolate_(isolate),
        heap_(heap),
        global_worklists_(global_worklists),
        marking_items_(std::move(marking_items)),
        remaining_marking_items_(marking_items_.size()),
        generator_(marking_items_.size()),
        young_marking_job_type_(young_marking_job_type),
        tasks_(tasks) {}

  void Run(JobDelegate* delegate) override;
  size_t GetMaxConcurrency(size_t worker_count) const override;

  bool ShouldDrainMarkingWorklist() const {
    return young_marking_job_type_ == YoungMarkingJobType::kAtomic;
  }

 private:
  void ProcessItems(JobDelegate* delegate);
  void ProcessMarkingItems(YoungGenerationMarkingTask* task);

  Isolate* isolate_;
  Heap* heap_;
  MarkingWorklists* global_worklists_;
  std::vector<PageMarkingItem> marking_items_;
  std::atomic_size_t remaining_marking_items_{0};
  IndexGenerator generator_;
  YoungMarkingJobType young_marking_job_type_;
  const std::vector<std::unique_ptr<YoungGenerationMarkingTask>>& tasks_;
};

class YoungGenerationMarkingTask final {
 public:
  YoungGenerationMarkingTask(Isolate* isolate, Heap* heap,
                             MarkingWorklists* global_worklists);

  YoungGenerationMarkingTask(const YoungGenerationMarkingTask&) = delete;
  YoungGenerationMarkingTask& operator=(const YoungGenerationMarkingTask&) =
      delete;

  void DrainMarkingWorklist();
  void PublishMarkingWorklist();

  void Finalize();

  YoungGenerationMainMarkingVisitor* visitor() { return &visitor_; }

 private:
  MarkingWorklists::Local marking_worklists_local_;
  YoungGenerationMainMarkingVisitor visitor_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_H_
