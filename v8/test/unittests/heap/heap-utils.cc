// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/heap-utils.h"

#include <algorithm>

#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/new-spaces.h"
#include "src/heap/safepoint.h"
#include "src/objects/free-space-inl.h"

namespace v8 {
namespace internal {

void HeapInternalsBase::SimulateIncrementalMarking(Heap* heap,
                                                   bool force_completion) {
  constexpr double kStepSizeInMs = 100;
  CHECK(v8_flags.incremental_marking);
  i::IncrementalMarking* marking = heap->incremental_marking();

  if (heap->sweeping_in_progress()) {
    IsolateSafepointScope scope(heap);
    heap->EnsureSweepingCompleted(
        Heap::SweepingForcedFinalizationMode::kV8Only);
  }

  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMajorMarking());
  if (!force_completion) return;

  while (!marking->IsMajorMarkingComplete()) {
    marking->AdvanceForTesting(kStepSizeInMs);
  }
}

namespace {

int FixedArrayLenFromSize(int size) {
  return std::min({(size - FixedArray::kHeaderSize) / kTaggedSize,
                   FixedArray::kMaxRegularLength});
}

void FillPageInPagedSpace(Page* page,
                          std::vector<Handle<FixedArray>>* out_handles) {
  Heap* heap = page->heap();
  DCHECK(page->SweepingDone());
  PagedSpaceBase* paged_space = static_cast<PagedSpaceBase*>(page->owner());
  // Make sure the LAB is empty to guarantee that all free space is accounted
  // for in the freelist.
  DCHECK_EQ(paged_space->limit(), paged_space->top());

  PauseAllocationObserversScope no_observers_scope(heap);

  CollectionEpoch full_epoch =
      heap->tracer()->CurrentEpoch(GCTracer::Scope::ScopeId::MARK_COMPACTOR);
  CollectionEpoch young_epoch = heap->tracer()->CurrentEpoch(
      GCTracer::Scope::ScopeId::MINOR_MARK_COMPACTOR);

  for (Page* p : *paged_space) {
    if (p != page) paged_space->UnlinkFreeListCategories(p);
  }

  // If min_block_size is larger than FixedArray::kHeaderSize, all blocks in the
  // free list can be used to allocate a fixed array. This guarantees that we
  // can fill the whole page.
  DCHECK_LT(FixedArray::kHeaderSize,
            paged_space->free_list()->min_block_size());

  std::vector<int> available_sizes;
  // Collect all free list block sizes
  page->ForAllFreeListCategories(
      [&available_sizes](FreeListCategory* category) {
        category->IterateNodesForTesting([&available_sizes](FreeSpace node) {
          int node_size = node.Size();
          if (node_size >= kMaxRegularHeapObjectSize) {
            available_sizes.push_back(node_size);
          }
        });
      });

  Isolate* isolate = heap->isolate();

  // Allocate as many max size arrays as possible, while making sure not to
  // leave behind a block too small to fit a FixedArray.
  const int max_array_length = FixedArrayLenFromSize(kMaxRegularHeapObjectSize);
  for (size_t i = 0; i < available_sizes.size(); ++i) {
    int available_size = available_sizes[i];
    while (available_size > kMaxRegularHeapObjectSize) {
      Handle<FixedArray> fixed_array = isolate->factory()->NewFixedArray(
          max_array_length, AllocationType::kYoung);
      if (out_handles) out_handles->push_back(fixed_array);
      available_size -= kMaxRegularHeapObjectSize;
    }
  }

  paged_space->FreeLinearAllocationArea();

  // Allocate FixedArrays in remaining free list blocks, from largest
  // category to smallest.
  std::vector<std::vector<int>> remaining_sizes;
  page->ForAllFreeListCategories(
      [&remaining_sizes](FreeListCategory* category) {
        remaining_sizes.push_back({});
        std::vector<int>& sizes_in_category =
            remaining_sizes[remaining_sizes.size() - 1];
        category->IterateNodesForTesting([&sizes_in_category](FreeSpace node) {
          int node_size = node.Size();
          DCHECK_LT(0, FixedArrayLenFromSize(node_size));
          sizes_in_category.push_back(node_size);
        });
      });
  for (auto it = remaining_sizes.rbegin(); it != remaining_sizes.rend(); ++it) {
    std::vector<int> sizes_in_category = *it;
    for (int size : sizes_in_category) {
      DCHECK_LE(size, kMaxRegularHeapObjectSize);
      int array_length = FixedArrayLenFromSize(size);
      DCHECK_LT(0, array_length);
      Handle<FixedArray> fixed_array = isolate->factory()->NewFixedArray(
          array_length, AllocationType::kYoung);
      if (out_handles) out_handles->push_back(fixed_array);
    }
  }

  DCHECK_EQ(0, page->AvailableInFreeList());
  DCHECK_EQ(0, page->AvailableInFreeListFromAllocatedBytes());

  for (Page* p : *paged_space) {
    if (p != page) paged_space->RelinkFreeListCategories(p);
  }

  // Allocations in this method should not require a GC.
  CHECK_EQ(full_epoch, heap->tracer()->CurrentEpoch(
                           GCTracer::Scope::ScopeId::MARK_COMPACTOR));
  CHECK_EQ(young_epoch, heap->tracer()->CurrentEpoch(
                            GCTracer::Scope::ScopeId::MINOR_MARK_COMPACTOR));
}

}  // namespace

void HeapInternalsBase::SimulateFullSpace(
    v8::internal::NewSpace* space,
    std::vector<Handle<FixedArray>>* out_handles) {
  // If you see this check failing, disable the flag at the start of your test:
  // v8_flags.stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!v8_flags.stress_concurrent_allocation);
  space->heap()->EnsureSweepingCompleted(
      Heap::SweepingForcedFinalizationMode::kV8Only);
  space->FreeLinearAllocationArea();
  if (v8_flags.minor_mc) {
    while (space->AddFreshPage()) {}
    for (Page* page : *space) {
      FillPageInPagedSpace(page, out_handles);
    }
    DCHECK_EQ(0, space->free_list()->Available());
  } else {
    do {
      FillCurrentPage(space, out_handles);
    } while (space->AddFreshPage());
  }
}

void HeapInternalsBase::SimulateFullSpace(v8::internal::PagedSpace* space) {
  // If you see this check failing, disable the flag at the start of your test:
  // v8_flags.stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!v8_flags.stress_concurrent_allocation);
  Heap* heap = space->heap();
  CodePageCollectionMemoryModificationScopeForTesting code_scope(heap);
  if (heap->sweeping_in_progress()) {
    heap->EnsureSweepingCompleted(
        Heap::SweepingForcedFinalizationMode::kV8Only);
  }
  space->FreeLinearAllocationArea();
  space->ResetFreeList();
}

namespace {
int GetSpaceRemainingOnCurrentSemiSpacePage(v8::internal::NewSpace* space) {
  Address top = space->top();
  if ((top & kPageAlignmentMask) == 0) {
    // `top` points to the start of a page signifies that there is not room in
    // the current page.
    return 0;
  }
  return static_cast<int>(Page::FromAddress(space->top())->area_end() - top);
}

std::vector<Handle<FixedArray>> CreatePadding(Heap* heap, int padding_size,
                                              AllocationType allocation) {
  std::vector<Handle<FixedArray>> handles;
  Isolate* isolate = heap->isolate();
  int allocate_memory;
  int length;
  int free_memory = padding_size;
  if (allocation == i::AllocationType::kOld) {
    heap->old_space()->FreeLinearAllocationArea();
    int overall_free_memory = static_cast<int>(heap->old_space()->Available());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  } else {
    int overall_free_memory = static_cast<int>(heap->new_space()->Available());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  }
  while (free_memory > 0) {
    if (free_memory > kMaxRegularHeapObjectSize) {
      allocate_memory = kMaxRegularHeapObjectSize;
      length = FixedArrayLenFromSize(allocate_memory);
    } else {
      allocate_memory = free_memory;
      length = FixedArrayLenFromSize(allocate_memory);
      if (length <= 0) {
        // Not enough room to create another FixedArray, so create a filler.
        if (allocation == i::AllocationType::kOld) {
          heap->CreateFillerObjectAt(
              *heap->old_space()->allocation_top_address(), free_memory);
        } else {
          heap->CreateFillerObjectAt(
              *heap->new_space()->allocation_top_address(), free_memory);
        }
        break;
      }
    }
    handles.push_back(isolate->factory()->NewFixedArray(length, allocation));
    CHECK((allocation == AllocationType::kYoung &&
           heap->new_space()->Contains(*handles.back())) ||
          (allocation == AllocationType::kOld &&
           heap->InOldSpace(*handles.back())) ||
          v8_flags.single_generation);
    free_memory -= handles.back()->Size();
  }
  return handles;
}

void FillCurrentSemiSpacePage(v8::internal::NewSpace* space,
                              std::vector<Handle<FixedArray>>* out_handles) {
  // We cannot rely on `space->limit()` to point to the end of the current page
  // in the case where inline allocations are disabled, it actually points to
  // the current allocation pointer.
  DCHECK_IMPLIES(!space->heap()->IsInlineAllocationEnabled(),
                 space->limit() == space->top());
  int space_remaining = GetSpaceRemainingOnCurrentSemiSpacePage(space);
  if (space_remaining == 0) return;
  std::vector<Handle<FixedArray>> handles =
      CreatePadding(space->heap(), space_remaining, i::AllocationType::kYoung);
  if (out_handles != nullptr) {
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
  }
}

void FillCurrenPagedSpacePage(v8::internal::NewSpace* space,
                              std::vector<Handle<FixedArray>>* out_handles) {
  if (space->top() == kNullAddress) return;
  Page* page = Page::FromAllocationAreaAddress(space->top());
  space->heap()->EnsureSweepingCompleted(
      Heap::SweepingForcedFinalizationMode::kV8Only);
  space->FreeLinearAllocationArea();
  FillPageInPagedSpace(page, out_handles);
}

}  // namespace

void HeapInternalsBase::FillCurrentPage(
    v8::internal::NewSpace* space,
    std::vector<Handle<FixedArray>>* out_handles) {
  PauseAllocationObserversScope pause_observers(space->heap());
  if (v8_flags.minor_mc)
    FillCurrenPagedSpacePage(space, out_handles);
  else
    FillCurrentSemiSpacePage(space, out_handles);
}

bool IsNewObjectInCorrectGeneration(HeapObject object) {
  return v8_flags.single_generation ? !i::Heap::InYoungGeneration(object)
                                    : i::Heap::InYoungGeneration(object);
}

void FinalizeGCIfRunning(Isolate* isolate) {
  if (!isolate) {
    return;
  }
  auto* heap = isolate->heap();
  if (heap->incremental_marking()->IsMarking()) {
    heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);
    heap->CompleteSweepingFull();
  }
}

}  // namespace internal
}  // namespace v8
