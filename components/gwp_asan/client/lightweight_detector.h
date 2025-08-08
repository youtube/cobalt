// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_H_

#include <atomic>
#include <memory>

#include "base/gtest_prod_util.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/common/lightweight_detector_state.h"

namespace gwp_asan::internal {

class GWP_ASAN_EXPORT LightweightDetector {
 public:
  LightweightDetector(LightweightDetectorMode, size_t num_metadata);
  ~LightweightDetector();

  LightweightDetector(const LightweightDetector&) = delete;
  LightweightDetector& operator=(const LightweightDetector&) = delete;

  // Records the deallocation stack trace and overwrites the allocation with a
  // pattern that allows the crash handler to recover the trace ID.
  void RecordLightweightDeallocation(void* ptr, size_t size);

  // Retrieves the textual address of the shared state required by the
  // crash handler.
  std::string GetCrashKey() const;

  // Returns internal memory used by the detector (required for sanitization
  // on supported platforms.)
  std::vector<std::pair<void*, size_t>> GetInternalMemoryRegions();

  bool HasAllocationForTesting(uintptr_t);

 private:
  // The state shared with with the crash analyzer.
  LightweightDetectorState state_;
  // Array of metadata (e.g. stack traces) for allocations.
  std::unique_ptr<LightweightDetectorState::SlotMetadata[]> metadata_;

  std::atomic<LightweightDetectorState::MetadataId> next_metadata_id_{0};

  FRIEND_TEST_ALL_PREFIXES(LightweightDetectorTest, PoisonAlloc);
  FRIEND_TEST_ALL_PREFIXES(LightweightDetectorTest, SlotReuse);
  FRIEND_TEST_ALL_PREFIXES(LightweightDetectorAnalyzerTest, InternalError);
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_H_
