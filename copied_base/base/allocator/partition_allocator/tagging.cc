// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/tagging.h"

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "build/build_config.h"

#if PA_CONFIG(HAS_MEMORY_TAGGING)
#include <arm_acle.h>
#include <sys/auxv.h>
#include <sys/prctl.h>
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_GET_TAGGED_ADDR_CTRL 56
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)

#if BUILDFLAG(IS_LINUX)
#include <linux/version.h>

// Linux headers already provide these since v5.10.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#define HAS_PR_MTE_MACROS
#endif
#endif

#ifndef HAS_PR_MTE_MACROS
#define PR_MTE_TCF_SHIFT 1
#define PR_MTE_TCF_NONE (0UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_SYNC (1UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_ASYNC (2UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_MASK (3UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TAG_SHIFT 3
#define PR_MTE_TAG_MASK (0xffffUL << PR_MTE_TAG_SHIFT)
#endif
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/allocator/partition_allocator/partition_alloc_base/files/file_path.h"
#include "base/allocator/partition_allocator/partition_alloc_base/native_library.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace partition_alloc {

#if PA_CONFIG(HAS_MEMORY_TAGGING)
namespace {
void ChangeMemoryTaggingModeInternal(unsigned prctl_mask) {
  if (internal::base::CPU::GetInstanceNoAllocation().has_mte()) {
    int status = prctl(PR_SET_TAGGED_ADDR_CTRL, prctl_mask, 0, 0, 0);
    PA_CHECK(status == 0);
  }
}
}  // namespace
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)

void ChangeMemoryTaggingModeForCurrentThread(TagViolationReportingMode m) {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  if (m == TagViolationReportingMode::kSynchronous) {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC |
                                    (0xfffe << PR_MTE_TAG_SHIFT));
  } else if (m == TagViolationReportingMode::kAsynchronous) {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC |
                                    (0xfffe << PR_MTE_TAG_SHIFT));
  } else {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_NONE);
  }
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)
}

namespace internal {

#if BUILDFLAG(IS_ANDROID)
void ChangeMemoryTaggingModeForAllThreadsPerProcess(
    TagViolationReportingMode m) {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  // In order to support Android NDK API level below 26, we need to call
  // mallopt via dynamic linker.
  // int mallopt(int param, int value);
  using MalloptSignature = int (*)(int, int);

  static MalloptSignature mallopt_fnptr = []() {
    base::FilePath module_path;
    base::NativeLibraryLoadError load_error;
    base::FilePath library_path = module_path.Append("libc.so");
    base::NativeLibrary library =
        base::LoadNativeLibrary(library_path, &load_error);
    PA_CHECK(library);
    void* func_ptr =
        base::GetFunctionPointerFromNativeLibrary(library, "mallopt");
    PA_CHECK(func_ptr);
    return reinterpret_cast<MalloptSignature>(func_ptr);
  }();

  int status = 0;
  if (m == TagViolationReportingMode::kSynchronous) {
    status = mallopt_fnptr(M_BIONIC_SET_HEAP_TAGGING_LEVEL,
                           M_HEAP_TAGGING_LEVEL_SYNC);
  } else if (m == TagViolationReportingMode::kAsynchronous) {
    status = mallopt_fnptr(M_BIONIC_SET_HEAP_TAGGING_LEVEL,
                           M_HEAP_TAGGING_LEVEL_ASYNC);
  } else {
    status = mallopt_fnptr(M_BIONIC_SET_HEAP_TAGGING_LEVEL,
                           M_HEAP_TAGGING_LEVEL_NONE);
  }
  PA_CHECK(status);
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)
}
#endif  // BUILDFLAG(IS_ANDROID)

namespace {
[[maybe_unused]] static bool CheckTagRegionParameters(void* ptr, size_t sz) {
  // Check that ptr and size are correct for MTE
  uintptr_t ptr_as_uint = reinterpret_cast<uintptr_t>(ptr);
  bool ret = (ptr_as_uint % kMemTagGranuleSize == 0) &&
             (sz % kMemTagGranuleSize == 0) && sz;
  return ret;
}

#if PA_CONFIG(HAS_MEMORY_TAGGING)
static bool HasCPUMemoryTaggingExtension() {
  return base::CPU::GetInstanceNoAllocation().has_mte();
}
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)

#if PA_CONFIG(HAS_MEMORY_TAGGING)
void* TagRegionRandomlyForMTE(void* ptr, size_t sz, uint64_t mask) {
  // Randomly tag a region (MTE-enabled systems only). The first 16-byte
  // granule is randomly tagged, all other granules in the region are
  // then assigned that initial tag via __arm_mte_set_tag.
  if (!CheckTagRegionParameters(ptr, sz)) {
    return nullptr;
  }
  // __arm_mte_create_random_tag generates a randomly tagged pointer via the
  // hardware's random number generator, but does not apply it to the memory.
  char* nptr = reinterpret_cast<char*>(__arm_mte_create_random_tag(ptr, mask));
  for (size_t i = 0; i < sz; i += kMemTagGranuleSize) {
    // Next, tag the first and all subsequent granules with the randomly tag.
    __arm_mte_set_tag(nptr +
                      i);  // Tag is taken from the top bits of the argument.
  }
  return nptr;
}

void* TagRegionIncrementForMTE(void* ptr, size_t sz) {
  // Increment a region's tag (MTE-enabled systems only), using the tag of the
  // first granule.
  if (!CheckTagRegionParameters(ptr, sz)) {
    return nullptr;
  }
  // Increment ptr's tag.
  char* nptr = reinterpret_cast<char*>(__arm_mte_increment_tag(ptr, 1u));
  for (size_t i = 0; i < sz; i += kMemTagGranuleSize) {
    // Apply the tag to the first granule, and all subsequent granules.
    __arm_mte_set_tag(nptr + i);
  }
  return nptr;
}

void* RemaskVoidPtrForMTE(void* ptr) {
  if (PA_LIKELY(ptr)) {
    // Can't look up the tag for a null ptr (segfaults).
    return __arm_mte_get_tag(ptr);
  }
  return nullptr;
}
#endif

void* TagRegionIncrementNoOp(void* ptr, size_t sz) {
  // Region parameters are checked even on non-MTE systems to check the
  // intrinsics are used correctly.
  return ptr;
}

void* TagRegionRandomlyNoOp(void* ptr, size_t sz, uint64_t mask) {
  // Verifies a 16-byte aligned tagging granule, size tagging granule (all
  // architectures).
  return ptr;
}

void* RemaskVoidPtrNoOp(void* ptr) {
  return ptr;
}

}  // namespace

void InitializeMTESupportIfNeeded() {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  if (HasCPUMemoryTaggingExtension()) {
    global_remask_void_ptr_fn = RemaskVoidPtrForMTE;
    global_tag_memory_range_increment_fn = TagRegionIncrementForMTE;
    global_tag_memory_range_randomly_fn = TagRegionRandomlyForMTE;
  }
#endif
}

RemaskPtrInternalFn* global_remask_void_ptr_fn = RemaskVoidPtrNoOp;
TagMemoryRangeIncrementInternalFn* global_tag_memory_range_increment_fn =
    TagRegionIncrementNoOp;
TagMemoryRangeRandomlyInternalFn* global_tag_memory_range_randomly_fn =
    TagRegionRandomlyNoOp;

TagViolationReportingMode GetMemoryTaggingModeForCurrentThread() {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  base::CPU cpu;
  if (!cpu.has_mte()) {
    return TagViolationReportingMode::kUndefined;
  }
  int status = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
  PA_CHECK(status >= 0);
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_SYNC)) {
    return TagViolationReportingMode::kSynchronous;
  }
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_ASYNC)) {
    return TagViolationReportingMode::kAsynchronous;
  }
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)
  return TagViolationReportingMode::kUndefined;
}

}  // namespace internal

}  // namespace partition_alloc
